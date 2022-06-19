#include <arduino.h>

#define ENABLE_SD 1
#include "global_variables.h"
#include "masterpiec.h"
#include "boiler_control.h"
#include <EEPROM.h>
#include <MD_DS1307.h>
#include "ui_handler.h"
#include <SPI.h>
#ifdef ENABLE_SD
#include <SD.h>
#endif

#define MAX_CFG_SLOTS 4
#define AFTER_CONFIG_STORAGE (MAX_CFG_SLOTS * CFG_SLOT_SIZE) + 8


TDeviceConfiguration defaultDevConfig() {
  return {
    0x6502, //Magic
    0, //settings bank
    {}, //dallas
    13 //default blower cycle
  };
}

TControlConfiguration defaultConfig() {
  return {
    0x6502, // Magic; //should always be 0x6502
    50,     // TCO; //co temp 
    50,    //TCWU;  //cwu temp
    0, //uint8_t TCWU2; //cwu2 temp
    44, //uint8_t TMinPomp; //minimum pump run temp.
    6, //uint8_t THistCwu; //histereza cwu
    6, //uint8_t THistCO;  //histereza co
    5, //uint8_t TDeltaCO; //delta co - temp powyzej zadanej przy ktorej przejdzie w podtrzymanie
    3, //uint8_t TDeltaCWU; //delta cwu - temp powyżej bojlera do ktorej rozgrzewamy piec
    35, //uint8_t P0BlowerTime; //czas pracy dmuchawy w podtrzymaniu
    2, // P0FuelFreq; //podawanie wegla co x cykli przedmuchu
    false, // _HomeThermostat;
    false, //SummerMode; //tryb letni
    {
      {
        5 * 60, //P0 - 5 min
        50,
        20,
        7
      },
      {
        60,  //P1 - 60 sec
        4*10,
        20,
        0
      },
      {
        60, //P2 - 42 sec
        9 * 10,
        50,
        0
      },
      {
        150, //P2 - 42 sec
        25 * 10,
        50,
        0
      }
    }, //  TBurnParams BurnConfigs[MAX_POWER_STATES]; //first one [0] is the podtrzymanie
    80, //uint8_t FeederTempLimit;
    25, //uint8_t NoHeatAlarmCycles; //time needed to deterimine if we have the fire
     0, //EnableThermostat; //0 or 1
    2 * 10, //uint8_t CooloffTimeM10; //minutes * 10
    3 * 10, // CooloffPauseM10; //minutes * 10
    13100, // FuelGrH; //fuel grams per hour of feeder work. 10 kg=10000. 
    200,  // FuelHeatValueMJ10; //fuel heat in MJ, * 10 (100 = 10MJ)
      1, // CooloffMode; //0 - none
      0, //  FuelCorrection; //0 - none, fuel feed correction %. value=20 => make feed time longer by 20%, value = -20 - make it shorter by 20%
    60, //uint8_t CircCycleMin; //60, 30, 15, 10, 6
    0, //CircWorkTimeS; //circ pump working time per cycle, sec*10 (10 = 100 sec)
    30, //ReductionP2ExtraTime; //in %, how much % of the P2 cycle time to add for reduction (0 = just the P2 cycle, 10 = P2 cycle + 10%)
    60, //BlowerMax; //Blower max value that will be our 100

     0,  //FireStartMode; //0 - none, 1-auto off, 2 - auto on and off
     3, // NumFireStartCycles; //automatic firestart timeout in minutes * 10 (250 = 25 minutes). After that time we conclude 'failed to start fire' if not detected earlier
   120, // HeaterMaxRunTimeS; //maximum run time of the heater. If exceeded, heater will be turned off for the duration of one cycle (of STATE_FIRESTART)
   5 * 10,  //FireDetExhDt10; //exhaust above co temp - times 10
   5 * 10, //uint8_t FireDetExhIncrD10; //how much has exh temp to increase
   5 * 10, //FireDetCOIncr10; //how much has CO temp to increase
   3,      //P0CyclesBeforeStandby;
  };
}

