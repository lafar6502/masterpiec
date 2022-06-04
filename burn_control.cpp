#include <arduino.h>
#include <assert.h>
#include "burn_control.h"
#include "boiler_control.h"
#include "hwsetup.h"
#include "piec_sensors.h"
#include "ui_handler.h"
#include "varholder.h"
#include <MD_DS1307.h>

#define MAX_TEMP 90


void initializeBurningLoop() {
  g_TargetTemp = g_CurrentConfig.TCO;
  g_HomeThermostatOn = true;
  forceState(STATE_P0);
}


float g_TargetTemp = 0.1; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
float g_TempCO = 0.0;
float g_TempCWU = 0.0; 
float g_TempPowrot = 0.0;  //akt. temp. powrotu
float g_TempSpaliny = 0.0; //akt. temp. spalin
float g_TempFeeder = 0.1;
float g_TempBurner = 0;
TSTATE g_BurnState = STATE_UNDEFINED;  //aktualny stan grzania
TSTATE g_ManualState = STATE_UNDEFINED; //wymuszony ręcznie stan (STATE_UNDEFINED: brak wymuszenia)
CWSTATE g_CWState = CWSTATE_OK; //current cw status
HEATNEED g_needHeat = NEED_HEAT_NONE; //0, 1 or 2
HEATNEED g_initialNeedHeat = NEED_HEAT_NONE; //heat needs at the beginning of current state

bool   g_HomeThermostatOn = true;  //true - termostat pokojowy kazał zaprzestać grzania
float g_TempZewn = 0.0; //aktualna temp. zewn
char* g_Alarm;
unsigned long g_P1Time = 0;
unsigned long g_P2Time = 0;
unsigned long g_P0Time = 0;

TReading lastCOTemperatures[11];
TIntReading lastBurnStates[11];
CircularBuffer<TReading> g_lastCOReads(lastCOTemperatures, sizeof(lastCOTemperatures)/sizeof(TReading));
CircularBuffer<TIntReading> g_lastBurnTransitions(lastBurnStates, sizeof(lastBurnStates)/sizeof(TIntReading));

//czas wejscia w bieżący stan, ms
unsigned long g_CurStateStart = 0;
//float  g_CurStateStartTempCO = 0; //temp pieca w momencie wejscia w bież. stan.
uint8_t g_BurnCyclesBelowMinTemp = 0; //number of burn cycles with g_TempCO below minimum pump temperature (for detecting extinction of fire)
unsigned long g_CurBurnCycleStart = 0; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia (ten podajnik+nadmuch)


void setAlarm(const char* txt) {
  if (txt != NULL) g_Alarm = txt;
  forceState(STATE_ALARM);
  
}

float g_dT60; //1-minute temp delta
float g_dTl3; //last 3 readings diff

float interpolate(const TReading& r1, const TReading& r2, unsigned long t0) {
  float tdT = r2.Val - r1.Val;
  unsigned long tm0 = r2.Ms - r1.Ms;
  //printf("interpolate (%ld,%f), (%ld, %f) at %ld: dt is %ld\r\n", r1.Ms, r1.Val, r2.Ms, r2.Val, t0, tm0);
  if (tm0==0) tm0 = 1;
  float tx = r1.Val + tdT * (t0 - r1.Ms) / tm0; // (tdT / tm0) * ((t0 - r1.Ms) / tm0);
  return tx;
}

float calcDT60() {
    unsigned long m  = millis();
    unsigned long m2 = m - 60 * 1000;
    TReading r2 {m, g_TempCO};
    int i;
    for (i = g_lastCOReads.GetCount() - 1; i >= 0; i--) {
        if (g_lastCOReads.GetAt(i)->Ms < m2) break;
    }
    if (i >= 0) {
        //printf("found reading at %d ms=%d, v=%f\r\n", i, g_reads.GetAt(i)->Ms, g_reads.GetAt(i)->Val);
        const TReading& r1 = *g_lastCOReads.GetAt(i);
        if (i < g_lastCOReads.GetCount() - 1) {
          r2 = *g_lastCOReads.GetAt(i + 1); //next read
        }
        float newPoint = interpolate(r1, r2, m2);
        float dx = (g_TempCO - newPoint);// * 1000 / (m - m2);
        //now interpolate btw r and r2
        return dx;
    }
    else {
        //printf("did not find reading");
        return 0;
    }
}

