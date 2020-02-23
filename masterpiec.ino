#include "masterpiec.h"
#include <assert.h>
#include <MD_DS1307.h>
#include "ui_handler.h"
#include "burn_control.h"
#include "piec_sensors.h"
#include "boiler_control.h"
#include "script.h"

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
  
  // put your setup code here, to run once:
  eepromRestoreConfig(0);
  initializeEncoder();
  initializeDisplay();
  initializeDallasSensors();
  initializeMax6675Sensors();
  initializeBlowerControl();
  loggingInit();
  Serial.println("inited the hardware");
  updateDallasSensorAssignmentFromConfig();
  initializeBurningLoop();
#ifdef MPIEC_ENABLE_WEBSERVER
  setupWebServer();
#endif
#ifdef MPIEC_ENABLE_SCRIPT
  setupSerialShell();
#endif
  changeUIState('0');
}


void loop() {
  uint64_t m = millis();
  RTC.readTime();
  refreshSensorReadings();
  
  burnControlTask();     //procedura kontroli spalania
  updateView();           //aktualizacja ui
  periodicDumpControlState();
#ifdef MPIEC_ENABLE_WEBSERVER
    webHandlingTask();
#endif
#ifdef MPIEC_ENABLE_SCRIPT
    handleSerialShellTask();
#endif
  loggingTask();
  int m2 = millis();
  int d = 150 - (m2 - m);
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
    Serial.print(", dm:");
    Serial.print(getCurrentBlowerPower());
    Serial.print(" (");
    Serial.print(getCurrentBlowerCycle());
    Serial.print("), pd:");
    Serial.print(isFeederOn() ? "ON": "OFF");
    Serial.print(", co:");
    Serial.print(isPumpOn(PUMP_CO1));
    Serial.print(", cwu:");
    Serial.print(isPumpOn(PUMP_CWU1));
    Serial.print(", stime:");
    Serial.print((t - g_CurStateStart) / 1000);
    Serial.print(", btime:");
    Serial.print((t - g_CurBurnCycleStart) / 1000);
    Serial.print(", tspalin");
    Serial.print(g_TempSpaliny);
    Serial.print(", tpiec");
    Serial.print(g_TempCO);
    Serial.print(", need:");
    Serial.print(g_needHeat);
    Serial.println();
  }
}

void updateDallasSensorAssignmentFromConfig() {
  uint8_t zbuf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  char buf[20];
  
  for(uint8_t i=0; i<8; i++) {
    uint8_t*p = g_CurrentConfig.DallasAddress[i];
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
