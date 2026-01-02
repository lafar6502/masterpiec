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
int8_t g_BlowerPowerCorrection = 0; //blower power adjustment for current burn state. we adjust this adjustment when correcting flow.

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
  return (PINC & pump_ctrl_pins[num].Mask) != 0;
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
  return (PINC & MASK_FEEDER) != 0;
 //return digitalReadFast(HW_FEEDER_CTRL_PIN) != LOW;
}

void setSV2HeatingPin(bool b) {
  if (HW_SV2_HEATING_PIN == 0) return;
  digitalWriteFast(HW_SV2_HEATING_PIN, HW_SV2_PIN_ACTIVELOW ? !b : b);
}

bool getSV2HeatingPin() {
  return digitalReadFast(HW_SV2_HEATING_PIN) != LOW;
}

unsigned long g_heaterStartTimeMs = 0;

bool isHeaterOn() {
  //return digitalReadFast(HW_HEATER_CTRL_PIN) != LOW;
  return (PINC & MASK_HEATER) != 0;
}
void setHeater(bool on) {
   //digitalWriteFast(HW_HEATER_CTRL_PIN, b ? HIGH : LOW);
   bool b = isHeaterOn();
  if (on) 
  {
    if (!b) g_heaterStartTimeMs = millis();
    g_powerFlags |= MASK_HEATER;
  }
  else
  {
    g_powerFlags &= ~MASK_HEATER;
  }
}

unsigned long getHeaterRunningTimeMs() {
  bool b = isHeaterOn();
  if (!b) return 0;
  return millis() - g_heaterStartTimeMs;
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
  static uint8_t skipper = 0;
  
  if (rem == 0) {
    brese_error += brese_increment;
    brese_curV = round(brese_error);
    brese_error -= brese_curV;
  }
  uint8_t b = brese_curV > rem ? 1 : 0;
  if (isFlowTooHigh()) {
    uint8_t m = g_CurrentConfig.AirControlMode - AIRCONTROL_HITMISS0 + 1;
    if (m > 1 && m <= 4) {
        if (b != 0) {
          skipper++;
          return (skipper % m) == 0;
        }
    }
    return 0;
  }
  return brese_curV > rem ? 1 : 0;
}




 void zeroCrossHandler2() {
  counter++;
  g_powerBits = PINC & POWER_PORT_MASK;
  uint8_t pcBits = PINL;
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
  PORTC = g_powerFlags & POWER_PORT_MASK; //put all bits at once
  if (power_set == 0 && !getManualControlMode())
    PORTL = pcBits & ~MASK_FLOW_PWR;
  else
    PORTL = pcBits | MASK_FLOW_PWR;
 }



//power in % 0..255 (255 = 100%), cycleLength 1..255
void breseInit(uint8_t power, uint8_t cycleLength) {
  kickstartCount = 0;
  if (power == 0) g_BlowerPowerCorrection = 0;
  float p0 = power + g_BlowerPowerCorrection;
  if (p0 < 0) p0 = 0;
  if (p0 > 255) p0 = 255;
  float powerPerc = p0 * (g_CurrentConfig.BlowerMax == 0 ? 1.0 : (float) g_CurrentConfig.BlowerMax / 255.0);
  if (power_set == 0) {
    counter = 0;
    power_counter = 0;    
    kickstartCount =  power > 0 && powerPerc < KICKSTART_MIN ? KICKSTART_MIN : 0;
  };
  power_set = power;
  brese_increment = powerPerc * cycleLength / 255.0;
  brese_cycle = cycleLength;
}

int8_t getBlowerPowerCorrection() {
  return g_BlowerPowerCorrection;
}
void   setBlowerPowerCorrection(int8_t c) {
  g_BlowerPowerCorrection = c;
  breseInit(power_set, brese_cycle);
}