void processSensorValues() {
  g_TempCO = getLastDallasValue(TSENS_BOILER);
  g_TempCWU = getLastDallasValue(TSENS_CWU);
  g_TempPowrot = getLastDallasValue(TSENS_RETURN);
  g_TempFeeder = getLastDallasValue(TSENS_FEEDER);
  g_TempZewn = getLastDallasValue(TSENS_EXTERNAL);
  g_TempSpaliny = getLastThermocoupleValue(T2SENS_EXHAUST);
  g_TempBurner = getLastThermocoupleValue(T2SENS_BURNER);
  if (g_CurrentConfig.EnableThermostat) 
  {
    g_HomeThermostatOn = isThermostatOn();
  }
  unsigned long ms = millis();
  if (g_lastCOReads.IsEmpty() || abs(g_lastCOReads.GetLast()->Val - g_TempCO) >= 0.5 || g_lastCOReads.GetLast()->Ms < (ms - 120000L)) 
  {
    g_lastCOReads.Enqueue({ms, g_TempCO});
  }
  g_dT60 = calcDT60();
  TReading* pr = g_lastCOReads.GetCount() >= 4 ? g_lastCOReads.GetAt(-3) : NULL; //discard the first read
  if (g_lastCOReads.GetFirst()->Ms > (ms - 15000L)) pr = NULL;
  g_dTl3 = pr != NULL ? (g_TempCO - pr->Val) * 60.0 * 1000.0 / (ms - pr->Ms) : 0.0;
}


void circulationControlTask() {
  if (!isPumpEnabled(PUMP_CIRC)) return;
  
  
  if (g_CurrentConfig.CircCycleMin == 0 || g_CurrentConfig.CircWorkTimeS == 0) return;
  if (getManualControlMode()) return;
  
  uint16_t cmin = RTC.h * 60 + RTC.m;
  cmin = cmin % g_CurrentConfig.CircCycleMin;
  uint16_t secs = cmin * 60 + RTC.s;
  bool pumpOn = secs < g_CurrentConfig.CircWorkTimeS;
  bool zoneIn = false;
  
  if ((RTC.h >= 17 && RTC.h <= 21))
        zoneIn = true;
  else if ((RTC.h >= 6 && RTC.h <= 7))
        zoneIn = true;
  else if ((RTC.h >= 11 && RTC.h <= 12))
        zoneIn = true;
  
  pumpOn = zoneIn && pumpOn;
        
  if (pumpOn) {
    if (!isPumpOn(PUMP_CIRC)) Serial.println("Circ pump start");
    setPumpOn(PUMP_CIRC);
  } else {
    if (isPumpOn(PUMP_CIRC)) Serial.println("Circ pump stop");
    setPumpOff(PUMP_CIRC);
  }
};
/***
 * ---
 */
