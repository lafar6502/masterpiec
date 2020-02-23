#include <arduino.h>
#include <assert.h>
#include "burn_control.h"
#include "boiler_control.h"
#include "hwsetup.h"
#include "piec_sensors.h"
#include "ui_handler.h"
#include "varholder.h"


#define MAX_TEMP 90

void initializeBurningLoop() {
  g_TargetTemp = g_CurrentConfig.TCO;
  g_HomeThermostatOn = true;
  forceState(STATE_P0);
}


typedef struct {
  unsigned long Ms;
  float Val;
} TReading;

float g_TargetTemp = 0.1; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
float g_CurrentHysteresis = 1.0;
float g_TempCO = 0.0;
float g_TempCWU = 0.0; 
float g_TempPowrot = 0.0;  //akt. temp. powrotu
float g_TempSpaliny = 0.0; //akt. temp. spalin
float g_TempFeeder = 0.1;
float g_TempBurner = 0;
TSTATE g_BurnState = STATE_UNDEFINED;  //aktualny stan grzania
bool   g_HomeThermostatOn = true;  //true - termostat pokojowy kazał zaprzestać grzania
float g_TempZewn = 0.0; //aktualna temp. zewn
char* g_Alarm;
unsigned long g_P1Time = 0;
unsigned long g_P2Time = 0;
TReading lastCOTemperatures[10];
CircularBuffer<TReading> g_lastCOReads(lastCOTemperatures, sizeof(lastCOTemperatures)/sizeof(TReading));


void setAlarm(const char* txt) {
  if (txt != NULL) g_Alarm = txt;
  forceState(STATE_ALARM);
  
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
  if (g_lastCOReads.IsEmpty() || ms >= (g_lastCOReads.GetLast()->Ms + 5000)) {
    g_lastCOReads.Enqueue({ms, g_TempCO});
  }
}

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
        Serial.print(F("BS: trans "));
        Serial.print(i);
        Serial.print(" ->");
        Serial.println(BURN_STATES[BURN_TRANSITIONS[i].To].Code);
        
        if (g_BurnState == STATE_P1)
          g_P1Time += (t - g_CurStateStart);
        else if (g_BurnState == STATE_P2)
          g_P2Time += (t - g_CurStateStart);
          
        if (BURN_TRANSITIONS[i].fAction != NULL) BURN_TRANSITIONS[i].fAction(i);
        //transition to new state
        g_BurnState = BURN_TRANSITIONS[i].To;
        assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
        g_CurStateStart = t;
        g_CurBurnCycleStart = g_CurStateStart;
        g_CurStateStartTempCO = g_TempCO;
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
    if (g_BurnState == STATE_STOP) {
      forceState(STATE_P0);
    }
  }
  else {
    if (g_BurnState != STATE_STOP) {
      forceState(STATE_STOP);
    }
  }
}

bool getManualControlMode()
{
  return g_BurnState == STATE_STOP;
}



//czas wejscia w bieżący stan, ms
unsigned long g_CurStateStart = 0;
float  g_CurStateStartTempCO = 0; //temp pieca w momencie wejscia w bież. stan.
unsigned long g_CurBurnCycleStart = 0; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia (ten podajnik+nadmuch)
float curStateMaxTempCO = 0;

//api - switch to state
void forceState(TSTATE st) {
  assert(st != STATE_UNDEFINED);
  if (st == g_BurnState) return;
  unsigned long t = millis();

  if (g_BurnState == STATE_P1)
    g_P1Time += (t - g_CurStateStart);
  else if (g_BurnState == STATE_P2)
    g_P2Time += (t - g_CurStateStart);
  TSTATE old = g_BurnState;
  g_BurnState = st;
  g_CurStateStart = t;
  g_CurBurnCycleStart = g_CurStateStart;
  g_CurStateStartTempCO = g_TempCO;
  if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(old);
  Serial.print("BS->");
  Serial.println(BURN_STATES[g_BurnState].Code);
}

void updatePumpStatus();
void adjustTargetTemperature();

//API 
//to nasza procedura aktualizacji stanu hardware-u
//wolana cyklicznie.
void burnControlTask() {
  
  processSensorValues();
  adjustTargetTemperature();
  updatePumpStatus();
  burningProc();
}




