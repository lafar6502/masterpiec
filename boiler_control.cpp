#include <arduino.h>
#include "hwsetup.h"
#include "piec_sensors.h"
#include "boiler_control.h"
#include "global_variables.h"

struct tPumpPin {
  uint8_t Pin;
  bool    Enabled;
  bool    On;
};

tPumpPin pump_ctrl_pins[] = {
  {HW_PUMP_CO1_CTRL_PIN, false},
  {HW_PUMP_CWU1_CTRL_PIN, false},
  {HW_PUMP_CO2_CTRL_PIN, false},
  {HW_PUMP_CIRC_CTRL_PIN, false}
};


void setPumpOn(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPumpPin)) return;
  pump_ctrl_pins[num].On = true;
  digitalWrite(pump_ctrl_pins[num].Pin, HIGH);
}

void setPumpOff(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPumpPin)) return;
  pump_ctrl_pins[num].On = false;
  digitalWrite(pump_ctrl_pins[num].Pin, LOW);
}

bool isPumpOn(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPumpPin)) return false;
  //return pump_ctrl_pins[num].On;
  return digitalRead(pump_ctrl_pins[num].Pin) != LOW;
}

bool isPumpEnabled(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPumpPin)) return false;
  return true;
}

bool feeder = false;
void setFeeder(bool on) {
  feeder = on;
  digitalWrite(HW_FEEDER_CTRL_PIN, on ? HIGH : LOW);
}
//uruchomienie podajnika
void setFeederOn() {
  setFeeder(true);
}
//zatrzymanie podajnika
void setFeederOff() {
  setFeeder(false);
}
//czy podajnik działa
bool isFeederOn() {
 return digitalRead(HW_FEEDER_CTRL_PIN) != LOW;
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
uint8_t breseControlStep() 
{
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
    //brese_curV = 0;
    //brese_error = 0;
    //Serial.print("brese pow:");
    //Serial.print(power);
    //Serial.print(", c:");
    //Serial.println(brese_cycle);
}

void initializeBlowerControl() {
  pinMode(HW_BLOWER_CTRL_PIN, OUTPUT);
  triacOff();
  pinMode(HW_ZERO_DETECT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(HW_ZERO_DETECT_PIN), zeroCrossHandler, RISING);  
  pinMode(HW_PUMP_CO1_CTRL_PIN, OUTPUT);
  pinMode(HW_PUMP_CWU1_CTRL_PIN, OUTPUT);
  pinMode(HW_PUMP_CO2_CTRL_PIN, OUTPUT);
  pinMode(HW_PUMP_CIRC_CTRL_PIN, OUTPUT);
  pinMode(HW_FEEDER_CTRL_PIN, OUTPUT);
  digitalWrite(HW_PUMP_CO1_CTRL_PIN, LOW);
  digitalWrite(HW_PUMP_CWU1_CTRL_PIN, LOW);
  digitalWrite(HW_PUMP_CO2_CTRL_PIN, LOW);
  digitalWrite(HW_PUMP_CIRC_CTRL_PIN, LOW);
  digitalWrite(HW_FEEDER_CTRL_PIN, LOW);
}



uint8_t getCycleLengthForBlowerPower(uint8_t power) {
  return 7;
}

void setBlowerPower(uint8_t power) {
  breseInit(power, getCycleLengthForBlowerPower(power));
}

void setBlowerPower(uint8_t power, uint8_t powerCycle) 
{
  breseInit(power, powerCycle);
}


uint8_t getCurrentBlowerPower() {
  return brese_increment * 100.0 / brese_cycle;
}
