#include <arduino.h>
#include <assert.h>
#include "burn_control.h"
#include "boiler_control.h"
#include "hwsetup.h"
#include "piec_sensors.h"

void initializeBurningLoop() {
  forceState(STATE_STOP);
}




float g_AktTempZadana = 0.1; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
float g_TempCO = 0.1;
float g_TempCWU = 0.0; 
float g_TempPowrot = 0.1;  //akt. temp. powrotu
float g_TempSpaliny = 0.1; //akt. temp. spalin
float g_TempPodajnik = 0.1;
TSTATE g_BurnState = STATE_UNDEFINED;  //aktualny stan grzania
bool   g_TermostatStop = false;  //true - termostat pokojowy kazał zaprzestać grzania
float g_TempZewn = 0.0; //aktualna temp. zewn
bool g_Alert = false;
char* g_AlertReason;





void processSensorValues() {
  g_TempCO = getLastDallasValue(TSENS_BOILER);
  g_TempCWU = getLastDallasValue(TSENS_CWU);
  g_TempPowrot = getLastDallasValue(TSENS_RETURN);
  g_TempPodajnik = getLastDallasValue(TSENS_FEEDER);
  g_TempZewn = getLastDallasValue(TSENS_EXTERNAL);
  g_TempSpaliny = getLastThermocoupleValue(T2SENS_EXHAUST);
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

void updatePumpStatus() {
  if (getManualControlMode()) return;
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
