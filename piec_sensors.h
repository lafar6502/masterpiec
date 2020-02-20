#ifndef _PIEC_SENSORS_H_INCLUDED_
#define _PIEC_SENSORS_H_INCLUDED_

#define DALLAS_SENSOR_DATA_PIN 22
#define MAX_DALLAS_SENSORS 8

#define TSENS_BOILER 0
#define TSENS_CWU 1
#define TSENS_FEEDER 2
#define TSENS_RETURN 3
#define TSENS_EXTERNAL 4
#define TSENS_USR1 5
#define TSENS_USR2 6
#define TSENS_USR3 7

#define MAX_THERMOCOUPLES 2
#define T2SENS_EXHAUST 0
#define T2SENS_BURNER  1

void initializeDallasSensors();
void initializeMax6675Sensors();


#endif
