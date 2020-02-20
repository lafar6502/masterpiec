#include "masterpiec.h"
#include <assert.h>
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
  // put your setup code here, to run once:
  eepromRestoreConfig(0);
  initializeEncoder(15, 18, 34);
  initializeDisplay();
  initializeDallasSensors();
  initializeMax6675Sensors();
  
  Serial.println("inited the encoder");
}

void loop() {
  uint64_t m = millis();
  refreshSensorReadings();
  processSensorValues();  //wciagamy odczyty sensorów do zmiennych programu
  standardBurnLoop();     //procedura kontroli spalania
  updateView();           //aktualizacja ui
  int m2 = millis();
  int d = 100 - (m2 - m);
  if (d > 0) 
  {
    delay(d);
  }
  else 
  {
    Serial.print("zabrakło mi ms ");
    Serial.println(-d);  
  }
}


//czas wejscia w bieżący stan, ms
unsigned long g_CurStateStart = 0;
uint16_t  g_CurStateStartC10 = 0; //temp pieca w momencie wejscia w bież. stan.
unsigned long g_CurBurnCycleStart = 0; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia
uint16_t g_BoilerC10; //aktualna temperatura pieca

TControlConfiguration g_CurrentConfig;

//to nasza procedura aktualizacji stanu hardware-u
//wolana cyklicznie.
void standardBurnLoop() {
  
  burningProc();
  updatePumpStatus();
  
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
      if (BURN_TRANSITIONS[i].fCondition()) 
      {
        //transition to new state
        g_BurnState = BURN_TRANSITIONS[i].To;
        assert(g_BurnState != STATE_UNDEFINED);
        g_CurStateStart = millis();
        g_CurStateStartC10 = g_BoilerC10;
        BURN_STATES[g_BurnState].fInitialize();
        return;    
      }
    }
  }
  
  //2. obsługa bieżącego stanu
  BURN_STATES[g_BurnState].fLoop();
  
  
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
    burnCycleStart = millis();
  }
}

void podtrzymanieStateInitialize() {
  
}

void podtrzymanieStateLoop() {
  unsigned long tNow = millis();
  
}

void eepromRestoreConfig(uint8_t slot) {
  
}

float g_AktTempZadana = 0; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
float g_TempCO;
float g_TempCWU; 
float g_TempPowrot;  //akt. temp. powrotu
float g_TempSpaliny; //akt. temp. spalin
float g_TempPodajnik;
TSTATE g_BurnState = STATE_UNDEFINED;  //aktualny stan grzania
bool   g_TermostatStop;  //true - termostat pokojowy kazał zaprzestać grzania
float g_TempZewn; //aktualna temp. zewn

void processSensorValues() {
  g_TempCO = getLastDallasValue(TSENS_BOILER);
  g_TempCWU = getLastDallasValue(TSENS_CWU);
  g_TempPowrot = getLastDallasValue(TSENS_RETURN);
  g_TempPodajnik = getLastDallasValue(TSENS_FEEDER);
  g_TempSpaliny = getLastThermocoupleValue(T2SENS_EXHAUST);
}


const TBurnTransition  BURN_TRANSITIONS[]  = 
{
  {STATE_STOP, STATE_P2, NULL, NULL},  
  {STATE_P2, STATE_P1, NULL, NULL},
  {STATE_P1, STATE_P2, NULL, NULL},
  {STATE_P1, STATE_P0, NULL, NULL},
  {STATE_P0, STATE_P2, NULL, NULL},
  {STATE_P2, STATE_P1, NULL, NULL},
  {STATE_P0, STATE_ALARM, NULL, NULL},
  {STATE_P1, STATE_ALARM, NULL, NULL},
  {STATE_P2, STATE_ALARM, NULL, NULL},
  {STATE_STOP, STATE_ALARM, NULL, NULL},

  
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
