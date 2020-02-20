#ifndef _PIEC_SENSORS_H_INCLUDED_
#define _PIEC_SENSORS_H_INCLUDED_

#include <DallasTemperature.h>

#define DALLAS_SENSOR_DATA_PIN 22
#define MAX_DALLAS_SENSORS 8

#define TSENS_BOILER 0 //temp pieca
#define TSENS_CWU 1    //temp bojlera cwu
#define TSENS_FEEDER 2 //temp podajn.
#define TSENS_RETURN 3  //temp powrotu
#define TSENS_EXTERNAL 4  //temp zewnetrzna
#define TSENS_USR1 5  //temp user 1
#define TSENS_USR2 6  //temp user 2 
#define TSENS_USR3 7  //temp user 3

#define MAX_THERMOCOUPLES 2
#define T2SENS_EXHAUST 0  //temp. spalin
#define T2SENS_BURNER  1  //temp. palnika

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
