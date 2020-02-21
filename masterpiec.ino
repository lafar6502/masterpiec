#include "masterpiec.h"
#include <assert.h>
#include <MD_DS1307.h>
#include "ui_handler.h"
#include "piec_sensors.h"
#include "boiler_control.h"

/**
 * struktura programu
 * - sterowanie silnikiem dmuchawy - na przerwaniu 0
 * - pętla sterowania grzaniem (tj załączanie dmuchawy i podajnika w odpowiednim reżimie) - na przerwaniu zegarowym, aktualizacja statusu co 1 sek. Pętla sterowania grzaniem obsługuje też wyłączanie i załączanie pomp.
 * - obsługa UI. Aktualizacja ekranu w głównym loop-ie, natomiast obsługa inputu - z przerwań. Nie powinno być wyswietlania czegokolwiek w przerwaniu.
 * - zewnętrzny termostat - podpięty do jednego z pinów. Odczytywany tryb (grzej / nie grzej) i obsługiwany odpowiednio w pętli sterowania grzaniem
 * - odczyt czujników temperatury - też powinien następować w pętli, nie wiem czy tak często jak pętla sterowania grzaniem
 * 
 * 
 * pompy - działają niezależnie. Jeśli grzanie=ON to pompa CO załącza się gdy temp. jest powyżej minimalnej i wyłącza się gdy jest poniżej. Jeśli grzanie = off to pompa CO załącza się wyłącznie po to żeby pozbyć się nadmiaru ciepła, czyli gdy temp aktualna CO jest wyższa 
 * niż TZadana + delta. Pozbywanie się nadmiaru ciepła trwa do momentu gdy temp spadnie albo gdy przestanie rosnąć albo przez ustalony cykl. Możemy też pompę co skonfigurować do załączania się cyklicznie na x minut co y minut.
 * Pompa CWU - działa gdy temp pieca jest wyższa niż temp bojlera+delta CWU (czyli to jest nasza minimalna temp załączenia pompy). Załącza się gdy temp bojlera spadnie poniżej tCWUZadana - hist1. W momencie załączenia przestawiamy TZadana pieca na tZadanaCWU+delta CWU
 * pompa CWU wyłącza się gdy temp bojlera osiągnie docelową (wtedy tez przywracamy temp zadana pieca) albo gdy temp spadnie poniżej tBojler+delta cwu.
 * 
 */
void setup() {
  //initialize interrupts etc
  //initialize hardware
  Serial.begin(9600);
  if (!RTC.isRunning()) RTC.control(DS1307_CLOCK_HALT, DS1307_OFF);
  // put your setup code here, to run once:
  eepromRestoreConfig(0);
  initializeEncoder(HW_ENCODER_PINA, HW_ENCODER_PINB, HW_ENCODER_PINBTN);
  initializeDisplay();
  initializeDallasSensors();
  initializeMax6675Sensors();
  initializeBlowerControl();
  Serial.println("inited the hardware");
  
  initializeBurningLoop();
  changeUIState('0');
}

void loop() {
  uint64_t m = millis();
  RTC.readTime();
  refreshSensorReadings();
  processSensorValues();  //wciagamy odczyty sensorów do zmiennych programu
  //standardBurnLoop();     //procedura kontroli spalania
  updateView();           //aktualizacja ui
  int m2 = millis();
  int d = 100 - (m2 - m);
  if (d > 0) 
  {
    delay(d);
  }
  else 
  {
    ///Serial.print("zabrakło mi ms ");
    ///Serial.println(-d);  
  }
}


//czas wejscia w bieżący stan, ms
unsigned long g_CurStateStart = 0;
float  g_CurStateStartTempCO = 0; //temp pieca w momencie wejscia w bież. stan.
unsigned long g_CurBurnCycleStart = 0; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia
uint8_t  g_CurrentBlowerPower = 0;

TControlConfiguration g_CurrentConfig;

//to nasza procedura aktualizacji stanu hardware-u
//wolana cyklicznie.
void standardBurnLoop() {
  
  burningProc();
  updatePumpStatus();
  
}

void initializeBurningLoop() {
  forceState(STATE_STOP);
}

void burningProc() 
{
  assert(g_BurnState != STATE_UNDEFINED);
  //1. check if we should change state
  uint8_t i=0;
  while(BURN_TRANSITIONS[i].From != STATE_UNDEFINED)
  {
    if (BURN_TRANSITIONS[i].From == g_BurnState) 
    {
      if (BURN_TRANSITIONS[i].fCondition != NULL && BURN_TRANSITIONS[i].fCondition()) 
      {
        if (BURN_TRANSITIONS[i].fAction != NULL) BURN_TRANSITIONS[i].fAction();
        //transition to new state
        g_BurnState = BURN_TRANSITIONS[i].To;
        assert(g_BurnState != STATE_UNDEFINED);
        g_CurStateStart = millis();
        g_CurStateStartTempCO = g_TempCO;
        if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize();
        return;    
      }
    }
  }
  
  //2. obsługa bieżącego stanu
  BURN_STATES[g_BurnState].fLoop();
  
  
}

void forceState(TSTATE st) {
  assert(st != STATE_UNDEFINED);
  g_CurStateStart = millis();
  g_CurStateStartTempCO = g_TempCO;
  if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize();
}

static unsigned long burnCycleLen = 0;
static unsigned long burnFeedLen = 0;
static unsigned long burnCycleStart = 0;

void workStateInitialize() {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP);
  burnCycleLen = g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000;
  burnFeedLen = g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 * 100;
  burnCycleStart = g_CurStateStart;
  setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower);
}

///pętla palenia dla stanu pracy
//załączamy dmuchawę na ustaloną moc no i pilnujemy podajnika
void workStateBurnLoop() {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP);
  unsigned long tNow = millis();
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
  }
}

void podtrzymanieStateInitialize() {
  
}

void podtrzymanieStateLoop() {
  unsigned long tNow = millis();
  
}

void updatePumpStatus() {
  
}

void eepromRestoreConfig(uint8_t slot) {
  
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


const TBurnTransition  BURN_TRANSITIONS[]  = 
{
  {STATE_P0, STATE_ALARM, NULL, NULL},
  {STATE_P1, STATE_ALARM, NULL, NULL},
  {STATE_P2, STATE_ALARM, NULL, NULL},
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

const TBurnStateConfig BURN_STATES[] = {
  {STATE_UNDEFINED, '~', NULL, NULL},
  {STATE_P0, 'P', podtrzymanieStateInitialize, podtrzymanieStateLoop},
  {STATE_P1, '1', workStateInitialize, workStateBurnLoop},
  {STATE_P2, '2', workStateInitialize, workStateBurnLoop},
  {STATE_STOP, 'S', NULL, NULL},
  {STATE_ALARM, 'A', NULL, NULL},
  {STATE_UNDEFINED, ' ', NULL, NULL} //sentinel
};
