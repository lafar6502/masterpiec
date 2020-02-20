#ifndef _HWSETUP_H_INCLUDED_
#define _HWSETUP_H_INCLUDED_

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


#define HW_FEEDER_CTRL_PIN 11
#define HW_ENCODER_PINA 15
#define HW_ENCODER_PINB 18
#define HW_ENCODER_PINBTN 34
#define HW_DISPLAY_PIN 1
#define HW_ZERO_DETECT_PIN 2
#define HW_BLOWER_CTRL_PIN 32
#endif