void initializeBlowerControl() {
  Serial.println("init gpio");
  delay(2000);
  brese_cycle = g_DeviceConfig.DefaultBlowerCycle;
  pinModeFast(HW_BLOWER_CTRL_PIN, OUTPUT);
  triacOff();
  pinMode(HW_ZERO_DETECT_PIN, INPUT);
  pinMode(HW_PUMP_CO1_CTRL_PIN, OUTPUT);
  pinMode(HW_PUMP_CWU1_CTRL_PIN, OUTPUT);
  pinMode(HW_PUMP_CO2_CTRL_PIN, OUTPUT);
  pinMode(HW_PUMP_CIRC_CTRL_PIN, OUTPUT);
  pinMode(HW_FEEDER_CTRL_PIN, OUTPUT);
  pinMode(HW_HEATER_CTRL_PIN, OUTPUT);
  pinMode(HF_FLOW_SENSOR_POWER_PIN, OUTPUT);
  pinMode(HW_SV2_HEATING_PIN, OUTPUT);
  
  digitalWrite(HW_PUMP_CO1_CTRL_PIN, LOW);
  digitalWrite(HW_PUMP_CWU1_CTRL_PIN, LOW);
  digitalWrite(HW_PUMP_CO2_CTRL_PIN, LOW);
  digitalWrite(HW_PUMP_CIRC_CTRL_PIN, LOW);
  digitalWrite(HW_FEEDER_CTRL_PIN, LOW);
  digitalWrite(HW_HEATER_CTRL_PIN, LOW);
  digitalWrite(HF_FLOW_SENSOR_POWER_PIN, LOW);
  digitalWrite(HW_SV2_HEATING_PIN, HW_SV2_PIN_ACTIVELOW ? HIGH : LOW);
  if (ALERT_STATE_PIN != 0) {
    pinMode(ALERT_STATE_PIN, OUTPUT); //low: alert state
    digitalWrite(ALERT_STATE_PIN, LOW);
  }
  
  if (HW_THERMOSTAT_PIN != 0) 
  {
    pinMode(HW_THERMOSTAT_PIN, g_CurrentConfig.EnableThermostat == 2 ? INPUT : INPUT_PULLUP);  
  }
  if (HW_THERMOSTAT_PIN_ALT != 0) 
  {
    pinMode(HW_THERMOSTAT_PIN_ALT, INPUT); 
  }
  attachInterrupt(digitalPinToInterrupt(HW_ZERO_DETECT_PIN), zeroCrossHandler2, RISING);  
  if (FURNACE_ENABLE_PIN != 0) {
    pinMode(FURNACE_ENABLE_PIN, INPUT_PULLUP); //pull up by default
    if (g_CurrentConfig.ExtFurnaceControlMode != 0) {
      int mode = g_CurrentConfig.ExtFurnaceControlMode % 2 == 0 ? INPUT_PULLUP : INPUT;
      pinMode(FURNACE_ENABLE_PIN, mode);
      Serial.print("pin piec:");
      Serial.print(FURNACE_ENABLE_PIN);
      Serial.print(" mode:");
      Serial.print(mode);
      Serial.println();
    }
  }
  
  if (g_CurrentConfig.ExtPumpControlMode != 0) {
    int mode = g_CurrentConfig.ExtPumpControlMode % 2 == 0 ? INPUT_PULLUP : INPUT;
    if (PUMP_CO_EXT_CTRL_PIN != 0) {
      pinMode(PUMP_CO_EXT_CTRL_PIN, mode);
      Serial.print("pin co: ");
      Serial.print(FURNACE_ENABLE_PIN);
      Serial.print(" mode:");
      Serial.print(mode);
      Serial.println();
    }
    if (PUMP_CW_EXT_CTRL_PIN != 0) {
      pinMode(PUMP_CW_EXT_CTRL_PIN, mode);
      Serial.print("pin cw: ");
      Serial.print(PUMP_CW_EXT_CTRL_PIN);
      Serial.print(" mode:");
      Serial.print(mode);
      Serial.println();
    }
    Serial.print("pullup:");
    Serial.print(INPUT_PULLUP);
    Serial.println();
  }
}


bool isThermostatOn() {
  if (HW_THERMOSTAT_PIN != 0 && (g_CurrentConfig.EnableThermostat & 0x01) != 0 && digitalReadFast(HW_THERMOSTAT_PIN) == LOW) return true;  //NO - low
  if (HW_THERMOSTAT_PIN_ALT != 0 && (g_CurrentConfig.EnableThermostat & 0x02) != 0 && digitalReadFast(HW_THERMOSTAT_PIN_ALT) == HIGH) return true; //NC
  return false;
}

uint8_t getCycleLengthForBlowerPower(uint8_t power) {
  return g_DeviceConfig.DefaultBlowerCycle;
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

int8_t getBlowerPowerCorrection();
void   setBlowerPowerCorrection();


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

/*
volatile unsigned PulseTime = 0;
ISR(TIMER1_CAPT_vect)
{
  static unsigned RisingEdgeTime = 0;
  static unsigned FallingEdgeTime = 0;
  
  // Which edge is armed?
  if (TCCR1B & (1 << ICES1))
  {
    // Rising Edge
    RisingEdgeTime = ICR1;
    TCCR1B &= ~(1 << ICES1); // Switch to Falling Edge
  }
  else
  {
    // Falling Edge
    FallingEdgeTime = ICR1;
    TCCR1B |= (1 << ICES1); // Switch to Rising Edge
    PulseTime = FallingEdgeTime - RisingEdgeTime;
  }
}


void loop() {
  unsigned pulseTime = 0;
  noInterrupts();
  pulseTime = PulseTime;
  PulseTime = 0;
  interrupts();


  if (pulseTime)
  {
    Serial.println(pulseTime);
  }
}
*/

void initializeFlowMeter() {
#ifdef HW_FLOW_SENSOR_INPUT_PIN
  pinMode(HW_FLOW_SENSOR_INPUT_PIN, INPUT);
  Serial.print("Flow sensor in V:");
  Serial.println(HW_FLOW_SENSOR_INPUT_PIN);
#endif

#ifdef HW_FLOW_SENSOR_PULSE_PIN
  

#endif
  
}


float getCurrentFlowRate() {
#ifdef HW_FLOW_SENSOR_INPUT_PIN
  int x = analogRead(HW_FLOW_SENSOR_INPUT_PIN);
  return (float) x;
#endif
#ifdef HW_FLOW_SENSOR_PULSE_PIN

#endif
  return 0;
}
