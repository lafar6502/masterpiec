#include <arduino.h>
#include "hwsetup.h"
#include "piec_sensors.h"
#include "boiler_control.h"
#include "global_variables.h"
#include "digitalWriteFast.h"

#define KICKSTART_MIN 10 //minimum pulses/sec - below that we run a kickstart

struct tPowerControlPin {
  uint8_t Pin;
  uint8_t Mask;
  bool    Enabled;
  bool    On;
};

//flags for synced turning on/off of port K bits
volatile uint8_t g_powerFlags = 0;
volatile uint8_t g_powerBits = 0;

//PINTK, PO
#define POWER_PORT_MASK 0b11111111

tPowerControlPin pump_ctrl_pins[] = {
  {HW_PUMP_CO1_CTRL_PIN,  MASK_PUMP_CO1, false},
  {HW_PUMP_CWU1_CTRL_PIN, MASK_PUMP_CWU1, false},
  {HW_PUMP_CO2_CTRL_PIN, MASK_PUMP_CO2, false},
  {HW_PUMP_CIRC_CTRL_PIN, MASK_PUMP_CIRC, false},
 
  {HW_FEEDER_CTRL_PIN, MASK_FEEDER, true},
  {HW_BLOWER_CTRL_PIN, MASK_BLOWER, true},
  
};

unsigned long g_FeederRunTime = 0;
unsigned long g_LastFeederStart = 0;


void setTriacOutOn(uint8_t num) {
  g_powerFlags |= pump_ctrl_pins[num].Mask;
}

void setTriacOutOff(uint8_t num) {
   g_powerFlags &= ~pump_ctrl_pins[num].Mask;
}

bool isTriacOutOnNow(uint8_t num) {
  return (PINK & pump_ctrl_pins[num].Mask) != 0;
}

void setPumpOn(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPowerControlPin)) return;
  pump_ctrl_pins[num].On = true;
  setTriacOutOn(num);
  //digitalWrite(pump_ctrl_pins[num].Pin, HIGH);
}

void setPumpOff(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPowerControlPin)) return;
  pump_ctrl_pins[num].On = false;
  setTriacOutOff(num);
  //g_powerFlags &= ~pump_ctrl_pins[num].Mask;
  //digitalWrite(pump_ctrl_pins[num].Pin, LOW);
}

bool isPumpOn(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPowerControlPin)) return false;
  //return pump_ctrl_pins[num].On;
  
  //return digitalRead(pump_ctrl_pins[num].Pin) != LOW;
  return isTriacOutOnNow(num);
}

bool isPumpEnabled(uint8_t num) {
  if (num >= sizeof(pump_ctrl_pins)/sizeof(tPowerControlPin)) return false;
  return true;
}


void setFeeder(bool on) {
  bool pr = (g_powerFlags & MASK_FEEDER) != 0;
  if (on) 
    g_powerFlags |= MASK_FEEDER;
  else
    g_powerFlags &= ~MASK_FEEDER;
  
  if (pr == on) return; // no change of state
  
  unsigned long m = millis();
  
  if (on) {
    g_LastFeederStart = m;  
  } else {   
    g_FeederRunTime += (m - g_LastFeederStart);
    g_LastFeederStart = m;
    Serial.print(F("Feed stop. run time ms "));
    Serial.println(g_FeederRunTime);
  }
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
  return (PINK & MASK_FEEDER) != 0;
 //return digitalReadFast(HW_FEEDER_CTRL_PIN) != LOW;
}


bool isHeaterOn() {
  //return digitalReadFast(HW_HEATER_CTRL_PIN) != LOW;
  return (PINK & MASK_HEATER) != 0;
}
void setHeater(bool on) {
   //digitalWriteFast(HW_HEATER_CTRL_PIN, b ? HIGH : LOW);
  if (on) 
    g_powerFlags |= MASK_HEATER;
  else
    g_powerFlags &= ~MASK_HEATER;
   
}


void triacOn() {
  digitalWriteFast(HW_BLOWER_CTRL_PIN, HIGH);
}

void triacOff() {
  digitalWriteFast(HW_BLOWER_CTRL_PIN, LOW);
}

//pulse count. We will reset it on every change of output power. It is used in kickstart mode.
volatile uint32_t counter;
volatile uint32_t power_counter; 