void burningProc() 
{
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
  if (g_BurnState != BURN_STATES[g_BurnState].State) {
    Serial.print(F("invalid burn st"));
    Serial.println(g_BurnState);
  }
  unsigned long t = millis();
  
  //1. check if we should change state
  for(int i=0; i < N_BURN_TRANSITIONS; i++)
  {
    if (BURN_TRANSITIONS[i].From == g_BurnState) 
    {
      if (BURN_TRANSITIONS[i].fCondition != NULL && BURN_TRANSITIONS[i].fCondition()) 
      {
        if (g_BurnState == STATE_P1 || g_BurnState == STATE_REDUCE1)
          g_P1Time += (t - g_CurStateStart);
        else if (g_BurnState == STATE_P2  || g_BurnState == STATE_REDUCE2)
          g_P2Time += (t - g_CurStateStart);
        else if (g_BurnState == STATE_P0) 
          g_P0Time += (t - g_CurStateStart);
          
        g_lastBurnTransitions.Enqueue({t - g_CurStateStart, i});
        Serial.print(F("BS: trans "));
        Serial.print(i);
        if (g_BurnState == STATE_P1) {
          Serial.print(" tP1:");
          Serial.print(g_P1Time);
        } else if (g_BurnState == STATE_P2) {
          Serial.print(" tP2:");
          Serial.print(g_P2Time);
        }
        Serial.print(" ");
        Serial.print(BURN_STATES[BURN_TRANSITIONS[i].From].Code);
        Serial.print("->");
        Serial.println(BURN_STATES[BURN_TRANSITIONS[i].To].Code);
        if (BURN_TRANSITIONS[i].fAction != NULL) BURN_TRANSITIONS[i].fAction(i);
        
        //transition to new state
        g_BurnState = BURN_TRANSITIONS[i].To;
        assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
        g_CurStateStart = t;
        g_initialNeedHeat = g_needHeat;
        g_CurBurnCycleStart = g_CurStateStart;
        g_BurnCyclesBelowMinTemp = 0;
        if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(BURN_TRANSITIONS[i].From);
        return;    
      }
    }
  }

  if (BURN_STATES[g_BurnState].fLoop != NULL) {
    BURN_STATES[g_BurnState].fLoop();  
  }
}


void setManualControlMode(bool b)
{
  if (!b) {
	g_ManualState = STATE_UNDEFINED;
    if (g_BurnState == STATE_STOP) {
      forceState(STATE_P0);
    }
  }
  else {
	  g_ManualState = STATE_STOP;
	if (g_BurnState != STATE_STOP) {
	  forceState(STATE_STOP);
	}
  }
}

bool getManualControlMode()
{
	return g_ManualState != STATE_UNDEFINED;
}



float curStateMaxTempCO = 0;

//api - switch to state
void forceState(TSTATE st) {
  assert(st != STATE_UNDEFINED);
  if (st == g_BurnState) return;
  unsigned long t = millis();

  if (g_BurnState == STATE_P1 || g_BurnState == STATE_REDUCE1)
    g_P1Time += (t - g_CurStateStart);
  else if (g_BurnState == STATE_P2  || g_BurnState == STATE_REDUCE2)
    g_P2Time += (t - g_CurStateStart);
  else if (g_BurnState == STATE_P0) 
    g_P0Time += (t - g_CurStateStart);
    
  g_lastBurnTransitions.Enqueue({t - g_CurStateStart, -g_BurnState});
  TSTATE old = g_BurnState;
  g_BurnState = st;
  g_CurStateStart = t;
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  g_BurnCyclesBelowMinTemp = 0;
  if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(old);
  Serial.print("BS->");
  Serial.print(BURN_STATES[g_BurnState].Code);
  Serial.print(" tP1:");
  Serial.print(g_P1Time);
  Serial.print(" tP2:");
  Serial.println(g_P2Time);
}

void updatePumpStatus();
void handleHeatNeedStatus();

//API 
//to nasza procedura aktualizacji stanu hardware-u
//wolana cyklicznie.
void burnControlTask() {
  
  processSensorValues();
  handleHeatNeedStatus();
  updatePumpStatus();
  burningProc();
}




//inicjalizacja dla stanu grzania autom. P1 P2
void workStateInitialize(TSTATE prev) {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP && g_BurnState < N_BURN_STATES);
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  g_BurnCyclesBelowMinTemp = 0;
  curStateMaxTempCO = g_TempCO;
  setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
  Serial.print(F("Burn init, cycle: "));
  Serial.println(g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec);
}

//przejscie do stanu recznego
void stopStateInitialize(TSTATE prev) {
  assert(g_BurnState == STATE_STOP);
  setBlowerPower(0);
  setFeederOff();
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
}

///pętla palenia dla stanu pracy
//załączamy dmuchawę na ustaloną moc no i pilnujemy podajnika
void workStateBurnLoop() {
  assert(g_BurnState == STATE_P1 || g_BurnState == STATE_P2);
  unsigned long tNow = millis();
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000L;
  //feeder time length
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 * (100L + g_CurrentConfig.FuelCorrection);
  
  if (tNow < g_CurBurnCycleStart + burnFeedLen) 
  {
    setFeederOn();
  }
  else 
  {
    setFeederOff();
  }
  if (g_TempCO > curStateMaxTempCO) curStateMaxTempCO = g_TempCO;
  if (tNow >= g_CurBurnCycleStart + burnCycleLen) 
  {
    g_CurBurnCycleStart = millis(); //next burn cycle
    setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
    g_BurnCyclesBelowMinTemp = g_TempCO <= g_CurrentConfig.TMinPomp ? g_BurnCyclesBelowMinTemp + 1 : 0;
  }
}

