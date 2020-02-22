#ifndef _MASTERPIEC_H_INCLUDED_
#define _MASTERPIEC_H_INCLUDED_

#include "global_variables.h"




//restore global configuration 
//from a specified slot. 0 is the default slot
bool eepromRestoreConfig(uint8_t configSlot);

//store current configuration in specified config slot
void eepromSaveConfig(uint8_t configSlot);

//reset config to default
void eepromResetConfig(uint8_t configSlot);

///przejscie w dany stan bez sprawdzania war. początkowych
void forceState(TSTATE state);

#endif
