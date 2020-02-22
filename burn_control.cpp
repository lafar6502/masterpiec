#include <arduino.h>
#include <assert.h>
#include "burn_control.h"
#include "boiler_control.h"
#include "hwsetup.h"
#include "piec_sensors.h"

void initializeBurningLoop() {
  g_TargetTemp = g_CurrentConfig.TCO;
  g_HomeThermostatOn = true;
  forceState(STATE_P0);
  Serial.println("Burn init");
}




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
bool g_Alert = false;
char* g_AlertReason;





void processSensorValues() {
  g_TempCO = getLastDallasValue(TSENS_BOILER);
  g_TempCWU = getLastDallasValue(TSENS_CWU);
  g_TempPowrot = getLastDallasValue(TSENS_RETURN);
  g_TempFeeder = getLastDallasValue(TSENS_FEEDER);
  g_TempZewn = getLastDallasValue(TSENS_EXTERNAL);
  g_TempSpaliny = getLastThermocoupleValue(T2SENS_EXHAUST);
  g_TempBurner = getLastThermocoupleValue(T2SENS_BURNER);
  if (g_CurrentConfig.HomeThermostat) 
  {
    g_HomeThermostatOn = digitalRead(HW_THERMOSTAT_PIN) != LOW;
  }
  
  
}


