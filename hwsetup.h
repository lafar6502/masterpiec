#ifndef _HWSETUP_H_INCLUDED_
#define _HWSETUP_H_INCLUDED_

#define DALLAS_SENSOR_DATA_PIN 22
#define MAX_DALLAS_SENSORS 8

//B (digital pin 8 to 13)
//C (analog input pins)
//D (digital pins 0 to 7)

//82  PK7 ( ADC15/PCINT23 ) Analog pin 15
//83  PK6 ( ADC14/PCINT22 ) Analog pin 14
//84  PK5 ( ADC13/PCINT21 ) Analog pin 13
//85  PK4 ( ADC12/PCINT20 ) Analog pin 12
//86  PK3 ( ADC11/PCINT19 ) Analog pin 11
//87  PK2 ( ADC10/PCINT18 ) Analog pin 10
//88  PK1 ( ADC9/PCINT17 )  Analog pin 9
//89  PK0 ( ADC8/PCINT16 )  Analog pin 8


#define HW_ENCODER_PINA 15
#define HW_ENCODER_PINB 18
#define HW_ENCODER_PINBTN 34
#define HW_DISPLAY_PIN 1
#define HW_ZERO_DETECT_PIN 2


//#define HW_FLOW_SENSOR_PULSE_PIN 3  //pulse width detection pin for pulse-driven flow meter
#define PUMP_CO1 0
#define PUMP_CWU1 1
#define PUMP_CO2 2
#define PUMP_CIRC 3
#define FEEDER 4
#define BLOWER 5

//#define HW_PUMP_CIRC_CTRL_PIN  A8   //portk 0
#define HW_FLOW_SENSOR_INPUT_PIN A0  //flow sensor input voltage (for voltage-out flow meter)
#define HW_PUMP_CO1_CTRL_PIN  A9    //portk 1
#define HW_FEEDER_CTRL_PIN A10      //portk 2
#define HW_PUMP_CIRC_CTRL_PIN  A11  //portk 3
#define HW_BLOWER_CTRL_PIN A12      //portk 5 remember mask
#define HW_HEATER_CTRL_PIN  A13    //portk 4
#define HW_PUMP_CO2_CTRL_PIN  A14   //portk 6
#define HW_PUMP_CWU1_CTRL_PIN  A15  //portk 7

//#define MASK_PUMP_CIRC  0b00000001
#define MASK_PUMP_CO1   0b00000010
#define MASK_FEEDER     0b00000100
#define MASK_PUMP_CIRC  0b00001000
#define MASK_BLOWER     0b00010000
#define MASK_HEATER     0b00100000
#define MASK_PUMP_CO2   0b01000000
#define MASK_PUMP_CWU1  0b10000000


#define HW_THERMOSTAT_PIN 47
#define HW_THERMOSTAT_PIN_ALT 26

#define MAX6675_0_SCK_PIN 39
#define MAX6675_0_CS_PIN 41
#define MAX6675_0_SO_PIN 40

#define MAX6675_1_SCK_PIN 0 //disable
#define MAX6675_1_CS_PIN 44
#define MAX6675_1_SO_PIN 46


#define DISPLAY_TEXT_LINES 2
#define DISPLAY_TEXT_LEN  16

//#define MPIEC_ENABLE_WEBSERVER
//#define MPIEC_ENABLE_SCRIPT

#endif
