#include "masterpiec.h"
#include <assert.h>
#include <MD_DS1307.h>
#include "ui_handler.h"
#include "burn_control.h"
#include "piec_sensors.h"
#include "boiler_control.h"

void updateDallasSensorAssignmentFromConfig();
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
  RTC.readTime();
  // put your setup code here, to run once:
  readInitialConfig();
  Serial.print("CFG size:");
  Serial.println(sizeof(TControlConfiguration));  
  if (sizeof(TControlConfiguration) > CFG_SLOT_SIZE) {
    exit(0);
  }
  initializeEncoder();
  initializeDisplay();
  g_CurrentUIView = 0;
  updateView();
  initializeDallasSensors();
  initializeMax6675Sensors();
  initializeBlowerControl();
  initializeFlowMeter();
  loggingInit();
  Serial.print("Hw inited. Time:");
  Serial.print(RTC.yyyy);
  Serial.print('-');
  Serial.print(RTC.mm);
  Serial.print('-');
  Serial.print(RTC.dd);
  Serial.print('T');
  Serial.print(RTC.h);
  Serial.print(':');
  Serial.println(RTC.m);
  
  updateDallasSensorAssignmentFromConfig();
  
  for(int i=0;i<3;i++) {
    delay(500);
    refreshSensorReadings();
    delay(500);
    processSensorValues();
  }
  initializeBurningLoop();
  changeUIState('0');
}


void loop() {
  uint64_t m = millis();
  void (*pf)(void*) = g_uiBottomHalf;
  if (pf != NULL) 
  {
    pf(g_uiBottomHalfCtx);
    if (pf == g_uiBottomHalf) 
    {
      g_uiBottomHalf = NULL;
      g_uiBottomHalfCtx = NULL;  
    }
  }
  RTC.readTime();
  refreshSensorReadings();
  burnControlTask();     
  circulationControlTask();
  gatherStatsTask();
  updateView();          
  periodicDumpControlState();
  

#ifdef MPIEC_ENABLE_SCRIPT
    handleSerialShellTask();
#endif
  loggingTask();
  commandHandlingTask();
  int m2 = millis();
  int d = 150 - (m2 - m);
  if (d > 0) 
  {
    delay(d);
  }
  else 
  {
    Serial.print(F("zabraklo mi ms "));
    Serial.println(-d);  
  }
}

extern uint32_t counter;

void periodicDumpControlState() {
  static unsigned long lastDump = 0;
  static TSTATE pstate = STATE_UNDEFINED;
  
  unsigned long t = millis();
  if (t - lastDump > 30000 || g_BurnState != pstate)
  {
    lastDump = t;
    pstate = g_BurnState;
    Serial.print(F("s:"));
    Serial.print(BURN_STATES[g_BurnState].Code);
    Serial.print(F(", dm:"));
    Serial.print(getCurrentBlowerPower());
    Serial.print(F(" ("));
    Serial.print(getCurrentBlowerCycle());
    Serial.print(F("), pd:"));
    Serial.print(isFeederOn() ? "ON": "OFF");
    Serial.print(F(", co:"));
    Serial.print(isPumpOn(PUMP_CO1));
    Serial.print(F(", cwu:"));
    Serial.print(isPumpOn(PUMP_CWU1));
    Serial.print(F(", stime:"));
    Serial.print((t - g_CurStateStart) / 1000);
    Serial.print(F(", btime:"));
    Serial.print((t - g_CurBurnCycleStart) / 1000);
    Serial.print(F(", tspalin:"));
    Serial.print(g_TempSpaliny);
    Serial.print(F(", tpiec:"));
    Serial.print(g_TempCO);
    Serial.print(F(", need:"));
    Serial.print(g_needHeat);
    Serial.print(F(", dT60:"));
    Serial.print(g_dT60);
    Serial.print(F(", dTl3:"));
    Serial.print(g_dTl3);
    Serial.print(F(", floV:"));
    Serial.print(g_AirFlow);
    Serial.print(F(":"));
    Serial.print(g_AirFlowNormal);
    Serial.print(F(", lastT:"));
    Serial.print(g_lastCOReads.IsEmpty() ? 0.0 : g_lastCOReads.GetLast()->Val);
	Serial.print(F(", CNT:"));
    Serial.print(counter);
	Serial.println();
  }
}

void updateDallasSensorAssignmentFromConfig() {
  uint8_t zbuf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  char buf[20];
  
  for(uint8_t i=0; i<8; i++) {
    uint8_t*p = g_DeviceConfig.DallasAddress[i];
    if (memcmp(p, zbuf, 8) == 0) continue;
    
    if (!ensureDallasSensorAtIndex(i, p)) {
      Serial.print(F("Invalid dallas address "));
      sprintf(buf, "%d: %02X%02X%02X%02X%02X%02X%02X%02X", i, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);     
      Serial.println(buf);    
    }
  }
  for(uint8_t i=0; i<8; i++) 
  {
    printDallasInfo(i, buf);
    Serial.print(F("czujnik "));
    Serial.print(i);
    Serial.print(" ");
    Serial.println(buf);
  }
}