TControlConfiguration g_CurrentConfig = defaultConfig();
TDeviceConfiguration g_DeviceConfig = defaultDevConfig();

TDailyLogEntry g_DailyLogEntries[DAILY_LOG_ENTRIES];


void readInitialConfig() {
  EEPROM.get(0, g_DeviceConfig);
  if (g_DeviceConfig.Magic != 0x6502) {
    Serial.print(F("Failed to restore device config - magic "));
    Serial.println(g_DeviceConfig.Magic);
    g_DeviceConfig = defaultDevConfig();
  }
  eepromRestoreConfig(g_DeviceConfig.SettingsBank);
  memset(g_DailyLogEntries, 0, sizeof(g_DailyLogEntries)); 
}


///save g_CurrentConfigSlot to eeprom so we read right config on next start
void updateDeviceConfig() {
  g_DeviceConfig.Magic = 0x6502;
  EEPROM.put(0, g_DeviceConfig);
}

//restore global configuration 
//from a specified slot. 0 is the default slot
bool eepromRestoreConfig(uint8_t configSlot) {
  uint16_t magic;
  if (configSlot > MAX_CFG_SLOTS) return false;
  TControlConfiguration tmp;
  EEPROM.get(DEV_CFG_SLOT_SIZE + configSlot * CFG_SLOT_SIZE, tmp);
  if (tmp.Magic != 0x6502) {
    Serial.print(F("Failed to read config - no magic in slot "));
    Serial.println(configSlot);
    tmp = defaultConfig();
  }
  g_CurrentConfig = tmp;
  Serial.print(F("Config restored from slot "));
  Serial.println(configSlot);
  return true;
}

//store current configuration in specified config slot
void eepromSaveConfig(uint8_t configSlot) {
  if (configSlot > MAX_CFG_SLOTS) return;
  g_CurrentConfig.Magic = 0x6502;
  EEPROM.put(DEV_CFG_SLOT_SIZE + configSlot * CFG_SLOT_SIZE, g_CurrentConfig);
  Serial.print(F("Config saved in slot "));
  Serial.println(configSlot);
}

void resetConfig() {
  g_CurrentConfig = defaultConfig();
}

//reset config to default
void eepromResetConfig(uint8_t configSlot) {
  g_CurrentConfig = defaultConfig();
  eepromSaveConfig(configSlot);
}


#define DAILY_LOG_BASE AFTER_CONFIG_STORAGE

uint8_t pdow = 200;


void resetLogEntry(uint8_t d, bool writeEeprom) {
  memset(&g_DailyLogEntries[d], 0, sizeof(TDailyLogEntry));
  g_DailyLogEntries[d].MDay = RTC.mm;
  if (writeEeprom) {
    EEPROM.put(DAILY_LOG_BASE + (d * sizeof(TDailyLogEntry)), g_DailyLogEntries[d]); 
  }
}

uint8_t g_SDEnabled = 0;

