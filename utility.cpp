#include <arduino.h>
#include <avr/pgmspace.h>
#include <wstring.h>
#include "global_variables.h"
#include "masterpiec.h"
#include "boiler_control.h"
#include <EEPROM.h>
#include <MD_DS1307.h>
#include "ui_handler.h"

#define MAX_CFG_SLOTS 4
#define AFTER_CONFIG_STORAGE (MAX_CFG_SLOTS * sizeof(TControlConfiguration)) + 8

TControlConfiguration defaultConfig() {
  return {
    0x6502,
    50, //co
    48, //cwu1
    0,  //cwu2
    35, //min pomp
    3,  //cwu hist
    3,  //co hist
    5,  //co delta
    3,  //cwu delta
    30, //czas dmuchawy w P0
    3,  //cykl podawania wegla w P0
    false, //tryb letni
    false, //termostat
    {
      {
        5 * 60, //P0 - 5 min
        50,
        20,
        7
      },
      {
        60,  //P1 - 60 sec
        20,
        9,
        7
      },
      {
        42, //P2 - 42 sec
        60,
        50,
        7
      }
    },
    {}, //dallas
    5,   //DefaultBlowerCycle
    100,  //feeder temp limit
    8, //NoHeatAlarmTimeM
    0,
    20,
    300, //cooloff pause m10
    10000,
    260,
    COOLOFF_OVERHEAT
  };
}

TControlConfiguration g_CurrentConfig = defaultConfig();

TDailyLogEntry g_DailyLogEntries[DAILY_LOG_ENTRIES];


//restore global configuration 
//from a specified slot. 0 is the default slot
bool eepromRestoreConfig(uint8_t configSlot) {
  uint16_t magic;
  TControlConfiguration tmp;
  EEPROM.get(configSlot * sizeof(TControlConfiguration), magic);
  if (magic != 0x6502) {
    Serial.print("Failed to read config - no magic in slot ");
    Serial.println(configSlot);
    return false;
  }
  EEPROM.get(configSlot * sizeof(TControlConfiguration), tmp);
  g_CurrentConfig = tmp;
  Serial.print("Config restored from slot ");
  Serial.println(configSlot);
  return true;
}

//store current configuration in specified config slot
void eepromSaveConfig(uint8_t configSlot) {
  g_CurrentConfig.Magic = 0x6502;
  EEPROM.put(configSlot * sizeof(TControlConfiguration), g_CurrentConfig);
  Serial.print("Config saved in slot ");
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
    EEPROM.get(AFTER_CONFIG_STORAGE + i*sizeof(TDailyLogEntry), g_DailyLogEntries[i]);
  }
  
  pdow = RTC.dow - 1;
  if (g_DailyLogEntries[pdow].MDay != RTC.mm) {
    resetLogEntry(pdow, true);
    Serial.print(F(" cleared entry "));
    Serial.print(pdow);
  }
  Serial.println(". loaded");
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

void loggingTask() {
  static unsigned long lastRun = 0;
  uint8_t d = RTC.dow - 1;
  if (pdow > 7) pdow = d;
  unsigned long t = millis();
  
  if (pdow != d) {
      EEPROM.put(DAILY_LOG_BASE + (pdow * sizeof(TDailyLogEntry)), g_DailyLogEntries[pdow]); //prev day - save
      pdow = d;
      resetLogEntry(d, true);
  }
  if (g_DailyLogEntries[d].MDay != RTC.mm) {
    Serial.print(d);
    Serial.print(F("! wrong mday:"));
    Serial.println(g_DailyLogEntries[d].MDay);
  }
  g_DailyLogEntries[d].FeederTotalSec += g_FeederRunTime / 1000L;
  g_FeederRunTime = 0;
  g_DailyLogEntries[d].P1TotalSec2 += g_P1Time / 2000L; //2000 because number of secs is div by 2
  g_P1Time = 0;
  g_DailyLogEntries[d].P2TotalSec2 += g_P2Time / 2000L;
  g_P2Time = 0;
  g_DailyLogEntries[d].P0TotalSec2 += (g_P0Time / 2000L);
  g_P0Time = 0;
  
  
  if (t - lastRun > 15L * 60 * 1000) //save every 15 m
  {
    lastRun = t;
    TDailyLogEntry de = g_DailyLogEntries[d];
    EEPROM.put(DAILY_LOG_BASE + (d * sizeof(TDailyLogEntry)), g_DailyLogEntries[d]);
    EEPROM.get(DAILY_LOG_BASE + (d * sizeof(TDailyLogEntry)), de);
    Serial.print(F("log saved "));
    Serial.print(d);
    Serial.print(F(",feed="));
    Serial.print(g_DailyLogEntries[d].FeederTotalSec);
    Serial.print(F(",p1="));
    Serial.print(g_DailyLogEntries[d].P1TotalSec2);
    Serial.print(F(",p2="));
    Serial.print(g_DailyLogEntries[d].P2TotalSec2);
    Serial.print(F(",p0="));
    Serial.println(g_DailyLogEntries[d].P0TotalSec2);
    if (memcmp(&de, g_DailyLogEntries + d, sizeof(TDailyLogEntry)) != 0) {
      Serial.print(d);
      Serial.println(F("! log entry check fail !"));
    }
  }
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
