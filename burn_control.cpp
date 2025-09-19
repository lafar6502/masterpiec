#include <arduino.h>
#include <assert.h>
#include "burn_control.h"
#include "boiler_control.h"
#include "hwsetup.h"
#include "piec_sensors.h"
#include "ui_handler.h"
#include "varholder.h"
#include <MD_DS1307.h>
#include <pid.h>

#define MAX_TEMP 90
#define FIRESTART_STABILIZE_TIME 30000



float g_TargetTemp = 0.1; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
float g_TempCO = 0.0;
float g_TempCWU = 0.0; 
float g_TempPowrot = 0.0;  //akt. temp. powrotu
float g_TempSpaliny = 0.0; //akt. temp. spalin
float g_TempFeeder = 0.1;
float g_TempBurner = 0;
float g_InitialTempCO = 0;
float g_InitialTempExh = 0;
float g_AirFlow = 0; //air flow measurement
volatile uint8_t g_AirFlowNormal = 0;
uint8_t g_TargetFlow; //
TSTATE g_BurnState = STATE_UNDEFINED;  //aktualny stan grzania
TSTATE g_ManualState = STATE_UNDEFINED; //wymuszony ręcznie stan (STATE_UNDEFINED: brak wymuszenia)
CWSTATE g_CWState = CWSTATE_OK; //current cw status
HEATNEED g_needHeat = NEED_HEAT_NONE; //0, 1 or 2
HEATNEED g_initialNeedHeat = NEED_HEAT_NONE; //heat needs at the beginning of current state
uint16_t g_burnCycleNum = 0; //number of burning cycles in current state
bool   g_HomeThermostatOn = true;  //true - termostat pokojowy kazał zaprzestać grzania
bool g_overrideBurning = false; //set to true to have system think fire is started
float g_TempZewn = 0.0; //aktualna temp. zewn
char* g_Alarm;
unsigned long g_P1Time = 0;
unsigned long g_P2Time = 0;
unsigned long g_P0Time = 0;

float lastCOTemperatures[21];
float lastExhaustTemperatures[21]; //every 30 sec => 10 minutes
float lastFlows[11];
uint8_t g_furnaceEnabled = 1;
uint8_t g_coPumpOverride = 0;
uint8_t g_cwuPumpOverride = 0;
unsigned long g_cwuOverrideOffTime = 0; //planned cwu override OFF time (delay..)

epid_t g_flow_pid_ctx;

CircularBuffer<float> g_lastCOReads(lastCOTemperatures, sizeof(lastCOTemperatures)/sizeof(float));
CircularBuffer<float> g_lastExhaustReads(lastExhaustTemperatures, sizeof(lastExhaustTemperatures)/sizeof(float));
CircularBuffer<float> g_lastFlows(lastFlows, sizeof(lastFlows)/sizeof(float));
//czas wejscia w bieżący stan, ms
unsigned long g_CurStateStart = 0;
//float  g_CurStateStartTempCO = 0; //temp pieca w momencie wejscia w bież. stan.
uint8_t g_BurnCyclesBelowMinTemp = 0; //number of burn cycles with g_TempCO below minimum pump temperature (for detecting extinction of fire)
unsigned long g_CurBurnCycleStart = 0; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia (ten podajnik+nadmuch)


TSTATE getInitialState() {
  if (g_CurrentConfig.FireStartMode == FIRESTART_MODE_DISABLED) return STATE_P0;
  if (g_CurrentConfig.FireStartMode == FIRESTART_MODE_JUSTSTOP) return STATE_FIRESTART;
  if (g_CurrentConfig.FireStartMode == FIRESTART_MODE_STARTSTOP) {
    if (g_needHeat != NEED_HEAT_NONE) return STATE_FIRESTART;
    return STATE_OFF;
  }
  return STATE_FIRESTART;
}

#define EPID_KP  2.0f
#define EPID_KI  0.15f
#define EPID_KD  0.1f
#define PID_LIM_MIN 0.0f /* Limit for PWM. */
#define PID_LIM_MAX 255.0f /* Limit for PWM. */
#define DEADBAND 0.02f /* Off==0 */

void initializeAirflowPid() {
  epid_info_t epid_err = epid_init(&g_flow_pid_ctx,
        g_AirFlowNormal, g_AirFlowNormal, g_TargetFlow,
        EPID_KP, EPID_KI, EPID_KD);

    if (epid_err != EPID_ERR_NONE) {
        Serial.print("\n\n** ERROR: epid_err != EPID_ERR_NONE **\n\n");
    }
  
}

void initializeBurningLoop() {  
  g_TargetTemp = g_CurrentConfig.TCO;
  g_HomeThermostatOn = true;
  initializeAirflowPid();
  TSTATE startState = getInitialState();
  forceState(startState);
}


void setAlarm(const char* txt) {
  if (txt != NULL) g_Alarm = txt;
  forceState(STATE_ALARM);
  
}

float g_dT60; //1-minute temp delta
float g_dTl3; //last 3 readings diff
float g_dTExh; //1-min temp delta for exhaust


const uint8_t RatioCoeffs[] = {6,4,3,2,2,1,1,1};

float CalcIncreaseRatio(const CircularBuffer<float>& buf, uint8_t numSamples) {
  uint8_t c = buf.GetCount()-1;
  if (c > numSamples) c = numSamples;
  if (c > sizeof(RatioCoeffs)/sizeof(uint8_t)) c = sizeof(RatioCoeffs)/sizeof(uint8_t);
  if (c == 0) return 0.0f;
  uint8_t coeffs = 0;
  float sum = 0.0f;
  float curVal = *buf.GetAt(-1);
  for (int i=0; i<c;i++) {
    coeffs += RatioCoeffs[i];
    float v = curVal - *buf.GetAt(-(i+2));
    v *= RatioCoeffs[i];
    v /= (i+1);
    sum += v;
  }
  return sum / coeffs;
}

float CalcIncreaseRatio2(const CircularBuffer<float>& buf, uint8_t numSamples) {
  uint8_t c = buf.GetCount()-1;
  if (c > numSamples) c = numSamples;
  if (c > sizeof(RatioCoeffs)/sizeof(uint8_t)) c = sizeof(RatioCoeffs)/sizeof(uint8_t);
  if (c == 0) return 0.0f;
  uint8_t coeffs = 0;
  float sum = 0.0f;
  float curVal = *buf.GetAt(-1);
  for (int i=0; i<c;i++) {
    coeffs += RatioCoeffs[i];
    float v = curVal - *buf.GetAt(-(i+2));
    v *= RatioCoeffs[i];
    v /= (i+1);
    sum += v;
  }
  return sum / coeffs;
}


#define AVG_CNT 4
float _tempCoAvg[AVG_CNT];
float _tempExhAvg[AVG_CNT];
uint16_t _avgIdx = 0;

#define TEMP_HISTORY_SAMPLE_TIME_MS 30000

