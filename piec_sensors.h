#ifndef _PIEC_SENSORS_H_INCLUDED_
#define _PIEC_SENSORS_H_INCLUDED_

#include <DallasTemperature.h>


#define TSENS_BOILER 0 //temp pieca
#define TSENS_CWU 1    //temp bojlera cwu
#define TSENS_FEEDER 2 //temp podajn.
#define TSENS_RETURN 3  //temp powrotu
#define TSENS_EXTERNAL 4  //temp zewnetrzna
#define TSENS_CWU2 5  //temp user 1
#define TSENS_USR1 6  //temp user 2 
#define TSENS_USR2 7  //temp user 3

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
void getDallasAddress(uint8_t idx, uint8_t buf[]);
void swapDallasAddress(uint8_t idx1, uint8_t idx2);
bool ensureDallasSensorAtIndex(uint8_t idx, uint8_t addr[8]);
int findDallasIndex(uint8_t addr[8]);
bool isThermocoupleEnabled(uint8_t idx);

void updateDallasSensorAssignmentFromConfig();
void printDallasInfo(uint8_t idx, char* buf);
#endif