//inicjalizacja dla stanu grzania autom. P1 P2
void workStateInitialize(TSTATE prev) {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP && g_BurnState < N_BURN_STATES);
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  g_CurStateStartTempCO = g_TempCO;
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
  g_CurBurnCycleStart = g_CurStateStart;
}

///pętla palenia dla stanu pracy
//załączamy dmuchawę na ustaloną moc no i pilnujemy podajnika
void workStateBurnLoop() {
  assert(g_BurnState == STATE_P1 || g_BurnState == STATE_P2);
  unsigned long tNow = millis();
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000L;
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 * 100L;
  
  if (tNow < g_CurBurnCycleStart + burnFeedLen) 
  {
    if (!isFeederOn()) setFeederOn();
  }
  else 
  {
    if (isFeederOn()) setFeederOff();
  }
  if (g_TempCO > curStateMaxTempCO) curStateMaxTempCO = g_TempCO;
  if (tNow >= g_CurBurnCycleStart + burnCycleLen) 
  {
    g_CurBurnCycleStart = millis(); //
    setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
    Serial.print("r1:");
    Serial.println(g_CurBurnCycleStart);
  }
}

unsigned long _reductionStateEndMs = 0; //inside reduction state - this is the calculated end time. Outside (before reduction) - we put remaining P1 or P2 time there before going to reduction.

void reductionStateInit(TSTATE prev) {
  assert(g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2);
  assert(prev == STATE_P1 || prev == STATE_P2);
  if (prev != STATE_P1 && prev != STATE_P2) {
    Serial.print(F("red: wrong state"));
    Serial.println(prev);
    prev = STATE_P1;
  }
  
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  unsigned long adj = _reductionStateEndMs;
  _reductionStateEndMs = g_CurStateStart + (unsigned long) g_CurrentConfig.BurnConfigs[prev].CycleSec * 1000L + adj;
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
  setBlowerPower(0);
  setFeederOff();
  //Serial.print("podtrz init. C:");
  //Serial.println(g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec);
}

void podtrzymanieStateLoop() {
  assert(g_BurnState == STATE_P0);
  unsigned long tNow = millis();
  static uint8_t cycleNum = 0;
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec * 1000L;
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[STATE_P0].FuelSecT10 * 100L;
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
    if (!isFeederOn()) setFeederOn();
  } 
  else 
  {
    if (isFeederOn()) setFeederOff();
  }
  
  if (tNow >= g_CurBurnCycleStart + burnCycleLen) 
  {
    g_CurBurnCycleStart = tNow;
    cycleNum++;
    /*Serial.print("rP:");
    Serial.print(burnCycleLen);
    Serial.print(", ");
    Serial.print(g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec);
    Serial.print(", s:");
    Serial.println(g_CurBurnCycleStart);*/
  }
}

void manualStateLoop() {
  
}


bool cond_shouldHeatCWU1() {
  //check if cwu1 heating is needed
  if (!isPumpEnabled(PUMP_CWU1)) return false;
  if (!isDallasEnabled(TSENS_CWU)) return false;
  if (g_TempCWU < g_CurrentConfig.TCWU - g_CurrentConfig.THistCwu) return true;
  return false;
}

///kiedy potrzebujemy grzać grzejniki - tzn kiedy jest zapotrzebowanie na grzanie w domowej instalacji, niezaleznie od akt. stanu kotła.
bool cond_shouldHeatHome() {
  if (getManualControlMode()) return false;
  if (g_CurrentConfig.SummerMode) return false;
  if (g_CurrentConfig.EnableThermostat) return g_HomeThermostatOn;
  return true;
}



void adjustTargetTemperature() {
  
  g_TargetTemp = g_CurrentConfig.TCO;
  g_CurrentHysteresis = g_CurrentConfig.THistCO;
  if (cond_shouldHeatCWU1()) {
     g_TargetTemp = max(g_CurrentConfig.TCO, g_CurrentConfig.TCWU + g_CurrentConfig.TDeltaCWU);
     g_CurrentHysteresis = min(g_CurrentConfig.THistCO, g_CurrentConfig.TDeltaCWU);
  }
  else if (cond_shouldHeatHome()) {
    //nothing...
  }
}