unsigned long _reductionStateEndMs = 0; //inside reduction state - this is the calculated end time. Outside (before reduction) - we put remaining P1 or P2 time there before going to reduction.

void firestartStateInit(TSTATE prev) {
	assert(g_BurnState == STATE_FIRESTART);
	g_CurStateStart = millis();
	g_initialNeedHeat = g_needHeat;
	g_CurBurnCycleStart = g_CurStateStart;  
}

void firestartStateLoop() {
	
}

void offStateInit(TSTATE prev) {
  assert(g_BurnState == STATE_OFF);
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;  
}

void offStateLoop() {
  
}


void reductionStateInit(TSTATE prev) {
  assert(g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2);
  assert(prev == STATE_P1 || prev == STATE_P2);
  if (prev != STATE_P1 && prev != STATE_P2) {
    Serial.print(F("red: wrong state"));
    Serial.println(prev);
    prev = STATE_P1;
  }
  
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  unsigned long adj = _reductionStateEndMs; //remaining time from P2 or P1
  
  _reductionStateEndMs = (unsigned long) g_CurrentConfig.BurnConfigs[prev].CycleSec * 1000L;
  if (prev == STATE_P2) adj += ((unsigned long) g_CurrentConfig.ReductionP2ExtraTime * (unsigned long) g_CurrentConfig.BurnConfigs[prev].CycleSec * 10L); // * 1000 / 100;
  _reductionStateEndMs = g_CurStateStart + _reductionStateEndMs + adj; //
  setBlowerPower(g_CurrentConfig.BurnConfigs[prev].BlowerPower, g_CurrentConfig.BurnConfigs[prev].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[prev].BlowerCycle);
  setFeederOff();
  Serial.print(F("red: cycle should end in "));
  Serial.print((_reductionStateEndMs - g_CurStateStart) / 1000.0);
  Serial.print(F(", extra time s:"));
  Serial.println(adj / 1000);
}

void reductionStateLoop() {
  assert(g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2);
  TSTATE prevState = g_BurnState == STATE_REDUCE1 ? STATE_P1 : STATE_P2;
  unsigned long tNow = millis();
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[prevState].CycleSec * 1000L;
  
  if (tNow > _reductionStateEndMs + 100) 
  {
    Serial.print(F("reduction should end by now:"));
    Serial.println(g_CurBurnCycleStart);
  }
}



void podtrzymanieStateInitialize(TSTATE prev) {
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  g_initialNeedHeat = g_needHeat;
  setBlowerPower(0);
  setFeederOff();
}

void podtrzymanieStateLoop() {
  assert(g_BurnState == STATE_P0);
  unsigned long tNow = millis();
  static uint8_t cycleNum = 0;
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec * 1000L;
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[STATE_P0].FuelSecT10 * (100L + g_CurrentConfig.FuelCorrection);
  unsigned long blowerStart = burnCycleLen - (unsigned long) g_CurrentConfig.P0BlowerTime * 1000L;
  
  if (tNow >= g_CurBurnCycleStart + blowerStart) 
  {
    setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
  } 
  else 
  {
    setBlowerPower(0);
  }

  if (tNow >= g_CurBurnCycleStart + blowerStart && tNow <= g_CurBurnCycleStart + blowerStart + burnFeedLen && (cycleNum % g_CurrentConfig.P0FuelFreq) == 0) 
  {
    setFeederOn();
  } 
  else 
  {
    setFeederOff();
  }
  
  if (tNow >= g_CurBurnCycleStart + burnCycleLen) 
  {
    g_CurBurnCycleStart = tNow;
    cycleNum++;
  }
}

void manualStateLoop() {
  
}


