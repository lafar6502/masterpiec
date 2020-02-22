#include <arduino.h>
#include "global_variables.h"
#include "masterpiec.h"
#include <EEPROM.h>


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
    5   //DefaultBlowerCycle
  };
}

TControlConfiguration g_CurrentConfig = defaultConfig();


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