void sdInit() {
#ifdef ENABLE_SD
  g_SDEnabled = 0;
  //pinMode(10,OUTPUT);
  //digitalWrite(10,HIGH);
  Serial.print(F("Starting SD.."));
  if(!SD.begin(4)) {
    Serial.println(F(" - failed"));
  }
  else {
    Serial.println(F("SD ok"));
    g_SDEnabled = 1;
    File ft = SD.open("testa.txt", FILE_WRITE);
    if (!ft) {
      Serial.println(F("test file fail"));
    }
    else {
      ft.print("TEST ");
      ft.println(RTC.dd);
      ft.close();
      if (!SD.exists("testa.txt")) {
        Serial.println(F("Test file not exists!"));
      }
      else {
        Serial.println(F("SD write ok"));
      }
    }
  }
#endif
}
//so what is the rule for 
// updating daily logs? we're running modulo 7 so there will be need to overwrite previous week
// how do we know if it's today or 7 days ago? by looking at mday
void loggingInit() {
  Serial.print(F("Logging init. config entries:"));
  Serial.print(MAX_CFG_SLOTS * sizeof(TControlConfiguration));
  Serial.print(F(" d.entries:"));
  Serial.print(sizeof(g_DailyLogEntries));
  Serial.print(F(" stored at"));
  Serial.print(DAILY_LOG_BASE);
  for (int i=0; i<7; i++) {
    EEPROM.get(DAILY_LOG_BASE + i*sizeof(TDailyLogEntry), g_DailyLogEntries[i]);
  }
  
  pdow = RTC.dow - 1;
  if (g_DailyLogEntries[pdow].MDay != RTC.mm) {
    resetLogEntry(pdow, true);
    g_DailyLogEntries[pdow].MDay = RTC.mm;
    Serial.print(F(" cleared entry "));
    Serial.print(pdow);
  }
  Serial.print(F(". loaded mday "));
  Serial.println(g_DailyLogEntries[pdow].MDay);
#ifdef ENABLE_SD
  sdInit();
#endif
}



void clearDailyLogs() {
  memset(g_DailyLogEntries, 0, sizeof(g_DailyLogEntries));
  for (int i=0; i<7; i++) {
    EEPROM.put(DAILY_LOG_BASE + i * sizeof(TDailyLogEntry), g_DailyLogEntries[i]);
    TDailyLogEntry te;
    EEPROM.get(DAILY_LOG_BASE + i * sizeof(TDailyLogEntry), te);
    Serial.print("clr f=");
    Serial.print(te.FeederTotalSec);
    Serial.print(",");
    Serial.print(te.P0TotalSec2);
    Serial.print(",");
    Serial.println(te.P2TotalSec2);
  }
  
}
void printFloat(float f, File t) {
  char buf[10];
  dtostrf(f, 3, 2, buf);
  t.print(buf);
}

unsigned long _lastSDRun = 0;
extern unsigned long g_feederRunMs;
extern unsigned long g_pumpCORunMs;
extern unsigned long g_pumpCWURunMs;
extern unsigned long g_pumpCircRunMs;


void sdLoggingTask() {
  //if (g_SDEnabled == 0) return;
  unsigned long t = millis();
  
  if ((t - _lastSDRun) < 60L * 1000L) return; 
  _lastSDRun = t;
  char buf[100];
  sprintf(buf, "p%02d%02d.txt", RTC.mm, RTC.dd);
  File df = SD.open(buf, O_CREAT | O_APPEND | O_WRITE);
  if (!df) {
    Serial.print(F("sd file error "));
    Serial.println(buf);
    return;
  }
  else Serial.println(buf);
  df.print(RTC.h);
  df.print(':');
  df.print(RTC.m);
  df.print('\t');
  printFloat(g_TempCO, df);
  df.print('\t');
  printFloat(g_TempCWU, df);
  df.print('\t');
  printFloat(g_TempPowrot, df);
  df.print('\t');
  printFloat(g_TempSpaliny, df);
  df.print('\t');
  printFloat(g_TempFeeder, df);
  df.print('\t');
  printFloat(g_TempBurner, df);
  df.print('\t');
  printFloat(g_dTl3, df);
  df.print('\t');
  df.print(BURN_STATES[g_BurnState].Code);
  df.print('\t');
  df.print(g_needHeat);
  df.print('\t');
  df.print(getCurrentBlowerPower());
  df.print('\t');
  df.print(isPumpOn(PUMP_CO1));
  df.print('\t');
  df.print(isPumpOn(PUMP_CWU1));
  df.print('\t');
  df.print(isPumpOn(PUMP_CIRC));
  df.print('\t');
  printFloat(g_TempZewn, df);
  df.print('\t');
  df.print(g_feederRunMs);
  df.print('\t');
  df.print(g_pumpCORunMs);
  df.print('\t');
  df.print(g_pumpCWURunMs);
  df.print('\t');
  df.print(g_pumpCircRunMs);
  df.print('\t');
  printFloat(g_TempSpaliny-g_InitialTempExh, df);
  df.print('\t');
  printFloat(g_dTExh, df);
  df.print('\t');
  printFloat(g_TempCO-g_InitialTempCO, df);
  df.print('\t');
  df.print(isHeaterOn());
  df.println();
  df.close();
}


