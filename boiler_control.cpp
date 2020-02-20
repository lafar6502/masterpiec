#include <arduino.h>
#include "piec_sensors.h"
#include "boiler_control.h"
#include "global_variables.h"


void setPumpOn(uint8_t num) {
  
  
}

void setPumpOff(uint8_t num) {
  
}
bool isPumpOn(uint8_t num);
bool isPumpEnabled(uint8_t num);

//uruchomienie podajnika
void setFeederOn() {
  digitalWrite(HW_FEEDER_CTRL_PIN, HIGH);
}
//zatrzymanie podajnika
void setFeederOff() {
  digitalWrite(HW_FEEDER_CTRL_PIN, LOW);
}
//czy podajnik działa
bool isFeederOn() {
 return digitalRead(HW_FEEDER_CTRL_PIN) == HIGH;
}



void triacOn() {
  digitalWrite(HW_BLOWER_CTRL_PIN, HIGH);
}

void triacOff() {
  digitalWrite(HW_BLOWER_CTRL_PIN, LOW);
}

//pulse count
volatile uint32_t counter;
volatile uint32_t power_counter; 
float brese_increment = 0.0;
float brese_error = 0.0;
uint8_t brese_cycle = 1;
uint8_t brese_curV = 0;

//1 - triac on, 0 - triac off
uint8_t breseControlStep() {
  uint8_t rem = counter % brese_cycle;
  
  if (rem == 0) {
    brese_error += brese_increment;
    brese_curV = round(brese_error);
    brese_error -= brese_curV;
  }
  return brese_curV > rem ? 1 : 0;
}



void zeroCrossHandler() {
  counter++;
  uint8_t stp = breseControlStep();
  if (stp) {
    power_counter++;
    triacOn();
  }
  else {
    triacOff();
  }
}



//power in % 0..100, cycleLength 1..255
void breseInit(uint8_t power, uint8_t cycleLength) {
    brese_increment = ((float) power) * cycleLength / 100.0;
    brese_cycle = cycleLength;
    brese_curV = 0;
    brese_error = 0;
}

void initializeBlowerControl() {
  pinMode(HW_BLOWER_CTRL_PIN, OUTPUT);
  triacOff();
  pinMode(HW_ZERO_DETECT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(HW_ZERO_DETECT_PIN), zeroCrossHandler, RISING);  
}

uint8_t getCycleLengthForBlowerPower(uint8_t power) {
  return 7;
}

void setBlowerPower(uint8_t power) {
  breseInit(power, getCycleLengthForBlowerPower(power));
}



uint8_t getCurrentBlowerPower() {
  return brese_increment * 100.0 / brese_cycle;
}
