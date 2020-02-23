#include <arduino.h>
#include "global_variables.h"
#include "masterpiec.h"
#include "boiler_control.h"
#include <EEPROM.h>
#include <MD_DS1307.h>

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
    300 //cooloff pause m10
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

uint8_t pdow;
void loggingInit() {
  Serial.print(F("Logging init. config entries:"));
  Serial.print(MAX_CFG_SLOTS * sizeof(TControlConfiguration));
  Serial.print(F(" d.entries:"));
  Serial.print(sizeof(g_DailyLogEntries));
  Serial.print(F(" stored at"));
  Serial.print(DAILY_LOG_BASE);
  EEPROM.get(AFTER_CONFIG_STORAGE, g_DailyLogEntries);
  Serial.println(". loaded");
  pdow = RTC.dow - 1;
}

void clearDailyLogs() {
  memset(g_DailyLogEntries, 0, sizeof(g_DailyLogEntries));
  EEPROM.put(DAILY_LOG_BASE, g_DailyLogEntries);
}

void loggingTask() {
  static unsigned long lastRun = 0;
  
  unsigned long t = millis();
  if (t - lastRun > 5L * 60 * 1000) {
    lastRun = t;
    uint8_t d = RTC.dow - 1;
    if (pdow != d) {
      EEPROM.put(DAILY_LOG_BASE + (pdow * sizeof(TDailyLogEntry)), g_DailyLogEntries[pdow]); //prev day
      memset(&g_DailyLogEntries[d], 0, sizeof(TDailyLogEntry)); //zero the new entry 
    }
    g_DailyLogEntries[d].FeederTotalSec += g_FeederRunTime / 1000L;
    g_FeederRunTime = 0;
    g_DailyLogEntries[d].FeederTotalSec += g_P1Time / 1000L;
    g_P1Time = 0;
    g_DailyLogEntries[d].FeederTotalSec += g_P2Time / 1000L;
    g_P2Time = 0;
    pdow = d;
    EEPROM.put(DAILY_LOG_BASE + (d * sizeof(TDailyLogEntry)), g_DailyLogEntries[d]);
    Serial.print(F("log saved "));
    Serial.println(d);
  }
}
