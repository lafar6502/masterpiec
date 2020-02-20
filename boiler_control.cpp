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

void setBlowerPower(uint8_t power) {
  
}
uint8_t getCurrentBlowerPower();