void burningProc() 
{
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
  if (g_BurnState != BURN_STATES[g_BurnState].State) {
    Serial.print("invalid burn st");
    Serial.println(g_BurnState);
  }

  
  //1. check if we should change state
  for(int i=0; i < N_BURN_TRANSITIONS; i++)
  {
    if (BURN_TRANSITIONS[i].From == g_BurnState) 
    {
      if (BURN_TRANSITIONS[i].fCondition != NULL && BURN_TRANSITIONS[i].fCondition()) 
      {
        Serial.print("BS: trans ");
        Serial.print(i);
        Serial.print(" ->");
        Serial.println(BURN_STATES[BURN_TRANSITIONS[i].To].Code);
        if (BURN_TRANSITIONS[i].fAction != NULL) BURN_TRANSITIONS[i].fAction();
        //transition to new state
        g_BurnState = BURN_TRANSITIONS[i].To;
        assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
        g_CurStateStart = millis();
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

//api - switch to state
void forceState(TSTATE st) {
  assert(st != STATE_UNDEFINED);
  if (st == g_BurnState) return;
  g_BurnState = st;
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  g_CurStateStartTempCO = g_TempCO;
  if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(g_BurnState);
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
void workStateInitialize(TSTATE t) {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP && g_BurnState < N_BURN_STATES);
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
  Serial.print("Burn init, cycle: ");
  Serial.println(g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec);
}

//przejscie do stanu recznego
void stopStateInitialize(TSTATE t) {
  assert(g_BurnState == STATE_STOP);
  setBlowerPower(0);
  setFeederOff();
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  Serial.println("Stop init");
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
  if (tNow >= g_CurBurnCycleStart + burnCycleLen) 
  {
    g_CurBurnCycleStart = millis(); //
    setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
    Serial.print("r1:");
    Serial.println(g_CurBurnCycleStart);
  }
}

unsigned long _reductionStateEndMs = 0;

void reductionStateInit(TSTATE prev) {
  assert(g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2);
  assert(prev == STATE_P1 || prev == STATE_P2);
  if (prev != STATE_P1 && prev != STATE_P2) {
    Serial.print("red: wrong state");
    Serial.println(prev);
    prev = STATE_P1;
  }
  
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  _reductionStateEndMs = g_CurStateStart + (unsigned long) g_CurrentConfig.BurnConfigs[prev].CycleSec * 1000L;
  setBlowerPower(g_CurrentConfig.BurnConfigs[prev].BlowerPower, g_CurrentConfig.BurnConfigs[prev].BlowerCycle == 0 ? g_CurrentConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[prev].BlowerCycle);
  setFeederOff();
  Serial.print("red: cycle should end in ");
  Serial.println((_reductionStateEndMs - g_CurStateStart) / 1000.0);
}

void reductionStateLoop() {
  assert(g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2);
  TSTATE prevState = g_BurnState == STATE_REDUCE1 ? STATE_P1 : STATE_P2;
  unsigned long tNow = millis();
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[prevState].CycleSec * 1000L;
  
  if (tNow > _reductionStateEndMs + 100) 
  {
    Serial.print("reduction should end by now:");
    Serial.println(g_CurBurnCycleStart);
  }
}



void podtrzymanieStateInitialize(TSTATE t) {
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  setBlowerPower(0);
  setFeederOff();
  Serial.print("podtrz init. C:");
  Serial.println(g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec);
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
    Serial.print("rP:");
    Serial.print(burnCycleLen);
    Serial.print(", ");
    Serial.print(g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec);
    Serial.print(", s:");
    Serial.println(g_CurBurnCycleStart);
  }
}

void manualStateLoop() {
  
}


bool cond_shouldHeatCWU1() {
  //check if cwu1 heating is needed
  if (!isPumpEnabled(PUMP_CWU1)) return false;
  if (g_TempCWU < g_CurrentConfig.TCWU - g_CurrentConfig.THistCwu) return true;
  return false;
}

///kiedy potrzebujemy grzać grzejniki - tzn kiedy chcemy pompować wodę 
bool cond_shouldHeatHome() {
  if (getManualControlMode()) return false;
  if (g_CurrentConfig.HomeThermostat) {
    return g_HomeThermostatOn;
  }
  return true;
}

bool cond_needsCooling() {
  return g_TempCO > 90;
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
//
// which pumps and when
// cwu heating needed -> turn on cwu pump if current temp is above min pump temp and above cwu temp + delta
//
void updatePumpStatus() {
  if (getManualControlMode()) return;
  if (cond_shouldHeatCWU1()) {
    uint8_t minTemp = max(g_CurrentConfig.TMinPomp, g_TempCWU + g_CurrentConfig.TDeltaCWU);
    if (g_TempCO >= minTemp) {
      setPumpOn(PUMP_CWU1);
    } 
    else 
    {
      setPumpOff(PUMP_CWU1);
    }
  }
  else if (cond_shouldHeatHome()) {
    uint8_t minTemp = g_CurrentConfig.TMinPomp;
    if (g_TempCO > minTemp) {
      setPumpOn(PUMP_CO1);
    } 
    else {
      setPumpOff(PUMP_CO1);
    }
  }
  else if (cond_needsCooling()) {
    setPumpOn(PUMP_CO1);
    if (isPumpEnabled(PUMP_CWU1)) setPumpOn(PUMP_CWU1);
  }
  else 
  {
    setPumpOff(PUMP_CWU1);
    setPumpOff(PUMP_CO1); 
  }
}





bool isAlarm_NoHeating() {
  //only for automatic heating cycles P1 P2
  return false;
}

bool cond_feederOnFire() {
  //is feeder on fire?
  return false;
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
  return g_TempCO >= g_TargetTemp + g_CurrentConfig.TDeltaCO;
}

//heating is needed and temp is below the target
bool cond_needHeatingAndBelowTargetTemp() {
  if (g_TempCO >= g_TargetTemp) return false;
  return cond_shouldHeatCWU1() || cond_shouldHeatHome();
}

bool cond_targetTempReachedAndHeatingNotNeeded() {
  if (g_TempCO < g_TargetTemp) return false;
  return !cond_shouldHeatHome() && !cond_shouldHeatCWU1();
}

const TBurnTransition  BURN_TRANSITIONS[]   = 
{
  {STATE_P0, STATE_ALARM, NULL, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_STOP, STATE_ALARM, NULL, NULL},
  
  {STATE_P1, STATE_P2, cond_belowHysteresis, NULL},
  {STATE_P1, STATE_REDUCE1, cond_boilerOverheated, NULL}, //P1 -> P0
  {STATE_P1, STATE_REDUCE1, cond_targetTempReachedAndHeatingNotNeeded, NULL}, //P1 -> P0

  {STATE_P0, STATE_P2, cond_belowHysteresis, NULL},
  {STATE_P0, STATE_P1, cond_needHeatingAndBelowTargetTemp, NULL},

  {STATE_P2, STATE_REDUCE1, cond_boilerOverheated, NULL},  //P2 -> P0
  {STATE_P2, STATE_REDUCE2, cond_targetTempReached, NULL}, //10 P2 -> P1
  
  {STATE_REDUCE2, STATE_P2, cond_belowHysteresis, NULL},
  {STATE_REDUCE2, STATE_P2, cond_needHeatingAndBelowTargetTemp, NULL},
  {STATE_REDUCE2, STATE_P1, cond_cycleEnded, NULL},
  
  {STATE_REDUCE1, STATE_P2, cond_belowHysteresis, NULL}, //juz nie redukujemy
  {STATE_REDUCE1, STATE_P0, cond_cycleEnded, NULL},
  {STATE_REDUCE1, STATE_P1, cond_needHeatingAndBelowTargetTemp, NULL}, //juz nie redukujemy  - np sytuacja się zmieniła i temp. została podniesiona
  
  {STATE_UNDEFINED, STATE_UNDEFINED, NULL, NULL} //sentinel
};



const TBurnStateConfig BURN_STATES[]  = {
  {STATE_P0, 'P', podtrzymanieStateInitialize, podtrzymanieStateLoop},
  {STATE_P1, '1', workStateInitialize, workStateBurnLoop},
  {STATE_P2, '2', workStateInitialize, workStateBurnLoop},
  {STATE_STOP, 'S', stopStateInitialize, manualStateLoop},
  {STATE_ALARM, 'A', NULL, NULL},
  {STATE_REDUCE1, 'R', reductionStateInit, reductionStateLoop},
  {STATE_REDUCE2, 'r', reductionStateInit, reductionStateLoop},
};

const uint8_t N_BURN_TRANSITIONS = sizeof(BURN_TRANSITIONS) / sizeof(TBurnTransition);
const uint8_t N_BURN_STATES = sizeof(BURN_STATES) / sizeof(TBurnStateConfig);