float brese_increment = 0.0;
float brese_error = 0.0;
uint8_t brese_cycle = 1;
uint8_t brese_curV = 0;
uint8_t power_set = 0;
uint8_t kickstartCount = 0;

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
 void zeroCrossHandler2() {
  counter++;
  g_powerBits = PINK & POWER_PORT_MASK;
  uint8_t stp = breseControlStep();
  if (kickstartCount > 0 && power_set > 0) {
     stp = 1;
     kickstartCount--;
  }
  if (stp) {
    power_counter++;
    g_powerFlags |= MASK_BLOWER;
  }
  else {
    g_powerFlags &= ~MASK_BLOWER;
  }
  PORTK = g_powerFlags & POWER_PORT_MASK; //put all bits at once
 }


//power in % 0..100, cycleLength 1..255
void breseInit(uint8_t power, uint8_t cycleLength) {
  kickstartCount = 0;
  float powerPerc = ((float) power) * (g_CurrentConfig.BlowerMax == 0 ? 1.0 : (float) g_CurrentConfig.BlowerMax / 100.0);
  if (power_set == 0) {
    counter = 0;
    power_counter = 0;    
    kickstartCount =  power > 0 && powerPerc < KICKSTART_MIN ? KICKSTART_MIN : 0;
  };
  power_set = power;
  brese_increment = powerPerc * cycleLength / 100.0;
  brese_cycle = cycleLength;
}

void initializeBlowerControl() {
  brese_cycle = g_CurrentConfig.DefaultBlowerCycle;
  pinModeFast(HW_BLOWER_CTRL_PIN, OUTPUT);
  triacOff();
  pinMode(HW_ZERO_DETECT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(HW_ZERO_DETECT_PIN), zeroCrossHandler2, RISING);  
  pinModeFast(HW_PUMP_CO1_CTRL_PIN, OUTPUT);
  pinModeFast(HW_PUMP_CWU1_CTRL_PIN, OUTPUT);
  pinModeFast(HW_PUMP_CO2_CTRL_PIN, OUTPUT);
  pinModeFast(HW_PUMP_CIRC_CTRL_PIN, OUTPUT);
  pinModeFast(HW_FEEDER_CTRL_PIN, OUTPUT);
  pinModeFast(HW_HEATER_CTRL_PIN, OUTPUT);
  
  digitalWriteFast(HW_PUMP_CO1_CTRL_PIN, LOW);
  digitalWriteFast(HW_PUMP_CWU1_CTRL_PIN, LOW);
  digitalWriteFast(HW_PUMP_CO2_CTRL_PIN, LOW);
  digitalWriteFast(HW_PUMP_CIRC_CTRL_PIN, LOW);
  digitalWriteFast(HW_FEEDER_CTRL_PIN, LOW);
  digitalWriteFast(HW_HEATER_CTRL_PIN, LOW);
  if (HW_THERMOSTAT_PIN != 0) 
  {
    pinModeFast(HW_THERMOSTAT_PIN, INPUT_PULLUP);  
  }
  if (HW_THERMOSTAT_PIN_ALT != 0) 
  {
    pinModeFast(HW_THERMOSTAT_PIN_ALT, INPUT); 
  }
}


bool isThermostatOn() {
  if (HW_THERMOSTAT_PIN != 0 && digitalReadFast(HW_THERMOSTAT_PIN) == LOW) return true;
  if (HW_THERMOSTAT_PIN_ALT != 0 && digitalReadFast(HW_THERMOSTAT_PIN_ALT) == HIGH) return true;
  return false;
}

uint8_t getCycleLengthForBlowerPower(uint8_t power) {
  return g_CurrentConfig.DefaultBlowerCycle;
}

void setBlowerPower(uint8_t power) {
  breseInit(power, brese_cycle == 0 ? getCycleLengthForBlowerPower(power) : brese_cycle);
}

void setBlowerPower(uint8_t power, uint8_t powerCycle) 
{
  breseInit(power, powerCycle);
}

uint8_t getCurrentBlowerCycle() {
  return brese_cycle;
}

uint8_t getCurrentBlowerPower() {
  return power_set;
}

unsigned long g_feederRunMs = 0;
unsigned long g_pumpCORunMs = 0;
unsigned long g_pumpCWURunMs = 0;
unsigned long g_pumpCircRunMs = 0;
//we just assume the thing has been running or not running the whole time since last check.
//so we should run this task quite frequently to avoid errors.
void gatherStatsTask() {
  static unsigned long _lastRun = 0;
  unsigned long t = millis();
  if (_lastRun != 0) {
    unsigned long rt = t - _lastRun;
    if (isFeederOn()) g_feederRunMs += rt;
    if (isPumpOn(PUMP_CO1)) g_pumpCORunMs += rt;
    if (isPumpOn(PUMP_CWU1)) g_pumpCWURunMs += rt;
    if (isPumpOn(PUMP_CIRC)) g_pumpCircRunMs += rt;  
  }
  _lastRun = t;
}