void handleHeatNeedStatus() {

  if (g_CWState == CWSTATE_OK) {
      if (g_TempCWU < g_CurrentConfig.TCWU - g_CurrentConfig.THistCwu) {
        g_CWState = CWSTATE_HEAT; //start heating cwu
        g_TargetTemp = max(g_CurrentConfig.TCO, g_CurrentConfig.TCWU + g_CurrentConfig.TDeltaCWU);
        Serial.print(F("CWU heat - adjusted target temp to "));
        Serial.println(g_TargetTemp);
      } else g_TargetTemp = g_CurrentConfig.TCO;
  }
  else if (g_CWState == CWSTATE_HEAT) {
    if (g_TempCWU >= g_CurrentConfig.TCWU) {
      g_CWState = CWSTATE_OK;
      g_TargetTemp = g_CurrentConfig.TCO;
      Serial.print(F("CWU ready - adjusted target temp to "));
      Serial.println(g_TargetTemp);
    }
  }
  else assert(false);

  HEATNEED prev = g_needHeat;
  g_needHeat = NEED_HEAT_NONE;
  if (g_CWState == CWSTATE_HEAT) g_needHeat = NEED_HEAT_CWU;
  if (!g_CurrentConfig.SummerMode && g_needHeat == NEED_HEAT_NONE)
  {
    if (!g_CurrentConfig.EnableThermostat || g_HomeThermostatOn) g_needHeat = NEED_HEAT_CO;
  }
  if (g_needHeat != prev) {
    Serial.print(F("Heat needs changed:"));
    Serial.println(g_needHeat);
  }
}

uint8_t cond_needCooling(); //below
bool cond_willFallBelowHysteresisSoon();
//
// which pumps and when
// cwu heating needed -> turn on cwu pump if current temp is above min pump temp and above cwu temp + delta
//
void updatePumpStatus() {
  if (g_TempCO >= MAX_TEMP) { //this should be the only time two pumps are allowed to work together
    if (isPumpEnabled(PUMP_CWU1)) setPumpOn(PUMP_CWU1);
    setPumpOn(PUMP_CO1);
    return;
  }
  if (getManualControlMode()) return; //all below only in automatic mode.
  if (g_TempCO < g_CurrentConfig.TMinPomp) {
    setPumpOff(PUMP_CO1);
    setPumpOff(PUMP_CWU1);
    return;
  }
  
  if (g_needHeat == NEED_HEAT_CWU) {
    uint8_t minTemp = max(g_CurrentConfig.TMinPomp, g_TempCWU + g_CurrentConfig.TDeltaCWU);
    if (g_TempCO >= minTemp) {
      setPumpOn(PUMP_CWU1);
      setPumpOff(PUMP_CO1);
    } 
    else {
      setPumpOff(PUMP_CWU1);
      //Serial.println(F("too low to heat cwu"));
    }
    return;
  }
  if (g_needHeat == NEED_HEAT_CO && !cond_willFallBelowHysteresisSoon()) { //co pump on - thermostat on or thermostat disabled (co pump always on)
    setPumpOn(PUMP_CO1);
    setPumpOff(PUMP_CWU1); //just to be sure
    return;
  }
  
  uint8_t cl = cond_needCooling();
  if (cl != 0) {
    bool cw = false;
    if (g_CurrentConfig.SummerMode && isPumpEnabled(PUMP_CWU1) && isDallasEnabled(TSENS_CWU)) {
       cw = true;
    }
    if (cl == 2) {
      cw = true;
    }
    setPumpOn(cw ? PUMP_CWU1 : PUMP_CO1);
    setPumpOff(cw ? PUMP_CO1 : PUMP_CWU1);
    return;
  }
  setPumpOff(PUMP_CWU1);
  setPumpOff(PUMP_CO1);
}


bool isAlarm_HardwareProblem() {
  if (!isDallasEnabled(TSENS_BOILER)) {
    g_Alarm = "Czujnik temp CO";
    return true;
  }
  if (g_CurrentConfig.FeederTempLimit != 0 && !isDallasEnabled(TSENS_FEEDER)) {
    g_Alarm = "Czujnik podajnika";
    return true;
  }
  return false;
}

