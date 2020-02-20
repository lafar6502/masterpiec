#ifndef _PIEC_SENSORS_H_INCLUDED_
#define _PIEC_SENSORS_H_INCLUDED_

#include <DallasTemperature.h>
#include "hwsetup.h"



void initializeDallasSensors();
void initializeMax6675Sensors();


void refreshSensorReadings();
float getLastDallasValue(uint8_t idx);
float getLastThermocoupleValue(uint8_t idx);
//aktualna wartość (odczytaj teraz)
float readThermocoupleValue(uint8_t idx);
bool isDallasEnabled(uint8_t idx);
bool isThermocoupleEnabled(uint8_t idx);



#endif