bool cond_needCooling(); //below
//
// which pumps and when
// cwu heating needed -> turn on cwu pump if current temp is above min pump temp and above cwu temp + delta
//
void updatePumpStatus() {
  if (g_TempCO >= MAX_TEMP) {
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
  if (cond_shouldHeatCWU1()) {
    uint8_t minTemp = max(g_CurrentConfig.TMinPomp, g_TempCWU + g_CurrentConfig.TDeltaCWU);
    if (g_TempCO >= minTemp) {
      setPumpOn(PUMP_CWU1);
    } 
    else {
      setPumpOff(PUMP_CWU1);
      //Serial.println(F("too low to heat cwu"));
    }
  }
  else if (cond_shouldHeatHome()) {
    setPumpOn(PUMP_CO1);
  }
  else if (cond_needCooling()) {
    if (g_CurrentConfig.SummerMode && isPumpEnabled(PUMP_CWU1)) {
      setPumpOn(PUMP_CWU1);
    }
    else {
      setPumpOn(PUMP_CO1);
    }
  }
  else 
  {
    setPumpOff(PUMP_CWU1);
    setPumpOff(PUMP_CO1); 
  }
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
  if (g_CurrentConfig.FeederTempLimit > 0 && g_TempFeeder > g_CurrentConfig.FeederTempLimit) {
    g_Alarm = "Temp. podajnika";
    return true;
  }
  return false;
}

bool isAlarm_NoHeating() {
  //only for automatic heating cycles P1 P2
  if (g_BurnState != STATE_P1 && g_BurnState != STATE_P2) return false;
  unsigned long m = millis();
  m = m - g_CurStateStart;
  if (g_CurrentConfig.NoHeatAlarmTimeM > 0 && m > 60 * 1000L * g_CurrentConfig.NoHeatAlarmTimeM && curStateMaxTempCO - g_CurStateStartTempCO < 2.0) 
  {
    g_Alarm = "Wygaslo";
    return true;
  }
  return false;
}

bool isAlarm_Any() {
  return isAlarm_HardwareProblem() || isAlarm_Overheat() || isAlarm_NoHeating();
}

bool cond_feederOnFire() {
  //is feeder on fire?
  return false;
}

void alarmStateInitialize(TSTATE prev) {
  changeUIState('0');
}

// kiedy przechodzimy z P0 do P2
// gdy temp. wody CO spadnie poniżej zadanej
// 


bool cond_belowHysteresis() {
  return  g_TempCO < g_TargetTemp - g_CurrentHysteresis;
}

bool cond_cycleEnded() {
  return _reductionStateEndMs <= millis();
}

bool cond_targetTempReached() {
  return g_TempCO >= g_TargetTemp;
}

bool cond_boilerOverheated() {
  return g_TempCO >= g_TargetTemp + g_CurrentConfig.TDeltaCO || g_TempCO >= MAX_TEMP;
}

uint8_t _coolState = 0; //1-cool, 2-pause
unsigned long _coolTs = 0;

//temp too high, need cooling by running co or cwu pump
bool cond_needCooling() {
  if (g_TempCO >= MAX_TEMP) {_coolState = 0; return true;} //always
  bool hot = (g_TempCO >= g_TargetTemp + g_CurrentConfig.TDeltaCO);
  if (!getManualControlMode() && g_BurnState != STATE_ALARM && g_CurrentConfig.CooloffTimeM10 != 0 && hot) 
  {
    unsigned long t = millis();
    switch(_coolState) {
      case 1:
        if ((t - _coolTs) > (unsigned long) g_CurrentConfig.CooloffTimeM10 * 60 * 1000) {//pause
          _coolState = 2;
          _coolTs = t;
          Serial.println(F("Cool pause"));
          return false;
        }
        return true;
      case 2:
        if ((t - _coolTs) > (unsigned long) g_CurrentConfig.CooloffPauseM10 * 60 * 1000) {//pause
          _coolState = 1;
          _coolTs = t;
          Serial.println(F("Cool resume"));
          return true;
        }
        return false;
        break;
      default:
        _coolState = 1;
        _coolTs = t;
        Serial.println(F("Cool start"));
        return true;
    }
  }
  else 
  {
    _coolState = 0;
    return false;  
  }
}


uint8_t needHeatingNow() {
  if (cond_shouldHeatCWU1()) return 2;
  if (cond_shouldHeatHome()) return 1;
  return 0;
}
//heating is needed and temp is below the target
bool cond_needHeatingAndBelowTargetTemp() {
  if (g_TempCO >= g_TargetTemp) return false;
  bool r2 = cond_shouldHeatCWU1() || cond_shouldHeatHome();
  return r2;
}
//to samo co wyzej, ale musialem dodac troche histerezy bo wpadamy w petle
bool cond_needHeatingAndBelowTargetTemp_unreduce() {
  if (g_TempCO >= g_TargetTemp - 1.1) return false;
  bool r2 = cond_shouldHeatCWU1() || cond_shouldHeatHome();
  return r2;
}





bool cond_targetTempReachedAndHeatingNotNeeded() {
  if (g_TempCO < g_TargetTemp) return false;
  return !cond_shouldHeatHome() && !cond_shouldHeatCWU1();
}

void onSwitchToReduction(int trans) {
  assert(g_BurnState == STATE_P1 || g_BurnState == STATE_P2);
  unsigned long t = millis();
  _reductionStateEndMs = 0;
  if (t < g_CurBurnCycleStart) return;
  unsigned long diff = t - g_CurBurnCycleStart;
  unsigned long fuelTimeMs =  g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 * 10000L; //feeder works at the start of P1 or P2 cycle
  unsigned long cycleLen = g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000L;
  _reductionStateEndMs = cycleLen - diff;
  if (diff < fuelTimeMs) //during feed
  {
    _reductionStateEndMs = _reductionStateEndMs * ((float) diff / fuelTimeMs);
  }
  Serial.print(F("Remaining time for reduction (ms):"));
  Serial.println(_reductionStateEndMs);
}

const TBurnTransition  BURN_TRANSITIONS[]   = 
{
  {STATE_P0, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_REDUCE1, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_REDUCE2, STATE_ALARM, isAlarm_Any, NULL},
  
  
  {STATE_STOP, STATE_ALARM, NULL, NULL},
  
  {STATE_P1, STATE_P2, cond_belowHysteresis, NULL},
  {STATE_P1, STATE_REDUCE1, cond_boilerOverheated, onSwitchToReduction}, //P1 -> P0
  {STATE_P1, STATE_REDUCE1, cond_targetTempReachedAndHeatingNotNeeded, onSwitchToReduction}, //10 P1 -> P0

  {STATE_P0, STATE_P2, cond_belowHysteresis, NULL},
  {STATE_P0, STATE_P1, cond_needHeatingAndBelowTargetTemp, NULL},

  {STATE_P2, STATE_REDUCE1, cond_boilerOverheated, onSwitchToReduction},  //P2 -> P0
  {STATE_P2, STATE_REDUCE2, cond_targetTempReached, onSwitchToReduction}, //10 P2 -> P1
  
  {STATE_REDUCE2, STATE_P2, cond_belowHysteresis, NULL},
  {STATE_REDUCE2, STATE_P2, cond_needHeatingAndBelowTargetTemp_unreduce, NULL},
  {STATE_REDUCE2, STATE_P1, cond_cycleEnded, NULL},
  
  {STATE_REDUCE1, STATE_P2, cond_belowHysteresis, NULL}, //juz nie redukujemy
  {STATE_REDUCE1, STATE_P0, cond_cycleEnded, NULL},
  {STATE_REDUCE1, STATE_P1, cond_needHeatingAndBelowTargetTemp_unreduce, NULL}, //juz nie redukujemy  - np sytuacja się zmieniła i temp. została podniesiona. uwaga - ten sam war. co w #10 - cykl
  
  {STATE_UNDEFINED, STATE_UNDEFINED, NULL, NULL} //sentinel
};



const TBurnStateConfig BURN_STATES[]  = {
  {STATE_P0, 'P', podtrzymanieStateInitialize, podtrzymanieStateLoop},
  {STATE_P1, '1', workStateInitialize, workStateBurnLoop},
  {STATE_P2, '2', workStateInitialize, workStateBurnLoop},
  {STATE_STOP, 'S', stopStateInitialize, manualStateLoop},
  {STATE_ALARM, 'A', alarmStateInitialize, NULL},
  {STATE_REDUCE1, 'R', reductionStateInit, reductionStateLoop},
  {STATE_REDUCE2, 'r', reductionStateInit, reductionStateLoop},
};

const uint8_t N_BURN_TRANSITIONS = sizeof(BURN_TRANSITIONS) / sizeof(TBurnTransition);
const uint8_t N_BURN_STATES = sizeof(BURN_STATES) / sizeof(TBurnStateConfig);