void processSensorValues() {
  unsigned long ms = millis();
  static unsigned long prevMs = 0;
  
  float f1 = getLastDallasValue(TSENS_BOILER);
  float f2 = getLastThermocoupleValue(T2SENS_EXHAUST);
  _tempCoAvg[_avgIdx % AVG_CNT] = f1;
  _tempExhAvg[_avgIdx % AVG_CNT] = f2;
  _avgIdx++;

  g_TempCO = f1;
  g_TempSpaliny = f2;
  
  uint16_t n = _avgIdx >= AVG_CNT ? AVG_CNT : _avgIdx;
  f1 = 0; f2 = 0;
  if (n > 0) {
    for(uint16_t i =0; i<n; i++) {
      f1 += _tempCoAvg[i];
      f2 += _tempExhAvg[i];
    } 
    g_TempCO = f1 / n;
    g_TempSpaliny = f2 / n; 
  }
  
  g_TempCWU = getLastDallasValue(TSENS_CWU);
  g_TempPowrot = getLastDallasValue(TSENS_RETURN);
  g_TempFeeder = getLastDallasValue(TSENS_FEEDER);
  g_TempZewn = getLastDallasValue(TSENS_EXTERNAL);
  g_TempBurner = getLastThermocoupleValue(T2SENS_BURNER);
  g_AirFlow = getCurrentFlowRate();
  if (g_CurrentConfig.EnableThermostat) 
  {
    g_HomeThermostatOn = isThermostatOn() && g_furnaceEnabled;
  }
  if (ms - prevMs >= TEMP_HISTORY_SAMPLE_TIME_MS) {
    prevMs = ms;
    //float ts = g_TempSpaliny < g_TempCO - EXHAUST_TEMP_DELTA_BELOW_CO ? g_TempCO - EXHAUST_TEMP_DELTA_BELOW_CO : g_TempSpaliny;
    g_lastExhaustReads.Enqueue(g_TempSpaliny);
    g_lastCOReads.Enqueue(g_TempCO);
    Serial.print("Q:");
    Serial.println(g_lastCOReads.GetCount());
  }
  g_lastFlows.Enqueue(g_AirFlow);
  n = g_lastFlows.GetCount();
  float f0 = 0.0;
  for (int i=0; i<n; i++) {
    f0 += *g_lastFlows.GetAt(i);
  }
  f0 /= n;
  f1 = (float) g_DeviceConfig.AirFlowCoeff * 4.0 + 3.0;
  g_AirFlowNormal = (uint8_t) ((f0 * 255.0) / f1);

  g_dTExh = CalcIncreaseRatio(g_lastExhaustReads, 4) * 2.0;
  g_dTl3 = CalcIncreaseRatio(g_lastCOReads, 4) * 2.0;
  g_dT60 = CalcIncreaseRatio(g_lastCOReads, 8) * 2.0;
}


///calculate delta (reduction or increase) to blower power 
///we run this adjustment in cycles (measure, adjust, measure, adjust) of xxx seconds
int8_t calculateBlowerPowerAdjustment(uint8_t desiredFlow, uint8_t currentFlow, uint8_t currentBlowerPower) {
  //if less than 1% difference of flow -dont do anything
  if (desiredFlow == 0) return -currentBlowerPower;
  float diff = (float) (desiredFlow - currentFlow) / (float) desiredFlow;
  
  if (abs(diff) <= 0.03) return 0;
  int8_t adj = diff * currentBlowerPower;
  if (adj > 0) {
    if (currentBlowerPower + adj < currentBlowerPower) adj = 255 - currentBlowerPower;
  }
  else {
    if (currentBlowerPower + adj > currentBlowerPower) adj = -currentBlowerPower;
  }
  /*Serial.print("adj diff:");
  Serial.print(diff);
  Serial.print(", cur:");
  Serial.print(currentFlow);
  Serial.print(",trg:");
  Serial.print(desiredFlow);
  
  Serial.print(", adj:");
  Serial.println(adj);*/
  return adj;
}


void maintainDesiredFlow1() {
  static unsigned long lastRun = 0L;
  unsigned long t = millis();
  if (t - lastRun < 5000) return;
  lastRun = t;
  if (g_TargetFlow == 0) return;
  if (!getManualControlMode()) 
  {
    if (g_CurrentConfig.AirControlMode != AIRCONTROL_CORRECT1 && g_CurrentConfig.AirControlMode != AIRCONTROL_CORRECT2) return;
    if (g_BurnState != STATE_P0 && g_BurnState != STATE_P1 && g_BurnState != STATE_P2 && g_BurnState != STATE_FIRESTART && g_BurnState != STATE_REDUCE1 && g_BurnState != STATE_REDUCE2) return;
  };
  
  uint8_t pow = getCurrentBlowerPower();
  

  if (pow == 0 && getManualControlMode()) 
  {
    pow = g_TargetFlow;
    setBlowerPower(pow);
  }
  float setF = (float) g_TargetFlow;
  float actF = (float) g_AirFlowNormal;
  epid_pid_calc(&g_flow_pid_ctx, setF, actF); /* Calc PID terms values */

   
  Serial.print("PID flow trg:");
  Serial.print(setF);
  Serial.print(", cur:");
  Serial.print(actF);
  

    /* Apply deadband filter to `delta[k]`. */
    float deadband_delta = g_flow_pid_ctx.p_term + g_flow_pid_ctx.i_term + g_flow_pid_ctx.d_term;
    if (true || (deadband_delta != deadband_delta) || (fabsf(deadband_delta) >= DEADBAND)) {
        /* Compute new control signal output */
        epid_pid_sum(&g_flow_pid_ctx, PID_LIM_MIN, PID_LIM_MAX);
        Serial.print(", calc pwr:");
        Serial.print(g_flow_pid_ctx.y_out);
        setBlowerPower((uint8_t) g_flow_pid_ctx.y_out);
    }
    Serial.println();
}


bool isFlowTooHigh() {
  if (g_CurrentConfig.AirControlMode < AIRCONTROL_HITMISS0 || g_CurrentConfig.AirControlMode > AIRCONTROL_HITMISS3) return false;
  if (g_BurnState != STATE_P0 && g_BurnState != STATE_P1 && g_BurnState != STATE_P2 && g_BurnState != STATE_FIRESTART && g_BurnState != STATE_REDUCE1 && g_BurnState != STATE_REDUCE2) return;
  if (g_TargetFlow == 0) return false;
  return g_AirFlowNormal > g_TargetFlow + 1;
}

void maintainDesiredFlow() {
  static unsigned long lastRun = millis();
  unsigned long t = millis();
  if (t - lastRun < 4000) return;
  lastRun = t;
  if (g_TargetFlow == 0) return;
  if (!getManualControlMode()) 
  {
    if (g_CurrentConfig.AirControlMode != AIRCONTROL_CORRECT1 && g_CurrentConfig.AirControlMode != AIRCONTROL_CORRECT2) return;
    if (g_BurnState != STATE_P0 && g_BurnState != STATE_P1 && g_BurnState != STATE_P2 && g_BurnState != STATE_FIRESTART && g_BurnState != STATE_REDUCE1 && g_BurnState != STATE_REDUCE2) return;
  };
  
  int16_t cp = getCurrentBlowerPower();
  
  if (cp == 0 && getManualControlMode()) {
    cp = g_TargetFlow;
    setBlowerPower(cp);
  }
  float setF = (float) g_TargetFlow;
  float actF = (float) g_AirFlowNormal;
  int16_t corr = getBlowerPowerCorrection();
  float dev = (setF - actF) / setF;
  if (abs(dev) <= 0.04) { //less than 5%
    return;
  }
  
  int8_t d = dev > 0.0 ? 1 : -1;
  if (abs(dev) > 0.1) d *= 2;
  if (abs(dev) > 0.25) d *= 2;
  corr += d;
  if (corr > 127) corr = 127;
  if (corr < -127) corr = -127;
  //Serial.print("flow:");
  //Serial.print(g_AirFlowNormal);
  //Serial.print(", dev:");
  //Serial.print(dev);
  //Serial.print(", corr:");
  //Serial.print(corr);
  //Serial.println();
  setBlowerPowerCorrection(corr);
}