bool isAlarm_Overheat() {
  if (g_TempCO > MAX_TEMP) {
    g_Alarm = "ZA GORACO";
    return true;
  }
  return false;
}

bool isAlarm_feederOnFire() {
  if (g_CurrentConfig.FeederTempLimit > 0 && g_TempFeeder > g_CurrentConfig.FeederTempLimit) {
    g_Alarm = "Temp. podajnika";
    return true;
  }
  return false;
}

bool isAlarm_NoHeating() {
  //only for automatic heating cycles P1 P2
  if (g_BurnState != STATE_P1 && g_BurnState != STATE_P2) return false;
  if (g_CurrentConfig.NoHeatAlarmCycles == 0) return false; // no detection
  if (g_TempCO > g_CurrentConfig.TMinPomp) return false; //if above the min temp we dont detect 'fire extinct'
  if (g_BurnCyclesBelowMinTemp > g_CurrentConfig.NoHeatAlarmCycles)
  {
    g_Alarm = "Wygaslo";
    return true;
  }
  return false;
}

bool isAlarm_Any() {
  return isAlarm_HardwareProblem() || isAlarm_Overheat() || isAlarm_NoHeating() || isAlarm_feederOnFire();
}



void alarmStateInitialize(TSTATE prev) {
  changeUIState('0');
}

// kiedy przechodzimy z P0 do P2
// gdy temp. wody CO spadnie poniżej zadanej
// 



