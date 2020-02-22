#ifndef _BOILER_CONTROL_H_INCLUDED_
#define _BOILER_CONTROL_H_INCLUDED_

#include "global_variables.h"

void setPumpOn(uint8_t num);
void setPumpOff(uint8_t num);
bool isPumpOn(uint8_t num);
bool isPumpEnabled(uint8_t num);

//uruchomienie podajnika
void setFeederOn();
//zatrzymanie podajnika
void setFeederOff();
void setFeeder(bool b);
//czy podajnik działa
bool isFeederOn();

void setBlowerPower(uint8_t power);
void setBlowerPower(uint8_t power, uint8_t powerCycle);
uint8_t getCurrentBlowerPower();
uint8_t getCurrentBlowerCycle();

void initializeBlowerControl();




#endif