void circulationControlTask() {
  if (!isPumpEnabled(PUMP_CIRC)) return;
  
  
  if (g_CurrentConfig.CircCycleMin == 0 || g_CurrentConfig.CircWorkTimeS == 0) return;
  if (getManualControlMode()) return;
  
  uint16_t cmin = RTC.h * 60 + RTC.m;
  cmin = cmin % g_CurrentConfig.CircCycleMin;
  uint16_t secs = cmin * 60 + RTC.s;
  bool pumpOn = secs < g_CurrentConfig.CircWorkTimeS;
  bool zoneIn = false;
  
  if ((RTC.h >= 17 && RTC.h <= 21))
        zoneIn = true;
  else if ((RTC.h >= 6 && RTC.h <= 7))
        zoneIn = true;
  else if ((RTC.h >= 11 && RTC.h <= 12))
        zoneIn = true;
  
  pumpOn = zoneIn && pumpOn;
        
  if (pumpOn) {
    if (!isPumpOn(PUMP_CIRC)) Serial.println("Circ pump start");
    setPumpOn(PUMP_CIRC);
  } else {
    if (isPumpOn(PUMP_CIRC)) Serial.println("Circ pump stop");
    setPumpOff(PUMP_CIRC);
  }
};
/***
 * ---
 */
void burningProc() 
{
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
  if (g_BurnState != BURN_STATES[g_BurnState].State) {
    Serial.print(F("invalid burn st"));
    Serial.println(g_BurnState);
  }
  unsigned long t = millis();
  
  //1. check if we should change state
  for(int i=0; i < N_BURN_TRANSITIONS; i++)
  {
    if (BURN_TRANSITIONS[i].From == g_BurnState) 
    {
      if (getManualControlMode() && BURN_TRANSITIONS[i].To != STATE_ALARM) {
        continue;
      }
      if (BURN_TRANSITIONS[i].fCondition != NULL && BURN_TRANSITIONS[i].fCondition()) 
      {
        if (g_BurnState == STATE_P1 || g_BurnState == STATE_REDUCE1)
          g_P1Time += (t - g_CurStateStart);
        else if (g_BurnState == STATE_P2  || g_BurnState == STATE_REDUCE2)
          g_P2Time += (t - g_CurStateStart);
        else if (g_BurnState == STATE_P0) 
          g_P0Time += (t - g_CurStateStart);
          
        Serial.print(F("BS: trans "));
        Serial.print(i);
        if (g_BurnState == STATE_P1) {
          Serial.print(" tP1:");
          Serial.print(g_P1Time);
        } else if (g_BurnState == STATE_P2) {
          Serial.print(" tP2:");
          Serial.print(g_P2Time);
        }
        Serial.print(" ");
        Serial.print(BURN_STATES[BURN_TRANSITIONS[i].From].Code);
        Serial.print("->");
        Serial.println(BURN_STATES[BURN_TRANSITIONS[i].To].Code);
        if (BURN_TRANSITIONS[i].fAction != NULL) BURN_TRANSITIONS[i].fAction(i);
        
        //transition to new state
        g_BurnState = BURN_TRANSITIONS[i].To;
        assert(g_BurnState != STATE_UNDEFINED && g_BurnState < N_BURN_STATES);
        g_CurStateStart = t;
        g_initialNeedHeat = g_needHeat;
        g_CurBurnCycleStart = g_CurStateStart;
        g_InitialTempCO = g_TempCO;
        g_InitialTempExh = g_TempSpaliny;
        g_BurnCyclesBelowMinTemp = 0;
        if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(BURN_TRANSITIONS[i].From);
        return;    
      }
    }
  }

  if (BURN_STATES[g_BurnState].fLoop != NULL) {
    BURN_STATES[g_BurnState].fLoop();  
  }
}



void setManualControlMode(bool b)
{
  if (!b) {
	  g_ManualState = STATE_UNDEFINED;
    if (g_BurnState == STATE_STOP) {
      TSTATE startState = getInitialState();
      forceState(startState);
    }
  }
  else {
	  g_ManualState = STATE_STOP;
  	if (g_BurnState != STATE_STOP) {
  	  forceState(STATE_STOP);
  	}
  }
}

TSTATE getManualControlState()
{
  return g_ManualState;
}

void setManualControlState(TSTATE t) {
  g_ManualState = t;
  if (t == STATE_UNDEFINED) {
    //forceState(STATE_P0);
    setManualControlMode(false);
  }
  else {
    if (g_BurnState != g_ManualState) {
      forceState(g_ManualState);  
    }
  }
}

bool getManualControlMode()
{
	return g_ManualState != STATE_UNDEFINED;
}



float curStateMaxTempCO = 0;

//api - switch to state
void forceState(TSTATE st) {
  assert(st != STATE_UNDEFINED);
  if (st == g_BurnState) return;
  unsigned long t = millis();

  if (g_BurnState == STATE_P1 || g_BurnState == STATE_REDUCE1)
    g_P1Time += (t - g_CurStateStart);
  else if (g_BurnState == STATE_P2  || g_BurnState == STATE_REDUCE2)
    g_P2Time += (t - g_CurStateStart);
  else if (g_BurnState == STATE_P0) 
    g_P0Time += (t - g_CurStateStart);
    
  TSTATE old = g_BurnState;
  g_BurnState = st;
  g_CurStateStart = t;
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  g_BurnCyclesBelowMinTemp = 0;
  if (BURN_STATES[g_BurnState].fInitialize != NULL) BURN_STATES[g_BurnState].fInitialize(old);
  Serial.print("BS->");
  Serial.print(BURN_STATES[g_BurnState].Code);
  Serial.print(" tP1:");
  Serial.print(g_P1Time);
  Serial.print(" tP2:");
  Serial.println(g_P2Time);
}

void updatePumpStatus();
void handleHeatNeedStatus();

//API 
//to nasza procedura aktualizacji stanu hardware-u
//wolana cyklicznie.
void burnControlTask() {
  //read
  g_furnaceEnabled = 1;
  g_coPumpOverride = 0;
  //g_cwuPumpOverride = 0;
  int st;
  if (g_CurrentConfig.ExtFurnaceControlMode != 0 && FURNACE_ENABLE_PIN != 0) {
    st = digitalRead(FURNACE_ENABLE_PIN);
    g_furnaceEnabled = st == ((g_CurrentConfig.ExtFurnaceControlMode == 1 || g_CurrentConfig.ExtFurnaceControlMode == 4) ? HIGH : LOW);
  }
  
  if (g_CurrentConfig.ExtPumpControlMode != 0 && PUMP_CO_EXT_CTRL_PIN != 0) {
    //0 - none, 1 - high active, 2 - low active
    //3, 4: furnace enable pin is information if compressor is running.. so if pump co is not active

    st = digitalRead(PUMP_CO_EXT_CTRL_PIN);
    g_coPumpOverride = st == ((g_CurrentConfig.ExtPumpControlMode % 2) == 1 ? HIGH : LOW);
    if (g_coPumpOverride != 0) {
      g_cwuPumpOverride = 0;
    }
    else if (PUMP_CW_EXT_CTRL_PIN != 0) {
      
      st = digitalRead(PUMP_CW_EXT_CTRL_PIN); //if this is on, we enable cwu. otherwise - 

      uint8_t ovr = st == ((g_CurrentConfig.ExtPumpControlMode % 2) == 1 ? HIGH : LOW);
      
      if (ovr && g_CurrentConfig.ExtPumpControlMode > 2) {
        //we need to check if compressor is running too
        st = digitalRead(FURNACE_ENABLE_PIN);
        bool compressor = st == ((g_CurrentConfig.ExtFurnaceControlMode % 2) == 1 ? HIGH : LOW);
        if (!compressor) 
        {
          ovr = 0;
        }
      }
      if (g_CurrentConfig.ExtCWPumpOffDelay > 0) {
        unsigned long t0 = millis();
        if (ovr == 0) {
          if (g_cwuPumpOverride != 0) {
            long dif = t0 - g_cwuOverrideOffTime;
            if (dif >= g_CurrentConfig.ExtCWPumpOffDelay * 10000L) {
              g_cwuPumpOverride = 0;
              Serial.print(F("CW over end "));
              Serial.println(g_cwuOverrideOffTime);
              g_cwuOverrideOffTime = 0;
            }
            else {
              //do nothing.. delay
              Serial.println(dif);
            }
          }
        }
        else {
          if (g_cwuPumpOverride == 0) {
            Serial.print(F("CW over start"));
            Serial.println(t0);
          }
          g_cwuPumpOverride = ovr;
          g_cwuOverrideOffTime = t0;
        }  
      }
      else {
        g_cwuPumpOverride = ovr;
      }
    }
  }
  else {
    g_cwuPumpOverride = 0;
    g_coPumpOverride = 0;
  }
  processSensorValues();
  if (g_furnaceEnabled == 0) {
    g_needHeat = NEED_HEAT_NONE;
  }
  else {
    handleHeatNeedStatus();  
  }
  
  updatePumpStatus();
  burningProc();
  setSV2HeatingPin(g_BurnState == STATE_P1 || g_BurnState == STATE_P2 || g_BurnState == STATE_P0);
}