bool cond_B_belowHysteresisAndNeedHeat() {
  if (g_needHeat != NEED_HEAT_NONE && g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return true;
  if (g_needHeat == NEED_HEAT_CWU) {
    //we're below min temp to heat the cwu
    return (g_TempCO < g_TempCWU + g_CurrentConfig.TDeltaCWU);
  }
  return false;  
}


//variant #2 of control 
bool cond_C_belowHysteresisAndNoNeedToHeat() {
  if (g_needHeat == NEED_HEAT_NONE and g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return true;
  return false;
}

bool cond_D_belowTargetTempAndNeedHeat() {
  if (g_needHeat != NEED_HEAT_NONE && g_TempCO < g_TargetTemp - 0.5) return true;
  return false;
}

bool cond_cycleEnded() {
  return _reductionStateEndMs <= millis();
}

bool cond_targetTempReached() {
  return g_TempCO >= g_TargetTemp;
}

bool cond_willReachTargetSoon() {
  if (g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return false;
  if (g_dTl3 < (g_needHeat == NEED_HEAT_NONE ? 0.2 : 0.5)) return false;
  if (g_TempCO + 2.5 * g_dTl3 > g_TargetTemp) return true; //we will be there in max 2.5 minutes
  return false;
}
//we're above hysteresis but the need for heat has suddenly disappeared (thermostat?) - so reduce from P2
bool cond_suddenHeatOffAndAboveHysteresis() {
  if (g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return false;
  if (g_needHeat == NEED_HEAT_NONE && g_initialNeedHeat != NEED_HEAT_NONE) return true;
  return false;
}

//we dont need heating and temperature is above the hysteresis low limit
bool cond_noNeedToHeatAndAboveHysteresis() {
  if (g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return false;
  if (g_needHeat == NEED_HEAT_NONE) return true;
  return false;
}

bool cond_willFallBelowHysteresisSoon() {
  if (g_needHeat == NEED_HEAT_NONE) return false;
  if (g_dTl3 > -0.5) return false;
  if (g_TempCO + 2 * g_dTl3 < g_TargetTemp - g_CurrentConfig.THistCO) return true;
  return false;
}

bool cond_boilerOverheated() {
  return g_TempCO >= g_TargetTemp + g_CurrentConfig.TDeltaCO || g_TempCO >= MAX_TEMP;
}

uint8_t _coolState = 0; //1-cool, 2-pause
unsigned long _coolTs = 0;

bool cond_canCoolWithCWU() {
  if (!isPumpEnabled(PUMP_CWU1)) return false;
  if (g_TempCWU  <= g_TempCWU + g_CurrentConfig.TDeltaCWU) return false;
  if (g_TempCWU >= g_CurrentConfig.TCWU + 2 * g_CurrentConfig.THistCwu) return false; //cwu too hot
  return true;
}
//temp too high, need cooling by running co or cwu pump
//0 = no need to cool, 1 - should cool, 2 - should cool, possibly with CWU
uint8_t cond_needCooling() {
  static uint8_t _cwCnt = 0;
  if (g_TempCO >= MAX_TEMP) {_coolState = 0; return true;} //always
  bool hot = g_CurrentConfig.CooloffMode == COOLOFF_NONE ? false : g_CurrentConfig.CooloffMode == COOLOFF_LOWER ? (g_TempCO > g_TargetTemp + (_coolState == 1 ? 0.1 : 0.4)) : (g_TempCO > g_TargetTemp + g_CurrentConfig.TDeltaCO);
  if (getManualControlMode() || !hot || g_BurnState == STATE_ALARM) {
    _coolState = 0;
    return 0;
  }
  
  unsigned long t = millis();
  bool cw = (_cwCnt % 2) == 0 && cond_canCoolWithCWU();
  switch(_coolState) {
    case 1:
      if ((t - _coolTs) > (unsigned long) g_CurrentConfig.CooloffTimeM10 * 6L * 1000) {//pause
        _coolState = 2;
        _coolTs = t;
        _cwCnt++;
        Serial.println(F("Cool pause"));
        return 0;
      }
      return cw ? 2 : 1;
    case 2:
      if ((t - _coolTs) > (unsigned long) g_CurrentConfig.CooloffPauseM10 * 6L * 1000) {//pause
        _coolState = 1;
        _coolTs = t;
        Serial.println(F("Cool resume"));
        return cw ? 2 : 1;
      }
      return 0;
      break;
    default:
      _coolState = 1;
      _coolTs = t;
      Serial.println(F("Cool start"));
      return cw ? 2 : 1;
  }
}


uint8_t needHeatingNow() {
  return g_needHeat;
}
//heating is needed and temp is below the target
bool cond_A_needSuddenHeatAndBelowTargetTemp() {
  if (g_TempCO >= g_TargetTemp) return false;
  return g_needHeat != NEED_HEAT_NONE && g_initialNeedHeat == NEED_HEAT_NONE;
}

void alertStateLoop() {
  static unsigned long _feederStart = 0;
  unsigned long t = millis();
  if (isAlarm_feederOnFire()) {
   
  }
}


bool cond_targetTempReachedAndHeatingNotNeeded() {
  return g_TempCO >= g_TargetTemp && g_needHeat == NEED_HEAT_NONE;
}

void onSwitchToReduction(int trans) {
  assert(g_BurnState == STATE_P1 || g_BurnState == STATE_P2);
  unsigned long t = millis();
  _reductionStateEndMs = 0;
  if (t < g_CurBurnCycleStart) return;
  unsigned long diff = t - g_CurBurnCycleStart;
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 *  (100L + g_CurrentConfig.FuelCorrection);
  unsigned long cycleLen = g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000L;
  _reductionStateEndMs = cycleLen - diff;
  if (diff < burnFeedLen) //during feed
  {
    _reductionStateEndMs = _reductionStateEndMs * ((float) diff / burnFeedLen); //reduce the time because not all fuel was added
  }
  Serial.print(F("Remaining time for reduction (ms):"));
  Serial.println(_reductionStateEndMs);
}

uint8_t g_ReductionsToP0 = 0; //reductions P1 -> P0 or P2 -> P0 which we dont ave 
uint8_t g_ReductionsToP1 = 0; //reductions P2 -> P1

void onReductionCycleEnded(int trans) {
  const TBurnTransition &t = BURN_TRANSITIONS[trans];
  if (t.To == STATE_P0) {
    g_ReductionsToP0++;
  }
  else if (t.To == STATE_P1) {
    g_ReductionsToP1++;
  }
}


#define CONTROL_VARIANT 2

const TBurnTransition  BURN_TRANSITIONS[]   = 
{

  //{STATE_P0, STATE_P1, cond_C_belowHysteresisAndNoNeedToHeat, NULL}, //#v2  
  {STATE_P0, STATE_P2, cond_C_belowHysteresisAndNoNeedToHeat, NULL}, //#v3: switch to P2 to quickly start burning, then reduce to P1 if no heat needed and temp above hysteresis
  {STATE_P0, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL}, //this fires only if heat needed bc cond_C is earlier
  {STATE_P0, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL},
  {STATE_P0, STATE_P2, cond_willFallBelowHysteresisSoon, NULL}, //temp is dropping fast, we need heat -> P2
  {STATE_P0, STATE_P1, cond_D_belowTargetTempAndNeedHeat, NULL},
  
  {STATE_P1, STATE_REDUCE1, cond_boilerOverheated, onSwitchToReduction}, //E. P1 -> P0
  {STATE_P1, STATE_REDUCE1, cond_targetTempReachedAndHeatingNotNeeded, onSwitchToReduction}, //F. P1 -> P0
  {STATE_P1, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL},
  {STATE_P1, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL},
  
  
  {STATE_REDUCE1, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL}, //juz nie redukujemy  - np sytuacja się zmieniła i temp. została podniesiona. uwaga - ten sam war. co w #10 - cykl
  {STATE_REDUCE1, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL},
  {STATE_REDUCE1, STATE_P1, cond_C_belowHysteresisAndNoNeedToHeat, NULL}, //#v2
  {STATE_REDUCE1, STATE_P1, cond_D_belowTargetTempAndNeedHeat, NULL},
  {STATE_REDUCE1, STATE_P0, cond_cycleEnded, onReductionCycleEnded},

  
  {STATE_P2, STATE_REDUCE2, cond_targetTempReached, onSwitchToReduction}, //10 P2 -> P1
  {STATE_P2, STATE_REDUCE2, cond_willReachTargetSoon, onSwitchToReduction}, //10 P2 -> P1
  //{STATE_P2, STATE_REDUCE2, cond_suddenHeatOffAndAboveHysteresis, onSwitchToReduction}, //v2 10 P2 -> P1
  {STATE_P2, STATE_REDUCE2,   cond_noNeedToHeatAndAboveHysteresis, onSwitchToReduction}, //v3 10 P2 -> P1

  
  
  {STATE_REDUCE2, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL},
  {STATE_REDUCE2, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL},
  {STATE_REDUCE2, STATE_P1, cond_cycleEnded, onReductionCycleEnded},
  
  
  {STATE_P0, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_REDUCE1, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_REDUCE2, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_STOP, STATE_ALARM, NULL, NULL},
  {STATE_FIRESTART, STATE_ALARM, NULL, NULL},
  {STATE_FIRESTART, STATE_P2, NULL, NULL}, 
  {STATE_FIRESTART, STATE_P1, NULL, NULL},
  {STATE_FIRESTART, STATE_ALARM, NULL, NULL}, //failed to start fire
  {STATE_OFF, STATE_ALARM, NULL, NULL},
  {STATE_UNDEFINED, STATE_UNDEFINED, NULL, NULL} //sentinel
};



const TBurnStateConfig BURN_STATES[]  = {
  {STATE_P0, 'P', podtrzymanieStateInitialize, podtrzymanieStateLoop},
  {STATE_P1, '1', workStateInitialize, workStateBurnLoop},
  {STATE_P2, '2', workStateInitialize, workStateBurnLoop},
  {STATE_STOP, 'S', stopStateInitialize, manualStateLoop},
  {STATE_ALARM, 'A', alarmStateInitialize, alertStateLoop},
  {STATE_REDUCE1, 'R', reductionStateInit, reductionStateLoop},
  {STATE_REDUCE2, 'r', reductionStateInit, reductionStateLoop},
  {STATE_FIRESTART, 'B', firestartStateInit, firestartStateLoop},
  {STATE_OFF, '0', offStateInit, offStateLoop},
};

const uint8_t N_BURN_TRANSITIONS = sizeof(BURN_TRANSITIONS) / sizeof(TBurnTransition);
const uint8_t N_BURN_STATES = sizeof(BURN_STATES) / sizeof(TBurnStateConfig);
