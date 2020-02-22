#include <arduino.h>
#include <assert.h>
#include "burn_control.h"
#include "boiler_control.h"
#include "hwsetup.h"
#include "piec_sensors.h"

void initializeBurningLoop() {
  g_AktTempZadana = g_CurrentConfig.TCO;
  g_HomeThermostatOn = true;
  forceState(STATE_STOP);
}




float g_AktTempZadana = 0.1; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
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
    Serial.print("inv burn st");
    Serial.println(g_BurnState);
  }
  //1. check if we should change state
  for(int i=0; i < N_BURN_TRANSITIONS; i++)
  {
    if (BURN_TRANSITIONS[i].From == g_BurnState) 
    {
      if (BURN_TRANSITIONS[i].fCondition != NULL && BURN_TRANSITIONS[i].fCondition()) 
      {
        if (BURN_TRANSITIONS[i].fAction != NULL) BURN_TRANSITIONS[i].fAction();
        //transition to new state
        g_BurnState = BURN_TRANSITIONS[i].To;
        assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
        g_CurStateStart = millis();
        g_CurStateStartTempCO = g_TempCO;
        if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(g_BurnState);
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
unsigned long g_CurBurnCycleStart = 0; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia

//api - switch to state
void forceState(TSTATE st) {
  assert(st != STATE_UNDEFINED);
  g_BurnState = st;
  g_CurStateStart = millis();
  g_CurStateStartTempCO = g_TempCO;
  if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(g_BurnState);
  Serial.print("BS->");
  Serial.println(BURN_STATES[g_BurnState].Code);
}

void updatePumpStatus();

//API 
//to nasza procedura aktualizacji stanu hardware-u
//wolana cyklicznie.
void burnControlTask() {
  
  processSensorValues();
  burningProc();
  updatePumpStatus();
  
}



static unsigned long burnCycleLen = 0;
static unsigned long burnFeedLen = 0;
static unsigned long burnCycleStart = 0;

//inicjalizacja dla stanu grzania autom. P1 P2
void workStateInitialize(TSTATE t) {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP && g_BurnState < N_BURN_STATES);
  burnCycleLen = g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000;
  burnFeedLen = g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 * 100;
  burnCycleStart = g_CurStateStart;
  setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower);
}

//przejscie do stanu recznego
void stopStateInitialize(TSTATE t) {
  assert(g_BurnState == STATE_STOP);
  setBlowerPower(0);
  setFeederOff();
}

///pętla palenia dla stanu pracy
//załączamy dmuchawę na ustaloną moc no i pilnujemy podajnika
void workStateBurnLoop() {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP && g_BurnState < N_BURN_STATES);
  unsigned long tNow = millis();
  static unsigned long tPrev = 0;
  if (tNow < burnCycleStart + burnFeedLen) 
  {
    if (!isFeederOn()) setFeederOn();
  }
  else 
  {
    if (isFeederOn()) setFeederOff();
  }
  if (tNow >= burnCycleStart + burnCycleLen) {
    burnCycleStart = millis(); //
    setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower);
  }
  tPrev = tNow;
}

void podtrzymanieStateInitialize(TSTATE t) {
  
}

void podtrzymanieStateLoop() {
  unsigned long tNow = millis();
  static unsigned long tPrev = 0;
  
  
  tPrev = tNow;
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
}

const TBurnTransition  BURN_TRANSITIONS[]   = 
{
  {STATE_P0, STATE_ALARM, NULL, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_STOP, STATE_ALARM, NULL, NULL},
  
  {STATE_STOP, STATE_P2, NULL, NULL},  
  {STATE_P2, STATE_P1, NULL, NULL},
  {STATE_P1, STATE_P2, NULL, NULL},
  {STATE_P1, STATE_P0, NULL, NULL},
  {STATE_P0, STATE_P2, NULL, NULL},
  {STATE_P2, STATE_P1, NULL, NULL},

  {STATE_P2, STATE_REDUCE2, NULL, NULL}, //P2 -> P1
  {STATE_P1, STATE_REDUCE1, NULL, NULL}, //P1 -> P0
  {STATE_REDUCE2, STATE_P1, NULL, NULL},
  {STATE_REDUCE1, STATE_P0, NULL, NULL},
  {STATE_REDUCE2, STATE_P2, NULL, NULL},
  {STATE_REDUCE1, STATE_P1, NULL, NULL}, //juz nie redukujemy  - np sytuacja się zmieniła i temp. została podniesiona
  {STATE_REDUCE1, STATE_P2, NULL, NULL}, //juz nie redukujemy
  {STATE_UNDEFINED, STATE_UNDEFINED, NULL, NULL} //sentinel
};



const TBurnStateConfig BURN_STATES[]  = {
  {STATE_P0, 'P', podtrzymanieStateInitialize, podtrzymanieStateLoop},
  {STATE_P1, '1', workStateInitialize, workStateBurnLoop},
  {STATE_P2, '2', workStateInitialize, workStateBurnLoop},
  {STATE_STOP, 'S', stopStateInitialize, manualStateLoop},
  {STATE_ALARM, 'A', NULL, NULL},
  {STATE_REDUCE1, 'R', NULL, NULL},
  {STATE_REDUCE2, 'r', NULL, NULL},
};

const uint8_t N_BURN_TRANSITIONS = sizeof(BURN_TRANSITIONS) / sizeof(TBurnTransition);
const uint8_t N_BURN_STATES = sizeof(BURN_STATES) / sizeof(TBurnStateConfig);