//inicjalizacja dla stanu grzania autom. P1 P2
void workStateInitialize(TSTATE prev) {
  assert(g_BurnState != STATE_UNDEFINED && g_BurnState != STATE_STOP && g_BurnState < N_BURN_STATES);
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  g_BurnCyclesBelowMinTemp = 0;
  curStateMaxTempCO = g_TempCO;
  g_burnCycleNum = 0;
  setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_DeviceConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
  if (g_CurrentConfig.AirControlMode != AIRCONTROL_NONE && g_CurrentConfig.BurnConfigs[g_BurnState].AirFlow > 0) {
    g_TargetFlow = g_CurrentConfig.BurnConfigs[g_BurnState].AirFlow;
  }
  Serial.print(F("Burn init, cycle: "));
  Serial.println(g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec);
  setHeater(false);
  g_InitialTempCO = g_TempCO;
  g_InitialTempExh = g_TempSpaliny;
}

//przejscie do stanu recznego
void stopStateInitialize(TSTATE prev) {
  assert(g_BurnState == STATE_STOP);
  setBlowerPower(0);
  g_TargetFlow = 0;
  setFeederOff();
  setHeater(false);
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  g_InitialTempCO = g_TempCO;
  g_InitialTempExh = g_TempSpaliny;
  g_overrideBurning = false;
}

///pętla palenia dla stanu pracy
//załączamy dmuchawę na ustaloną moc no i pilnujemy podajnika
void workStateBurnLoop() {
  assert(g_BurnState == STATE_P1 || g_BurnState == STATE_P2 || g_BurnState == STATE_FIRESTART);
  unsigned long tNow = millis();
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000L;
  //feeder time length
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 * (100L + g_CurrentConfig.FuelCorrection);
  
  if (tNow - g_CurBurnCycleStart < burnFeedLen) 
  {
    setFeederOn();
  }
  else 
  {
    setFeederOff();
  }
  if (g_TempCO > curStateMaxTempCO) curStateMaxTempCO = g_TempCO;
  if (tNow - g_CurBurnCycleStart >= burnCycleLen) 
  {
    g_CurBurnCycleStart = tNow; //next burn cycle
    setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_DeviceConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
    g_BurnCyclesBelowMinTemp = g_TempCO <= g_CurrentConfig.TMinPomp ? g_BurnCyclesBelowMinTemp + 1 : 0;
    g_burnCycleNum++;
    if (g_CurrentConfig.AirControlMode != AIRCONTROL_NONE && g_CurrentConfig.BurnConfigs[g_BurnState].AirFlow > 0) {
      g_TargetFlow = g_CurrentConfig.BurnConfigs[g_BurnState].AirFlow;
    }
  }
  if (g_BurnState != STATE_FIRESTART) setHeater(false);
  maintainDesiredFlow();
}

unsigned long _reductionStateEndMs = 0; //inside reduction state - this is the calculated end time. Outside (before reduction) - we put remaining P1 or P2 time there before going to reduction.