void loggingTask() {
  static unsigned long lastRun = 0;
  static uint8_t g_PrevState = 0;
  uint8_t d = RTC.dow - 1;
  if (pdow > 7) pdow = d;
  unsigned long t = millis();

  sdLoggingTask();
  
  
  g_DailyLogEntries[d].FeederTotalSec += g_FeederRunTime / 1000L;
  g_FeederRunTime = 0;
  g_DailyLogEntries[d].P1TotalSec2 += g_P1Time / 2000L; //2000 because number of secs is div by 2
  g_P1Time = 0;
  g_DailyLogEntries[d].P2TotalSec2 += g_P2Time / 2000L;
  g_P2Time = 0;
  g_DailyLogEntries[d].P0TotalSec2 += (g_P0Time / 2000L);
  g_P0Time = 0;
  
}

float calculateFuelWeightKg(unsigned long feederCycleSec) {
  return  ((float) g_CurrentConfig.FuelGrH / 1000.0) * ((float) feederCycleSec / 3600.0);
};
///fuel rate in grams/hr
int calculateFuelRateGrH(float feedTimePerCycle, int cycleLen) {
  float f0 = ((float) feedTimePerCycle) / cycleLen;
  return (int) (g_CurrentConfig.FuelGrH * f0);
}

float calculateHeatPowerFor(float feedTimePerCycle, int cycleLength) {
  int grH = calculateFuelRateGrH(feedTimePerCycle, cycleLength);
  //how many MJ per h?
  float v = ((float) grH * (float) g_CurrentConfig.FuelHeatValueMJ10) / (10000.0 * 3.6); 
  return v;
}

char g_command[40];

void commandHandlingTask() {
  static uint8_t len = 0;
  int c;
  while((c = Serial.read()) > 0) 
  {
    if (c == '\r' || c == '\n') {
      if (len > 0) {
        processCommand(g_command);
        len = 0;
      }
      continue;
    }
    g_command[len++] = (char) c;
    if (len < sizeof(g_command)) {
      g_command[len] = 0;
    }
    else {
      g_command[len - 1] = 0;
    }
  }
}

void dumpVariablesToSerial();

void processCommand(char* cmd) {
  Serial.println(cmd);
  char* p = strchr(cmd, '=');
  if (cmd[0] == 0 || cmd[0] == ';' || cmd[0] == '#') return;
  if (p > 0) {
    p[0] = 0;
    p++;
    bool found = false;
    for(int i=0; i<N_UI_VARIABLES; i++) {
      if (strcmp(cmd, UI_VARIABLES[i].Name) == 0) {
        updateVariableFromString(i, p);
        found = true;
        break;
      }
    }
    if (!found) {
      Serial.print(F("Variable not found:"));
      Serial.println(cmd);
    }
  }
  else if (strcmp(cmd, "get") == 0) {
    dumpVariablesToSerial();
  }
}

void dumpVariablesToSerial() {
  char buf[20];
  for(int i=0; i<N_UI_VARIABLES; i++) {
    const TUIVarEntry& ent = UI_VARIABLES[i];
    if (ent.PrintTo == NULL) continue;
    ent.PrintTo(i, NULL, buf, false);
    Serial.print(ent.Name);
    Serial.print('=');
    Serial.println(buf);
  }
}