void firestartStateInit(TSTATE prev) {
	assert(g_BurnState == STATE_FIRESTART);
	g_CurStateStart = millis();
	g_initialNeedHeat = g_needHeat;
	g_CurBurnCycleStart = g_CurStateStart;  
  g_burnCycleNum = 0;
  g_BurnCyclesBelowMinTemp = 0;
  curStateMaxTempCO = g_TempCO;
  g_InitialTempCO = g_TempCO;
  g_InitialTempExh = g_TempSpaliny;
  g_overrideBurning = false;
  uint8_t bp = g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower;
  setBlowerPower(0);
  setBlowerPower(bp, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_DeviceConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
  if (g_CurrentConfig.AirControlMode != AIRCONTROL_NONE && g_CurrentConfig.BurnConfigs[g_BurnState].AirFlow > 0) {
    g_TargetFlow = g_CurrentConfig.BurnConfigs[g_BurnState].AirFlow;
  }
  if (bp > 0) {
    setHeater(true);
  }
  Serial.print(F("Firestart init, cycle: "));
  Serial.println(g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec);
}

void firestartStateLoop() {
	workStateBurnLoop();

  unsigned long tRun = millis() - g_CurStateStart;
  if (tRun < FIRESTART_STABILIZE_TIME) {
    g_InitialTempCO = g_TempCO;
    g_InitialTempExh = g_TempSpaliny;
  }
  else {
    if (g_TempCO < g_InitialTempCO) g_InitialTempCO = g_TempCO;
    if (g_TempSpaliny < g_InitialTempExh) g_InitialTempExh = g_TempSpaliny;
  }
  
  uint8_t bp = getCurrentBlowerPower();
  bool heater = bp > 0;
  if (heater && g_CurrentConfig.HeaterMaxRunTimeS != 0) {
    const unsigned int tCool = 30 * 1000;
    unsigned long t = millis() - g_CurStateStart;
    unsigned long ht = g_CurrentConfig.HeaterMaxRunTimeS * 1000L + tCool;
    unsigned long t2 = t % ht;
    heater = t2 <= ht - tCool;
    if (!heater) {

    }
    maintainDesiredFlow();
  }

  setHeater(heater);
  
}

void offStateInit(TSTATE prev) {
  assert(g_BurnState == STATE_OFF);
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  g_InitialTempCO = g_TempCO;
  g_InitialTempExh = g_TempSpaliny;
  g_TargetFlow = 0;
  setHeater(false);  
  setBlowerPower(0);
  setFeederOff();
}

void offStateLoop() {
  setHeater(false);
  setBlowerPower(0);
  setFeederOff();
}


void reductionStateInit(TSTATE prev) {
  assert(g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2);
  assert(prev == STATE_P1 || prev == STATE_P2);
  if (prev != STATE_P1 && prev != STATE_P2) {
    Serial.print(F("red: wrong state"));
    Serial.println(prev);
    prev = STATE_P1;
  }
  
  g_CurStateStart = millis();
  g_initialNeedHeat = g_needHeat;
  g_CurBurnCycleStart = g_CurStateStart;
  g_InitialTempCO = g_TempCO;
  g_InitialTempExh = g_TempSpaliny;
  unsigned long adj = _reductionStateEndMs; //remaining time from P2 or P1
  
  _reductionStateEndMs = (unsigned long) g_CurrentConfig.BurnConfigs[prev].CycleSec * 1000L;
  if (prev == STATE_P2) adj += ((unsigned long) g_CurrentConfig.ReductionP2ExtraTime * (unsigned long) g_CurrentConfig.BurnConfigs[prev].CycleSec * 10L); // * 1000 / 100;
  _reductionStateEndMs = g_CurStateStart + _reductionStateEndMs + adj; //
  setBlowerPower(g_CurrentConfig.BurnConfigs[prev].BlowerPower, g_CurrentConfig.BurnConfigs[prev].BlowerCycle == 0 ? g_DeviceConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[prev].BlowerCycle);
  if (g_CurrentConfig.AirControlMode != AIRCONTROL_NONE && g_CurrentConfig.BurnConfigs[prev].AirFlow > 0) {
    g_TargetFlow = g_CurrentConfig.BurnConfigs[prev].AirFlow;
  }
  setFeederOff();
  setHeater(false);
  Serial.print(F("red: cycle should end in "));
  Serial.print((_reductionStateEndMs - g_CurStateStart) / 1000.0);
  Serial.print(F(", extra time s:"));
  Serial.println(adj / 1000);
}

void reductionStateLoop() {
  assert(g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2);
  TSTATE prevState = g_BurnState == STATE_REDUCE1 ? STATE_P1 : STATE_P2;
  unsigned long tNow = millis();
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[prevState].CycleSec * 1000L;
  
  if (tNow > _reductionStateEndMs + 100) 
  {
    Serial.print(F("reduction should end by now:"));
    Serial.println(g_CurBurnCycleStart);
  }
  maintainDesiredFlow();
}



void podtrzymanieStateInitialize(TSTATE prev) {
  g_CurStateStart = millis();
  g_CurBurnCycleStart = g_CurStateStart;
  g_initialNeedHeat = g_needHeat;
  g_burnCycleNum = 0;
  g_InitialTempCO = g_TempCO;
  g_InitialTempExh = g_TempSpaliny;
  g_overrideBurning = false;
  setBlowerPower(0);
  g_TargetFlow = 0;
  setFeederOff();
  setHeater(false);
}

void podtrzymanieStateLoop() {
  assert(g_BurnState == STATE_P0);
  unsigned long tNow = millis();
  static uint8_t cycleNum = 0;
  unsigned long burnCycleLen = (unsigned long) g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec * 1000L;
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[STATE_P0].FuelSecT10 * (100L + g_CurrentConfig.FuelCorrection);
  unsigned long blowerStart = burnCycleLen - (unsigned long) g_CurrentConfig.P0BlowerTime * 1000L;
  
  if (tNow - g_CurBurnCycleStart >= blowerStart) 
  {
    setBlowerPower(g_CurrentConfig.BurnConfigs[g_BurnState].BlowerPower, g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_DeviceConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle);
  } 
  else 
  {
    setBlowerPower(0);
  }

  if (tNow - g_CurBurnCycleStart >=  blowerStart && tNow - g_CurBurnCycleStart <= blowerStart + burnFeedLen && (cycleNum % g_CurrentConfig.P0FuelFreq) == 0) 
  {
    setFeederOn();
  } 
  else 
  {
    setFeederOff();
  }
  
  if (tNow - g_CurBurnCycleStart >= burnCycleLen) 
  {
    g_CurBurnCycleStart = tNow;
    cycleNum++;
    g_burnCycleNum++;
  }
  setHeater(false);
}

void manualStateLoop() {
  if (isHeaterOn()) {
    unsigned long t = getHeaterRunningTimeMs();
    if (g_CurrentConfig.HeaterMaxRunTimeS != 0 && t > g_CurrentConfig.HeaterMaxRunTimeS * 1000) {
      setHeater(false);
    }
  }

  maintainDesiredFlow();
}


void handleHeatNeedStatus() {

  if (g_CurrentConfig.SummerMode == 2) 
  {  
    g_CWState = CWSTATE_OK;
    g_TargetTemp = g_CurrentConfig.TCO;
  }
  else 
  {
    if (g_CWState == CWSTATE_OK) {
        if (g_TempCWU < g_CurrentConfig.TCWU - g_CurrentConfig.THistCwu) {
          g_CWState = CWSTATE_HEAT; //start heating cwu
          g_TargetTemp = max(g_CurrentConfig.TCO, g_CurrentConfig.TCWU + g_CurrentConfig.TDeltaCWU);
          Serial.print(F("CWU heat - adjusted target temp to "));
          Serial.println(g_TargetTemp);
        } else g_TargetTemp = g_CurrentConfig.TCO;
    }
    else if (g_CWState == CWSTATE_HEAT) {
      if (g_TempCWU >= g_CurrentConfig.TCWU) {
        g_CWState = CWSTATE_OK;
        g_TargetTemp = g_CurrentConfig.TCO;
        Serial.print(F("CWU ready - adjusted target temp to "));
        Serial.println(g_TargetTemp);
      } else {
        g_TargetTemp = max(g_CurrentConfig.TCO, g_CurrentConfig.TCWU + g_CurrentConfig.TDeltaCWU);
      }
    }
    else assert(false);
  }
  
  HEATNEED prev = g_needHeat;
  g_needHeat = NEED_HEAT_NONE;
  if (g_CurrentConfig.SummerMode != 2) 
  {
    if (g_CWState == CWSTATE_HEAT) g_needHeat = NEED_HEAT_CWU;
    if (g_CurrentConfig.SummerMode == 0 && g_needHeat == NEED_HEAT_NONE)
    {
      if (!g_CurrentConfig.EnableThermostat || g_HomeThermostatOn) g_needHeat = NEED_HEAT_CO;
    }  
  }
  if (g_needHeat != prev) {
    Serial.print(F("Heat needs changed:"));
    Serial.println(g_needHeat);
  }
}

uint8_t cond_needCooling(); //below
bool cond_willFallBelowHysteresisSoon(float adj = 0);
//
// which pumps and when
// cwu heating needed -> turn on cwu pump if current temp is above min pump temp and above cwu temp + delta
//
void updatePumpStatus() {
  if (g_TempCO >= MAX_TEMP) { //this should be the only time two pumps are allowed to work together
    if (isPumpEnabled(PUMP_CWU1)) setPumpOn(PUMP_CWU1);
    setPumpOn(PUMP_CO1);
    return;
  }
  if (getManualControlMode()) return; //all below only in automatic mode.
  
  if (g_coPumpOverride != 0) {
    setPumpOn(PUMP_CO1);
  }
  if (g_cwuPumpOverride != 0) {
    setPumpOn(PUMP_CWU1);
  }
  
  if (g_BurnState == STATE_FIRESTART) {
    if (!g_coPumpOverride) setPumpOff(PUMP_CO1);
    if (!g_cwuPumpOverride) setPumpOff(PUMP_CWU1);
    return;
  }
  if (g_TempCO < g_CurrentConfig.TMinPomp) {
    if (!g_coPumpOverride) setPumpOff(PUMP_CO1);
    if (!g_cwuPumpOverride) setPumpOff(PUMP_CWU1);
    return;
  }
  
  if (g_needHeat == NEED_HEAT_CWU) {
    uint8_t minTemp = max(g_CurrentConfig.TMinPomp, g_TempCWU + g_CurrentConfig.TDeltaCWU);
    if (g_TempCO >= minTemp) {
      setPumpOn(PUMP_CWU1);
    } 
    else {
      if (!g_cwuPumpOverride) setPumpOff(PUMP_CWU1);
      //Serial.println(F("too low to heat cwu"));
    }
    if (!g_coPumpOverride) setPumpOff(PUMP_CO1);
    return;
  }
  
  bool pco = isPumpOn(PUMP_CO1);
  if (g_needHeat == NEED_HEAT_CO) { //co pump on - thermostat on or thermostat disabled (co pump always on)
    if (!cond_willFallBelowHysteresisSoon(pco ? -0.1 : 0.1)) {
      setPumpOn(PUMP_CO1);  
    } else {
      if (!g_coPumpOverride) setPumpOff(PUMP_CO1);  
    }
    if (!g_cwuPumpOverride) setPumpOff(PUMP_CWU1); //just to be sure
    return;
  }
  
  //no need to heat from now...
  uint8_t cl = cond_needCooling();
  
  bool cw = g_CurrentConfig.SummerMode == 1 && isPumpEnabled(PUMP_CWU1) && isDallasEnabled(TSENS_CWU) && cl == 2;
  
  if (cl != 0 && !g_coPumpOverride && !g_cwuPumpOverride) {
    setPumpOn(cw ? PUMP_CWU1 : PUMP_CO1);
    setPumpOff(cw ? PUMP_CO1 : PUMP_CWU1);
    return;
  }
  if (!g_cwuPumpOverride) setPumpOff(PUMP_CWU1);
  if (!g_coPumpOverride) setPumpOff(PUMP_CO1);
}


bool isAlarm_HardwareProblem() {
  if (!isDallasEnabled(TSENS_BOILER)) {
    g_Alarm = "Czujnik temp CO";
    return true;
  }
  if (g_CurrentConfig.FeederTempLimit != 0 && !isDallasEnabled(TSENS_FEEDER)) {
    g_Alarm = "Czujnik podajnika";
    return true;
  }
  return false;
}

bool isAlarm_Overheat() {
  if (g_TempCO > MAX_TEMP) {
    g_Alarm = "ZA GORACO";
    return true;
  }
  return false;
}

bool isAlarm_feederOnFire() {
  if (g_CurrentConfig.FeederTempLimit > 0 && g_TempFeeder > g_CurrentConfig.FeederTempLimit) {
    g_Alarm = "Temp. podajnika";
    return true;
  }
  return false;
}

#define NMIN 6

int firestartIsBurningCheck() {
  bool isFirestart =  g_BurnState == STATE_FIRESTART;
  unsigned long tRun = millis() - g_CurStateStart;
  if (isFirestart && tRun < FIRESTART_STABILIZE_TIME) return 0;

  
  float fm = g_CurrentConfig.FireDetExhDt10 / 10.0;  //exhaust temp over CO temp
  float ctd = g_CurrentConfig.FireDetExhIncrD10 / 10.0; //exhaust temp increase by degrees
  float cte = g_CurrentConfig.FireDetCOIncr10 / 10.0;  //CO temp increase by degrees

  float exhStart = g_InitialTempExh < g_InitialTempCO - EXHAUST_TEMP_DELTA_BELOW_CO ? g_InitialTempCO - EXHAUST_TEMP_DELTA_BELOW_CO : g_InitialTempExh; //jesli temp spalin jest ponizej temp kotla to znaczy ze komin sie wychlodzil bardziej niz kociol - nieprawidl. wartosc
  
  float d = g_TempSpaliny  - exhStart;
  float e = g_TempCO - g_InitialTempCO;
  float f = g_TempSpaliny - g_TempCO;
  bool cond1 = g_TempSpaliny >= g_TempCO - EXHAUST_TEMP_DELTA_BELOW_CO;
  bool cond3 = tRun > 2UL*FIRESTART_STABILIZE_TIME;
  if (isDebugTime()) {
    Serial.print("FC T:");
    Serial.print(tRun/1000);
    Serial.print(",d:");
    Serial.print(d);
    Serial.print(",ctd:");
    Serial.print(ctd);
    Serial.print(",C1:");
    Serial.print(cond1);
    Serial.print(",e:");
    Serial.print(e);
    Serial.print(",cte:");
    Serial.print(cte);
    Serial.print(",f:");
    Serial.print(f);
    Serial.print(",fm:");
    Serial.print(fm);
    Serial.print(",C3:");
    Serial.print(cond3);
    Serial.println();
  }
  if (ctd > 0) {
    if (cond1) { //exh temp high enough
      if (d >= ctd) return 1;
      
      if (cond3 && exhStart > g_InitialTempCO - EXHAUST_TEMP_DELTA_BELOW_CO && g_dTExh > 0.5 && d + g_dTExh > ctd) return 2;  
      if (g_lastExhaustReads.GetCount() >= NMIN && tRun >= NMIN * TEMP_HISTORY_SAMPLE_TIME_MS) {
        float dif = *g_lastExhaustReads.GetAt(-1) - *g_lastExhaustReads.GetAt(-NMIN);
        if (dif > ctd) return 3;
      }
    }
    
  }

  if (cte > 0 && e >= cte) return 4;
  
  
  if (fm > 0 && cond3) { 
      if (f >= fm) {
        return 5;
      }
  }

  
  return false;
}
//detect if fire has started in automatic fire start mode
bool cond_firestartIsBurning() {

  int n = firestartIsBurningCheck();
  if (isDebugTime()) {
    Serial.print("FC:");
    Serial.println(n);
  }
  return n != 0;
}

bool cond_noHeating() {
  if (g_BurnState != STATE_P1 && g_BurnState != STATE_P2 && g_BurnState != STATE_FIRESTART) return false;
  if (cond_firestartIsBurning()) return false;
  //in P1 temperature is expected to go down, both exhaust and boiler water, so we cant use temp growth
  if (g_CurrentConfig.NoHeatAlarmCycles == 0) return false; // no detection
  if (g_TempCO > g_CurrentConfig.TMinPomp) return false; //if above the min temp we dont detect 'fire extinct'
  if (g_BurnCyclesBelowMinTemp > g_CurrentConfig.NoHeatAlarmCycles)
  {
    //g_Alarm = "Wygaslo";
    return true;
  }
  return false;
}

//no heating - fire went out, fuel run out, other
//in P1 temp is supposed to drop and so exhaust temp will drop as well, even if the fire is burning all the time
//so how we detect? 
bool isAlarm_NoHeating() {
  if (g_CurrentConfig.FireStartMode == FIRESTART_MODE_STARTSTOP) return false;
  
  if (cond_noHeating()) {
    g_Alarm = "Wygaslo";
    return true;
  }
  return false;
}

bool cond_noHeating_Firestart() {
  if (g_CurrentConfig.FireStartMode != FIRESTART_MODE_STARTSTOP) return false;
  return cond_noHeating();
}

bool isAlarm_Any() {
  return isAlarm_HardwareProblem() || isAlarm_Overheat() || isAlarm_NoHeating() || isAlarm_feederOnFire();
}


void alarmStateInitialize(TSTATE prev) {
  setHeater(0);
  changeUIState('0');
  setBlowerPower(0);
}

// kiedy przechodzimy z P0 do P2
// gdy temp. wody CO spadnie poniżej zadanej
// 



bool cond_B_belowHysteresisAndNeedHeat() {
  if (g_furnaceEnabled == 0) return false;
  if (g_needHeat != NEED_HEAT_NONE && g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return true;
  if (g_needHeat == NEED_HEAT_CWU) {
    //we're below min temp to heat the cwu
    return (g_TempCO < g_TempCWU + g_CurrentConfig.TDeltaCWU);
  }
  return false;  
}


//variant #2 of control 
bool cond_C_belowHysteresisAndNoNeedToHeat() {
  if (g_needHeat == NEED_HEAT_NONE and g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return true;
  return false;
}

//we're below hysteresis, but auto stop is not allowed
bool cond_C_belowHysteresisAndNoNeedToHeat_NoAutoStop() {
  if (g_CurrentConfig.FireStartMode != FIRESTART_MODE_DISABLED) return false;
  if (g_needHeat == NEED_HEAT_NONE and g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return true;
  return false;
}


bool cond_D_belowTargetTempAndNeedHeat() {
  if (g_furnaceEnabled == 0) return false;
  if (g_needHeat != NEED_HEAT_NONE && g_TempCO < g_TargetTemp - 0.5) return true;
  return false;
}

bool cond_D_belowTargetTempAndNeedHeatAndAutoAllowed() {
  if (g_CurrentConfig.FireStartMode != FIRESTART_MODE_STARTSTOP) return false;
  return cond_D_belowTargetTempAndNeedHeat();
  
}

bool cond_cycleEnded() {
  return _reductionStateEndMs <= millis();
}

bool cond_targetTempReached() {
  return g_TempCO >= g_TargetTemp;
}

bool cond_willReachTargetSoon() {
  if (g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return false;
  if (g_dTl3 < (g_needHeat == NEED_HEAT_NONE ? 0.2 : 0.5)) return false;
  if (g_TempCO + 2.5 * g_dTl3 > g_TargetTemp) return true; //we will be there in max 2.5 minutes
  return false;
}
//we're above hysteresis but the need for heat has suddenly disappeared (thermostat?) - so reduce from P2
bool cond_suddenHeatOffAndAboveHysteresis() {
  if (g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return false;
  if (g_needHeat == NEED_HEAT_NONE && g_initialNeedHeat != NEED_HEAT_NONE) return true;
  return false;
}

//we dont need heating and temperature is above the hysteresis low limit
bool cond_noNeedToHeatAndAboveHysteresis() {
  if (g_TempCO < g_TargetTemp - g_CurrentConfig.THistCO) return false;
  if (g_needHeat == NEED_HEAT_NONE) return true;
  return false;
}

bool cond_willFallBelowHysteresisSoon(float adj = 0) {
  if (g_needHeat == NEED_HEAT_NONE) return false;
  if (g_dTl3 > -0.5) return false;
  if (g_TempCO + 2 * g_dTl3 < g_TargetTemp - g_CurrentConfig.THistCO + adj) return true;
  return false;
}

bool cond_boilerOverheated() {
  return g_TempCO >= g_TargetTemp + g_CurrentConfig.TDeltaCO || g_TempCO >= MAX_TEMP;
}

uint8_t _coolState = 0; //1-cool, 2-pause
unsigned long _coolTs = 0;

bool cond_canCoolWithCWU() {
  if (!isPumpEnabled(PUMP_CWU1)) return false;
  if (g_TempCO  <= g_TempCWU) return false;
  if (g_TempCWU >= g_CurrentConfig.TCWU + 2 * g_CurrentConfig.THistCwu) return false; //cwu too hot
  return true;
}
//temp too high, need cooling by running co or cwu pump
//0 = no need to cool, 1 - should cool, 2 - should cool, possibly with CWU
uint8_t cond_needCooling() {
  static uint8_t _cwCnt = 0;
  
  bool cw = ((_cwCnt % 2) == 0 || g_CurrentConfig.SummerMode != 0) && cond_canCoolWithCWU();
  
  if (g_TempCO >= MAX_TEMP) {_coolState = 0; return cw ? 2 : 1;} //always
  
  bool hot = g_CurrentConfig.CooloffMode == COOLOFF_NONE ? false : g_CurrentConfig.CooloffMode == COOLOFF_LOWER ? (g_TempCO > g_TargetTemp + (_coolState == 1 ? 0.1 : 0.4)) : (g_TempCO > g_TargetTemp + g_CurrentConfig.TDeltaCO);

 
  unsigned long t = millis();
  
  if (!hot && g_CurrentConfig.FireStartMode != FIRESTART_MODE_DISABLED  && g_BurnState == STATE_OFF && g_CurrentConfig.CooloffMode != COOLOFF_NONE) {
    //standby, we can go lower
    if (cw) {
      hot = g_TempCO  > g_TempCWU + (g_CurrentConfig.TDeltaCWU / 4.0);

      //g_TempPowrot = getLastDallasValue(TSENS_RETURN)
      if (!hot && _coolState == 1 && (t - _coolTs) > 30000L && g_TempPowrot > 0 && g_TempCO >= g_TempCWU && g_TempPowrot < (g_TempCO - g_CurrentConfig.TDeltaCWU / 2.0) && isDallasEnabled(TSENS_RETURN)) {
        hot = true; //boiler reached the temp, but return temp still lower and boiler can be heated up a little
      }
    }
    else if (g_CurrentConfig.SummerMode == 0) {
      hot = g_CurrentConfig.CooloffMode == COOLOFF_LOWER ? g_TempCO >= g_CurrentConfig.TMinPomp : g_TempCO >= g_TargetTemp - g_CurrentConfig.THistCO;  
    }
  }
  
  
  if (getManualControlMode() || !hot || g_BurnState == STATE_ALARM) {
    _coolState = 0;
    return 0;
  }

  //hot! hot! 
  
  switch(_coolState) {
    case 1:
      if ((t - _coolTs) > (unsigned long) g_CurrentConfig.CooloffTimeM10 * 6L * 1000) {//pause
        _coolState = 2;
        _coolTs = t;
        _cwCnt++;
        Serial.println(F("Cool pause"));
        return 0;
      }
      return cw ? 2 : 1;
    case 2:
      if ((t - _coolTs) > (unsigned long) g_CurrentConfig.CooloffPauseM10 * 6L * 1000) {//pause
        _coolState = 1;
        _coolTs = t;
        Serial.println(F("Cool resume"));
        return cw ? 2 : 1;
      }
      return 0;
      break;
    default:
      _coolState = 1;
      _coolTs = t;
      Serial.println(F("Cool start"));
      return cw ? 2 : 1;
  }
}


uint8_t needHeatingNow() {
  return g_needHeat;
}
//heating is needed and temp is below the target
bool cond_A_needSuddenHeatAndBelowTargetTemp() {
  if (g_furnaceEnabled == 0) return false;
  if (g_TempCO >= g_TargetTemp) return false;
  return g_needHeat != NEED_HEAT_NONE && g_initialNeedHeat == NEED_HEAT_NONE;
}

void alertStateLoop() {
  static unsigned long _feederStart = 0;
  unsigned long tNow = millis();
  setHeater(0);
  setBlowerPower(0);
  unsigned long burnCycleLen = 60 * 4 * 1000; //4 min
  uint8_t sig = 1;
  if (isAlarm_feederOnFire()) {
    if (tNow - g_CurBurnCycleStart < burnCycleLen) 
    {
      setFeederOn();
      sig = 0;
    }
    else 
    {
      setFeederOff();
    } 
  }
  if (ALERT_STATE_PIN != 0) {
    digitalWrite(ALERT_STATE_PIN, sig ? HIGH : LOW);
  }
}


bool cond_targetTempReachedAndHeatingNotNeeded() {
  return g_TempCO >= g_TargetTemp && g_needHeat == NEED_HEAT_NONE;
}

//we're in auto stop mode and we're above min temp
bool cond_autoFire_minTempReachedAndHeatingNotNeeded() {
  if (g_CurrentConfig.FireStartMode == FIRESTART_MODE_DISABLED) return false;
  return g_needHeat == NEED_HEAT_NONE && g_TempCO > g_CurrentConfig.TMinPomp; 
}

//verify if fire is no longer burning
//this can only be detected in work cycles
bool cond_fireIsNotBurning() {
  if (g_lastExhaustReads.GetCount() <= 5) return false;

  return false;
}

bool cond_fireBurningAndBelowTargetTemp() {
  return cond_D_belowTargetTempAndNeedHeat() && cond_firestartIsBurning();
}

bool cond_firestartTimeout() {
  if (g_BurnState != STATE_FIRESTART) return false;
  if (g_burnCycleNum > g_CurrentConfig.NumFireStartCycles) {
    g_Alarm = "Rozpal";
    return true;
  }
  return false;
}

bool cond_firestartOverride() {
  return g_overrideBurning;
}

//check: we are in P0 and we need to shut the burner down
bool cond_shouldGoToStandby() {
  if (g_CurrentConfig.FireStartMode != FIRESTART_MODE_JUSTSTOP && g_CurrentConfig.FireStartMode != FIRESTART_MODE_STARTSTOP) return false;
  if (g_needHeat != NEED_HEAT_NONE) return false;
  if (g_burnCycleNum < g_CurrentConfig.P0CyclesBeforeStandby) return false;
  return true;
}

bool cond_furnaceDisabled() {
  return g_furnaceEnabled == 0;
}

void onSwitchToReduction(int trans) {
  assert(g_BurnState == STATE_P1 || g_BurnState == STATE_P2);
  unsigned long t = millis();
  _reductionStateEndMs = 0;
  if (t < g_CurBurnCycleStart) return;
  unsigned long diff = t - g_CurBurnCycleStart;
  unsigned long burnFeedLen = (unsigned long) g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 *  (100L + g_CurrentConfig.FuelCorrection);
  unsigned long cycleLen = g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec * 1000L;
  _reductionStateEndMs = cycleLen - diff;
  if (diff < burnFeedLen) //during feed
  {
    _reductionStateEndMs = _reductionStateEndMs * ((float) diff / burnFeedLen); //reduce the time because not all fuel was added
  }
  Serial.print(F("Remaining time for reduction (ms):"));
  Serial.println(_reductionStateEndMs);
}

uint8_t g_ReductionsToP0 = 0; //reductions P1 -> P0 or P2 -> P0 which we dont ave 
uint8_t g_ReductionsToP1 = 0; //reductions P2 -> P1

void onReductionCycleEnded(int trans) {
  const TBurnTransition &t = BURN_TRANSITIONS[trans];
  if (t.To == STATE_P0) {
    g_ReductionsToP0++;
  }
  else if (t.To == STATE_P1) {
    g_ReductionsToP1++;
  }
}


#define CONTROL_VARIANT 2

const TBurnTransition  BURN_TRANSITIONS[]   = 
{

  //{STATE_P0, STATE_P1, cond_C_belowHysteresisAndNoNeedToHeat, NULL}, //#v2  
  {STATE_P0, STATE_P2, cond_C_belowHysteresisAndNoNeedToHeat_NoAutoStop, NULL}, //#v3: switch to P2 to quickly start burning, then reduce to P1 if no heat needed and temp above hysteresis
  {STATE_P0, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL}, //this fires only if heat needed bc cond_C is earlier
  {STATE_P0, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL},
  {STATE_P0, STATE_P2, cond_willFallBelowHysteresisSoon, NULL}, //temp is dropping fast, we need heat -> P2
  {STATE_P0, STATE_P1, cond_D_belowTargetTempAndNeedHeat, NULL},
  {STATE_P0, STATE_OFF, cond_shouldGoToStandby, NULL}, //wygaszenie
  
  {STATE_P1, STATE_REDUCE1, cond_furnaceDisabled, onSwitchToReduction}, //E. P1 -> P0
  {STATE_P1, STATE_REDUCE1, cond_boilerOverheated, onSwitchToReduction}, //E. P1 -> P0
  {STATE_P1, STATE_REDUCE1, cond_targetTempReachedAndHeatingNotNeeded, onSwitchToReduction}, //F. P1 -> P0
  {STATE_P1, STATE_REDUCE1, cond_autoFire_minTempReachedAndHeatingNotNeeded, onSwitchToReduction}, //F. P1 -> P0
  {STATE_P1, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL},
  
  {STATE_P1, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL},
  
  
  {STATE_REDUCE1, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL}, //juz nie redukujemy  - np sytuacja się zmieniła i temp. została podniesiona. uwaga - ten sam war. co w #10 - cykl
  {STATE_REDUCE1, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL},
  {STATE_REDUCE1, STATE_P1, cond_C_belowHysteresisAndNoNeedToHeat_NoAutoStop, NULL}, //#v2
  {STATE_REDUCE1, STATE_P1, cond_D_belowTargetTempAndNeedHeat, NULL},
  {STATE_REDUCE1, STATE_P0, cond_cycleEnded, onReductionCycleEnded},


  {STATE_P2, STATE_REDUCE2, cond_furnaceDisabled, onSwitchToReduction}, //10 P2 -> P1
  {STATE_P2, STATE_REDUCE2, cond_targetTempReached, onSwitchToReduction}, //10 P2 -> P1
  {STATE_P2, STATE_REDUCE2, cond_willReachTargetSoon, onSwitchToReduction}, //10 P2 -> P1
  {STATE_P2, STATE_REDUCE2, cond_autoFire_minTempReachedAndHeatingNotNeeded, onSwitchToReduction}, //10 P2 -> P1
  
  //{STATE_P2, STATE_REDUCE2, cond_suddenHeatOffAndAboveHysteresis, onSwitchToReduction}, //v2 10 P2 -> P1
  {STATE_P2, STATE_REDUCE2,   cond_noNeedToHeatAndAboveHysteresis, onSwitchToReduction}, //v3 10 P2 -> P1

  
  
  {STATE_REDUCE2, STATE_P2, cond_A_needSuddenHeatAndBelowTargetTemp, NULL},
  {STATE_REDUCE2, STATE_P2, cond_B_belowHysteresisAndNeedHeat, NULL},
  {STATE_REDUCE2, STATE_P1, cond_cycleEnded, onReductionCycleEnded},
  
  {STATE_P2, STATE_FIRESTART, cond_noHeating_Firestart},
  {STATE_P1, STATE_FIRESTART, cond_noHeating_Firestart},
  
  {STATE_P0, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P1, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_P2, STATE_ALARM, isAlarm_NoHeating, NULL},
  {STATE_REDUCE1, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_REDUCE2, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_STOP, STATE_ALARM, NULL, NULL},
  {STATE_FIRESTART, STATE_ALARM, isAlarm_Any, NULL},
  {STATE_FIRESTART, STATE_P2, cond_fireBurningAndBelowTargetTemp, NULL}, 
  {STATE_FIRESTART, STATE_P1, cond_firestartIsBurning, NULL},
  {STATE_FIRESTART, STATE_P0, cond_firestartOverride, NULL},
  {STATE_FIRESTART, STATE_ALARM, cond_firestartTimeout, NULL}, //failed to start fire
  {STATE_OFF, STATE_FIRESTART, cond_D_belowTargetTempAndNeedHeatAndAutoAllowed, NULL},
  {STATE_OFF, STATE_FIRESTART, cond_firestartOverride, NULL},
  {STATE_OFF, STATE_ALARM, NULL, NULL},
  
  {STATE_UNDEFINED, STATE_UNDEFINED, NULL, NULL} //sentinel
};

const TBurnStateConfig BURN_STATES[]  = {
  {STATE_P0, 'P', podtrzymanieStateInitialize, podtrzymanieStateLoop},
  {STATE_P1, '1', workStateInitialize, workStateBurnLoop},
  {STATE_P2, '2', workStateInitialize, workStateBurnLoop},
  {STATE_FIRESTART, 'F', firestartStateInit, firestartStateLoop},  
  {STATE_ALARM, 'A', alarmStateInitialize, alertStateLoop},
  {STATE_REDUCE1, 'R', reductionStateInit, reductionStateLoop},
  {STATE_REDUCE2, 'r', reductionStateInit, reductionStateLoop},
  {STATE_STOP, 'S', stopStateInitialize, manualStateLoop},
  {STATE_OFF, 'z', offStateInit, offStateLoop},
};

const uint8_t N_BURN_TRANSITIONS = sizeof(BURN_TRANSITIONS) / sizeof(TBurnTransition);
const uint8_t N_BURN_STATES = sizeof(BURN_STATES) / sizeof(TBurnStateConfig);
