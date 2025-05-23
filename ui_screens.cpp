#include <arduino.h>
#include <avr/pgmspace.h>
#include "global_variables.h"
#include "ui_handler.h"
#include "boiler_control.h"
#include <MD_DS1307.h>
#include "masterpiec.h"
#include "piec_sensors.h"

uint16_t g_CurrentlyEditedVariable = 0;
uint8_t g_CurrentUIState = 0;
uint8_t g_CurrentUIView = 1;


typedef union TempVarData {
  
} TTempBuffer;

void scrSplash(uint8_t idx, char* lines[])
{
  
  sprintf(lines[0], "MASTERPIEC");
  sprintf(lines[1], "v0.0.1 LAFAR6502");
}

void scrDefault(uint8_t idx, char* lines[]) 
{
  char buf1[10], buf2[10], buf3[10];
  dtostrf(g_TargetTemp,2, 0, buf1);
  dtostrf(g_TempCO,3, 1, buf2);
  sprintf(buf3, "CW:");
  dtostrf(g_TempCWU,3, 1, buf3);
  uint8_t nh = needHeatingNow(); 
  sprintf(lines[0], "T%c%s/%s B:%s",g_HomeThermostatOn ? ';' : ':', buf2, buf1, buf3);
  dtostrf(g_dTl3, 3, 1, buf1);
  buf2[0] = 0;
  if (g_BurnState == STATE_STOP) sprintf(buf2, "STOP");
  sprintf(lines[1], "%c%c %d %s %s", g_furnaceEnabled == 0 ? '|' : (nh == NEED_HEAT_NONE ? '_' : nh == NEED_HEAT_CO ? '!' : '@'), BURN_STATES[g_BurnState].Code, getCurrentBlowerPower(), buf1, buf2); 
  
}

void scrTime(uint8_t idx, char* lines[]) {
  sprintf(lines[0], "%04d.%02d.%02d      ", RTC.yyyy, RTC.mm, RTC.dd, RTC.h, RTC.m);
  sprintf(lines[1], "%02d:%02d:%02d      ", RTC.h, RTC.m, RTC.s);
}

  
void scrSensors1(uint8_t idx, char* lines[] ) {
  char buf1[10], buf2[10];
  dtostrf(g_TempSpaliny,3, 1, buf1);
  dtostrf(g_AirFlow, 3, 1, buf2);
  sprintf(lines[0], "TSpalin:%s", buf1);
  sprintf(lines[1], "Flow:%d %d", g_AirFlowNormal, getBlowerPowerCorrection());
}

void scrSensors2(uint8_t idx, char* lines[] ) {
  char buf1[10], buf2[10];
  dtostrf(g_TempPowrot,2, 1, buf1);
  dtostrf(g_TempFeeder,2, 1, buf2);
  sprintf(lines[0], "TPowrot:%s", buf1);
  sprintf(lines[1], "TPodaj:%s", buf2);
}

void scrSensors3(uint8_t idx, char* lines[] ) {
  char buf1[10], buf2[10];
  int fc = firestartIsBurningCheck();
  float exhStart = g_InitialTempExh < g_InitialTempCO - EXHAUST_TEMP_DELTA_BELOW_CO ? g_InitialTempCO - EXHAUST_TEMP_DELTA_BELOW_CO : g_InitialTempExh;
  dtostrf(g_TempSpaliny - exhStart, 3, 1, buf1);
  dtostrf(g_TempCO - g_InitialTempCO,3, 1, buf2);
  sprintf(lines[0], "WS:%s WC:%s", buf1, buf2);
  dtostrf(g_TempSpaliny - g_TempCO, 3, 1, buf1);
  dtostrf(g_dTExh,3, 1, buf2);
  sprintf(lines[1], "%dSC:%s E:%s", fc, buf1, buf2);
}

extern unsigned long _reductionStateEndMs; //burn control

void scrBurnInfo(uint8_t idx, char* lines[]) {
  unsigned long tnow = millis();
  unsigned long tt = (tnow - g_CurStateStart) / 1000L;
  char zbuf[8];
  if (g_BurnState == STATE_P0 || g_BurnState == STATE_P1 || g_BurnState == STATE_P2)
  {
    uint8_t cycle = g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle == 0 ? g_DeviceConfig.DefaultBlowerCycle : g_CurrentConfig.BurnConfigs[g_BurnState].BlowerCycle;
    
    uint8_t nh = needHeatingNow();
    float f2 = calculateHeatPowerFor(g_CurrentConfig.BurnConfigs[g_BurnState].FuelSecT10 / 10.0, g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec);
    zbuf[5]=0;
    dtostrf(f2, 2, 2, zbuf);
    
    sprintf(lines[0], "#%c %skW T%d", BURN_STATES[g_BurnState].Code, zbuf, g_CurrentConfig.BurnConfigs[g_BurnState].CycleSec - (tnow - g_CurBurnCycleStart) / 1000); 
    sprintf(lines[1], "%c%d %d %ld", nh == NEED_HEAT_NONE ? '_' : nh == NEED_HEAT_CO ? '!' : '@', getCurrentBlowerPower(), g_burnCycleNum, tt);
  }
  else if (g_BurnState == STATE_REDUCE1 || g_BurnState == STATE_REDUCE2) 
  {
    int tt = (_reductionStateEndMs - millis()) / 1000L;
    sprintf(lines[0], "redukcja %c -%d", BURN_STATES[g_BurnState].Code, tt);  
  }
  else if (g_BurnState == STATE_STOP) {
    sprintf(lines[0], "STOP - tryb reczny");
  }
  else if (g_BurnState == STATE_ALARM) {
    sprintf(lines[0], "ALARM");
    if (g_Alarm != NULL) sprintf(lines[1], "%s", g_Alarm);
  }
  else if (g_BurnState == STATE_FIRESTART) {
    sprintf(lines[0], "ROZPAL #%d", g_burnCycleNum);
    sprintf(lines[1], "dEx:%f dCO:%f", g_TempSpaliny - g_InitialTempExh, g_TempCO - g_InitialTempCO);
  }
  else if (g_BurnState == STATE_OFF) {
    sprintf(lines[0], "STANDBY");
    sprintf(lines[1], "T:%ld", tt);
  }
}

void scrOverrides(uint8_t idx, char* lines[] ) {
  char buf1[10], buf2[10];
  sprintf(lines[0], "E|C|W|e|c|w|t");
  sprintf(lines[1], "%d|%d|%d|%d|%d|%d|%d%d%d", g_furnaceEnabled, g_coPumpOverride, g_cwuPumpOverride, digitalRead(FURNACE_ENABLE_PIN), digitalRead(PUMP_CO_EXT_CTRL_PIN), digitalRead(PUMP_CW_EXT_CTRL_PIN), digitalRead(HW_THERMOSTAT_PIN), digitalRead(HW_THERMOSTAT_PIN_ALT), isThermostatOn());
}

uint16_t findNextView(uint16_t currentView, bool increment, bool (*f)(uint16_t))
{
  int16_t c = currentView;
  int cnt = 0;
  while(cnt++ < N_UI_SCREENS) {
    c += increment ? 1 : -1;
    if (c < 0) c = N_UI_SCREENS - 1;
    if (c >= N_UI_SCREENS) c = 0;
    if (f(c)) return c;
  }
  return currentView;
}

bool viewCodeMatchesState(uint16_t n) {
  TUIScreenEntry se;
  memcpy_P(&se, UI_SCREENS + n, sizeof(TUIScreenEntry));
    
  return se.Code == UI_STATES[g_CurrentUIState].Code;
}

bool variableIsAdvanced(uint16_t n) {
  return (UI_VARIABLES[n].Flags & VAR_ADVANCED) != 0;
}

bool variableIsNotAdvanced(uint16_t n) {
  return (UI_VARIABLES[n].Flags & VAR_ADVANCED) == 0;
}

uint16_t findNextVariable(uint16_t currentVariable, bool increment, bool (*f)(uint16_t))
{
  int16_t c = currentVariable;
  int cnt = 0;
  while(cnt++ < N_UI_VARIABLES) {
    c += increment ? 1 : -1;
    if (c < 0) c = N_UI_VARIABLES - 1;
    if (c >= N_UI_VARIABLES) c = 0;
    if (f == NULL || f(c)) return c;
  }
  return currentVariable;
}

void stDefaultEventHandler(uint8_t ev, uint8_t arg) 
{
  const TUIStateEntry* ps = UI_STATES + g_CurrentUIState;
  if (ev == UI_EV_INITSTATE) {
    
    if (g_BurnState == STATE_ALARM) 
      g_CurrentUIView = 5;
    else
    {
      if (!viewCodeMatchesState(g_CurrentUIView)) g_CurrentUIView = findNextView(g_CurrentUIView, true, viewCodeMatchesState);
    }
    return;
  }
  if (ev == UI_EV_UP) {
    g_CurrentUIView = findNextView(g_CurrentUIView, true, viewCodeMatchesState);
  } else if (ev == UI_EV_DOWN) {
    g_CurrentUIView = findNextView(g_CurrentUIView, false, viewCodeMatchesState);
  }
  else if (ev == UI_EV_BTNPRESS) {
    changeUIState('V');
  }
}

void* g_editCopy = NULL;

void stSelectVariableHandler(uint8_t ev, uint8_t arg) 
{
  const TUIStateEntry* ps = UI_STATES + g_CurrentUIState;
  bool advanced = ps->Data.numV == 1;
  g_CurrentUIView = ps->DefaultView;
  if (g_CurrentlyEditedVariable > N_UI_VARIABLES) g_CurrentlyEditedVariable = 0;
  bool (*f)(uint16_t) = advanced ? variableIsAdvanced : variableIsNotAdvanced;
  if (ev == UI_EV_INITSTATE) {
    if (!f(g_CurrentlyEditedVariable)) g_CurrentlyEditedVariable = findNextVariable(g_CurrentlyEditedVariable, true, f);
    return;
  }
  
  
  if (ev == UI_EV_UP) 
  {
    g_CurrentlyEditedVariable = findNextVariable(g_CurrentlyEditedVariable, true, f);
    Serial.print("next var:");
    Serial.println(g_CurrentlyEditedVariable);
  }
  else if (ev == UI_EV_DOWN) 
  {
    g_CurrentlyEditedVariable = findNextVariable(g_CurrentlyEditedVariable, false, f);
    Serial.print("next var:");
    Serial.println(g_CurrentlyEditedVariable);
  }
  else if (ev == UI_EV_BTNPRESS) 
  {
    if ((UI_VARIABLES[g_CurrentlyEditedVariable].Flags & VAR_IMMEDIATE) != 0 && UI_VARIABLES[g_CurrentlyEditedVariable].Adjust != NULL)
    {
      UI_VARIABLES[g_CurrentlyEditedVariable].Adjust(g_CurrentlyEditedVariable, NULL, 1);
      return;
    }
    if ((UI_VARIABLES[g_CurrentlyEditedVariable].Adjust) != NULL)
    {
      changeUIState('E');
    }  
  }
  else if (ev == UI_EV_IDLE) {
    changeUIState('0');
  }
}

//edycja zmiennej g_CurrentlyEditedVariable
void stEditVariableHandler(uint8_t ev, uint8_t arg) 
{
  const TUIStateEntry* ps = UI_STATES + g_CurrentUIState;
  const TUIVarEntry* pv = UI_VARIABLES + g_CurrentlyEditedVariable;
  if (ev == UI_EV_INITSTATE) 
  {
    //1. make a copy
    g_editCopy = NULL;
    if (pv->Store != NULL) 
    {
      Serial.print("cp1 ");
      Serial.print(g_CurrentlyEditedVariable);
      Serial.print(" ");
      Serial.println((unsigned long) pv->DataPtr);
      
      g_editCopy = pv->Store(g_CurrentlyEditedVariable, pv->DataPtr, false);
    };
    return;
  }
  
  
  if (ev == UI_EV_UP || ev == UI_EV_DOWN) {
    if (pv->Adjust != NULL) {
      Serial.print(F("adj var:"));
      Serial.println(g_CurrentlyEditedVariable);
      //delay(20);
      pv->Adjust(g_CurrentlyEditedVariable, g_editCopy, ev == UI_EV_UP ? 1 : -1);
    }
  }
  else if (ev == UI_EV_BTNPRESS) 
  {
    Serial.print(F("x save var:"));
	  Serial.println(g_CurrentlyEditedVariable);
  
    if (pv->Store != NULL && g_editCopy != NULL)
    {
      Serial.print("cp2 ");
      Serial.print(g_CurrentlyEditedVariable);
      Serial.print(" ");
      Serial.print((unsigned long) g_editCopy);
      Serial.print(" ");
      Serial.println((unsigned long) pv->DataPtr);
      //delay(20);
      pv->Store(g_CurrentlyEditedVariable, g_editCopy, true);
    }
    g_editCopy = NULL;
    if (pv->Commit != NULL) 
    {
      Serial.println("commit");
      //delay(20);
      //return;
      pv->Commit(g_CurrentlyEditedVariable);  
    }
    
    Serial.println("saved!");
    delay(20);
    changeUIState((pv->Flags & VAR_ADVANCED) != 0 ? 'W' : 'V');
  }
  else if (ev == UI_EV_IDLE) {
    //Serial.print("dont save var:");
    //Serial.println(g_CurrentlyEditedVariable);
    
    changeUIState((pv->Flags & VAR_ADVANCED) != 0 ? 'W' : 'V');
  }
}

uint8_t _curDay;
void stDailyLogsHandler(uint8_t ev, uint8_t arg) 
{
  if (ev == UI_EV_INITSTATE) {
    _curDay = RTC.dow - 1;
    g_CurrentUIView = 8;
    return;
  }
  if (ev == UI_EV_UP) 
  {
    _curDay = _curDay >= 6 ? 0 : _curDay + 1;
  } 
  else if (ev == UI_EV_DOWN) 
  {
    _curDay = _curDay == 0 ? 6 : _curDay - 1;
  }
  else if (ev == UI_EV_BTNPRESS) {
    changeUIState('0');
  }
}

void scrLog(uint8_t idx, char* lines[]) 
{
  char buf[7];
  _curDay = _curDay % 7;
  bool today = _curDay == (RTC.dow - 1);
  TDailyLogEntry ent = g_DailyLogEntries[_curDay];
  dtostrf(calculateFuelWeightKg(ent.FeederTotalSec), 3, 1, buf);
  sprintf(lines[0], "%c%d %skg %d", today ? '*' : ' ', _curDay + 1, buf, ent.FeederTotalSec);
  sprintf(lines[1], "P %d %d %d", ent.P0TotalSec2 / 30, ent.P1TotalSec2 / 30, ent.P2TotalSec2 / 30);
}

void scrSelectVariable(uint8_t idx, char* lines[])
{
  const TUIStateEntry* ps = UI_STATES + g_CurrentUIState;
  const TUIVarEntry* pv = UI_VARIABLES + g_CurrentlyEditedVariable;
  strcpy_P(lines[0], pv->Name);
  //sprintf(lines[0], "%s", pv->Name);
  sprintf(lines[1], "V:");
  if (pv->PrintTo != NULL) pv->PrintTo(g_CurrentlyEditedVariable, NULL, lines[1] + 2, false);
}

void scrEditVariable(uint8_t idx, char* lines[])
{
  const TUIStateEntry* ps = UI_STATES + g_CurrentUIState;
  const TUIVarEntry* pv = UI_VARIABLES + g_CurrentlyEditedVariable;
  strcpy_P(lines[1], pv->Name);
  sprintf(lines[0], "<-%s->", lines[1]);
  if (pv->PrintTo != NULL) 
    pv->PrintTo(g_CurrentlyEditedVariable, g_editCopy, lines[1], false);
  else
    lines[1][0] = '\0';
}
void printUint16_1000(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint16_t* pv = (uint16_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (uint16_t) (atof(buf) * 1000);
    return;
  }
  float f = *pv / 1000.0;
  char buf1[10];
  dtostrf(f,2, 2, buf1);
  strcpy(buf, buf1);
}

//print uint_8 value as percent (100% = 255)
void printUint8_perc(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {

  uint8_t* pv = (uint8_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (uint8_t) (atof(buf) * 2.55);
    return;
  }
  float f = (float) *pv / 2.55;
  dtostrf(f, 2, 1, buf);
}


void adjustUint8(uint8_t varIdx, void* data, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  uint8_t* pd = (uint8_t*) (data == NULL ? pv->DataPtr : data);
  if (pv->PrintTo == printUint16_1000) increment *= 10;
  uint8_t v2 = *pd + increment;
  if (v2 < pv->Min) v2 = (uint8_t) pv->Max;
  if (v2 > pv->Max) v2 = (uint8_t) pv->Min;
  *pd = v2;
}

void adjustint8(uint8_t varIdx, void* data, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  int8_t* pd = (int8_t*) (data == NULL ? pv->DataPtr : data);
  int8_t v2 = *pd + increment;
  if (v2 < pv->Min) v2 = (int8_t) pv->Min;
  if (v2 > pv->Max) v2 = (int8_t) pv->Max;
  *pd = v2;
}




void adjustInt(uint8_t varIdx, void* data, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  int* pd = (int*) (data == NULL ? pv->DataPtr : data);
  int v2 = *pd + increment;
  if (v2 < pv->Min) v2 = (int) pv->Max;
  if (v2 > pv->Max) v2 = (int) pv->Min;
  *pd = v2;
}


void adjustBool(uint8_t varIdx, void* data, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  bool* pd = (bool*) (data == NULL ? pv->DataPtr : data);
  Serial.print(F("adjb var:"));
  Serial.print(g_CurrentlyEditedVariable);
  Serial.print(" ");
  Serial.print((unsigned long) pv);
  Serial.print(" ");
  Serial.print(*pd);
  Serial.print("->");
  if (increment != 0) *pd = *pd == true ? false : true;
  Serial.println(*pd);
}


void adjustUint16(uint8_t varIdx, void* data, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  uint16_t* pd = (uint16_t*) (data == NULL ? pv->DataPtr : data);
  if (pv->PrintTo == printUint16_1000) increment *= 10;
  uint16_t v2 = (*pd) + increment;
  if (v2 < (uint16_t) pv->Min) v2 = (uint16_t) pv->Max;
  if (v2 > (uint16_t) pv->Max) v2 = (uint16_t) pv->Min;
  Serial.print("upd v:");
  Serial.println(v2);
  *pd = v2;
}


void adjustFloat(uint8_t varIdx, void* data, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  float* pd = (float*) (data == NULL ? pv->DataPtr : data);
  float v2 = *pd + (increment * 0.1);
  if (v2 < pv->Min) v2 = (uint8_t) pv->Max;
  if (v2 > pv->Max) v2 = (uint8_t) pv->Min;
  Serial.print("upd v:");
  Serial.println(v2);
  *pd = v2;
}

void printUint8(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint8_t* pv = (uint8_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (uint8_t) atoi(buf);
    return;
  }
  sprintf(buf, "%d", *pv);
}

//print value from strings array
void printUint8_values(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint8_t* pv = (uint8_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  const char** arr = UI_VARIABLES[varIdx].Data.strings;
  uint8_t mx = (uint8_t) UI_VARIABLES[varIdx].Max;
  char tmp[20];
  if (parseString) {
    if (pv == NULL) return;
    if (arr != NULL) {
      
      for(int i=0; i<mx; i++) {
        strcpy_P(tmp, (char*)pgm_read_word(&(arr[i])));
        if (strcmp(buf, tmp) == 0) {
          *pv = i;
          return;
        }
      }
    }
    *pv = (uint8_t) atoi(buf);
    return;
  }
  if (arr != NULL && *pv <= mx) {
    strcpy_P(buf, (char*)pgm_read_word(&(arr[*pv])));
  }
  else {
    sprintf(buf, "%d", *pv);  
  }
}

void printUint8AsBool(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint8_t* pv = (uint8_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = strcasecmp(buf, "ON") == 0 || strcmp(buf, "1") == 0 || strcasecmp(buf, "true") == 0 ? 1 : 0;
    return;
  }
  sprintf(buf, "%s", *pv == 0 ? "OFF" : "ON");
}
void printUint16(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint16_t* pv = (uint16_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (uint16_t) atoi(buf);
    return;
  }
  sprintf(buf, "%d", *pv);
}

void printUint16_10(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint16_t* pv = (uint16_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (uint16_t) (atof(buf) * 10);
    return;
  }
  float f = *pv / 10.0;
  char buf1[10];
  dtostrf(f,2, 1, buf1);
  strcpy(buf, buf1);
}


void printFeedRate_WithHeatPower(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint16_t *pv = (uint16_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  uint16_t *pc = UI_VARIABLES[varIdx].Data.ptr;
  if (parseString) {
    if (pv == NULL) return;
    *pv = (uint16_t) (atof(buf) * 10);
    return;
  }
  float f = *pv / 10.0;
  char buf1[10]; char buf2[10] = {0};
  dtostrf(f,2, 1, buf1);
  if (pc != NULL) {
    float f2 = calculateHeatPowerFor(f, *pc);
    dtostrf(f2,2, 2, buf2);
    sprintf(buf, "%s  %s kW", buf1, buf2);
    return;
  }
  strcpy(buf, buf1);
}


void printUint8_10(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint8_t* pv = (uint8_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (uint8_t) (atof(buf) * 10);
    return;
  }
  float f = *pv / 10.0;
  char buf1[10];
  dtostrf(f,2, 1, buf1);
  strcpy(buf, buf1);
}



void printint8_10(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  int8_t* pv = (int8_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (int8_t) (atof(buf) * 10);
    return;
  }
  float f = *pv / 10.0;
  char buf1[10];
  dtostrf(f,2, 1, buf1);
  strcpy(buf, buf1);
}

void printint8(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  int8_t* pv = (int8_t*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = (int8_t) atoi(buf);
    return;
  }
  sprintf(buf, "%d", (int) *pv);
}


void printFloat(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  float* pv = (float*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = atof(buf);
    return;
  }
  char buf1[10];
  dtostrf(*pv,2, 1, buf1);
  strcpy(buf, buf1);
}

void printBool(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  bool* pv = (bool*) (editCopy == NULL ? UI_VARIABLES[varIdx].DataPtr : editCopy);
  if (parseString) {
    if (pv == NULL) return;
    *pv = strcasecmp(buf, "ON") == 0 || strcmp(buf, "1") == 0 || strcasecmp(buf, "true") == 0 ? true : false;
    return;
  }
  strcpy(buf, *pv ? "ON" : "OFF");
}

void printVBoolSwitch(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  bool* pd = (bool*) editCopy;
  BoolFun f = (BoolFun) UI_VARIABLES[varIdx].DataPtr;
  if (parseString) {
    if (pd == NULL) return;
    *pd = strcasecmp(buf, "ON") == 0 || strcmp(buf, "1") == 0 || strcasecmp(buf, "true") == 0 ? true : false;
    return;
  }
  bool v = false;
  if (pd != NULL || f != NULL) {
    v = pd == NULL ? f() : *pd;
  }
  strcpy(buf, v ? "ON" : "OFF");
}

void printState(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  uint8_t cst = getManualControlState();
  uint8_t* pv = (uint8_t*) (editCopy == NULL ? &cst : editCopy);
  
  if (parseString) {
    if (pv == NULL) return;
    for(int i=0; i<N_BURN_STATES;i++) {
      if (BURN_STATES[i].Code == buf[0]) {
        *pv = i;
        return;
      }
    }
    *pv = STATE_UNDEFINED;
    return;
  }
  if (*pv >= 0 && *pv < N_BURN_STATES) {
    sprintf(buf, "%c", BURN_STATES[*pv].Code);
  }
  else {
    sprintf(buf, "?");
  }
  
}

void adjustManualState(uint8_t varIdx, void* d, int8_t increment) {
  TSTATE* p = (TSTATE*) d;
  if (!getManualControlMode()) return;
  
  uint8_t v = *p;
  v = (v + increment) % (STATE_OFF+1); //+= increment;
  *p = v;
  Serial.print("adj state:");
  Serial.print(v);
  Serial.println();
}


void* copyManualState(uint8_t varIdx, void* pData, bool save) 
{
  static TSTATE _copy;
  if (save) {
    setManualControlState(_copy);
    Serial.print("set st:");
    Serial.print(_copy);
    Serial.println();
    return NULL;
  } else {
    _copy = getManualControlState();
    Serial.print("get st:");
    Serial.print(_copy);
    Serial.println();
    return &_copy;
  }
}

void* copyVBoolSwitch(uint8_t varIdx, void* pData, bool save) {
  BoolFun f = (BoolFun) UI_VARIABLES[varIdx].DataPtr;
  static bool _copy;
  
  if (save) {
    SetBoolFun sbf = UI_VARIABLES[varIdx].Data.setBoolF;
	//unsigned long v0 = (unsigned long) setManualControlMode;
	//Serial.print("copy vbool ");
	//Serial.print(v0);
	//Serial.print(" ");
	//Serial.print((unsigned long) sbf);
  //Serial.print(" v:");
  //Serial.print(_copy);
	//Serial.println();
	//return;
    if (sbf != NULL) sbf(_copy);
  }
  else {
    _copy = f == NULL ? false : f();
    return &_copy;
  }
}
typedef uint8_t (*U8Fun)();
void printVU8_perc(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
    
  U8Fun f = (U8Fun) UI_VARIABLES[varIdx].DataPtr;
  if (parseString) {
    if (editCopy == NULL) return;
    uint8_t* pv = (uint8_t*) editCopy;
    *pv = (uint8_t) (atof(buf) * 2.55);
    return;
  }
  if (f == NULL) return;
  float fv = (float) f() / 2.55;
  dtostrf(fv, 2, 1, buf);
  
}

void printVU8(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  U8Fun f = (U8Fun) UI_VARIABLES[varIdx].DataPtr;
  if (parseString) {
    if (editCopy == NULL) return;
    uint8_t* pv = (uint8_t*) editCopy;
    *pv = (uint8_t) (atof(buf) * 10);
    return;
  }
  if (f == NULL) return;
  uint8_t v = f();
  sprintf(buf, "%d", v);
}

void printVU8_blower(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  U8Fun f = (U8Fun) UI_VARIABLES[varIdx].DataPtr;
  if (parseString) {
    if (editCopy == NULL) return;
    uint8_t* pv = (uint8_t*) editCopy;
    *pv = (uint8_t) (atof(buf) * 10);
    return;
  }
  if (f == NULL) return;
  uint8_t v = f();
  
  sprintf(buf, "%d F%d", v, g_AirFlowNormal);
}


void printPumpState(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  int i = (int) UI_VARIABLES[varIdx].DataPtr;
  if (!isPumpEnabled(i)) {
    strcpy(buf, "BRAK");
    return;
  }
  bool v = isPumpOn(i);
  strcpy(buf, v ? "ON" : "OFF");
}

void adjustPumpState(uint8_t varIdx, void* d, int8_t increment) {
  int i = (int) UI_VARIABLES[varIdx].DataPtr;
  if (!isPumpEnabled(i)) return;
  if (!getManualControlMode()) return;
  if (isPumpOn(i))
    setPumpOff(i);
  else
    setPumpOn(i);
}

//jak to działa -> otoz zmieniamy wartość zmiennej (i) ktora ma nr czujnika.
void printDallasInfo(uint8_t varIdx, void* editCopy, char* buf, bool parseString) {
  int idx = (int) UI_VARIABLES[varIdx].DataPtr;
  if (parseString) 
  {
    if (editCopy == NULL) return; //error
    //buf has the address in hex, parse it and put in the editcopy.
    //assert(false);//todo
    return;
  }
  if (editCopy == NULL) {
    int idx2 = findDallasIndex(g_DeviceConfig.DallasAddress[idx]);
    if (idx2 < 0) 
      sprintf(buf, "%s", "-brak-");
    else
      printDallasInfo(idx2, buf); 
  }
  else {
    idx = * ((int*) editCopy);
    if (idx < 0) 
      sprintf(buf, "%s", "-brak-");
    else
      printDallasInfo(idx, buf);
  }
}

void* copyDallasInfo(uint8_t vIdx, void* pData, bool save) 
{
  int idx = (int) UI_VARIABLES[vIdx].DataPtr;
  static int cidx;
  if (!save) {
    int i2 = findDallasIndex(g_DeviceConfig.DallasAddress[idx]);
    cidx = i2 >= 0 ? i2 : -1;
    return &cidx;
  } 
  else  
  {
    if (cidx < 0 || cidx >= 8)
    {
      memset(g_DeviceConfig.DallasAddress[idx], 0, 8);
    }
    else 
    {
      getDallasAddress(cidx, g_DeviceConfig.DallasAddress[idx]);//copy to config
      if (cidx != idx) swapDallasAddress(idx, cidx); //move the sensor info
    }
  }
}

void adjustFeederState(uint8_t varIdx, void*d, int8_t increment) {
  if (!getManualControlMode()) return;
  setFeeder(!isFeederOn());
}

void adjustHeaterState(uint8_t varIdx, void*d, int8_t increment) {
  if (!getManualControlMode()) return;
  setHeater(!isHeaterOn());
}


void adjustBlowerState(uint8_t varIdx, void* d, int8_t increment) {
  if (!getManualControlMode()) return;
  g_TargetFlow = 0;
  uint8_t v = getCurrentBlowerPower();
  v += increment;
  setBlowerPower(v);
}

void adjustUIState(uint8_t varIdx, void* d, int8_t increment) {
  char c = (char) UI_VARIABLES[varIdx].DataPtr;
  changeUIState(c);
}

void* copyU8(uint8_t varIdx, void* pData, bool save) 
{
  static uint8_t _copy;
  
  uint8_t* p = (uint8_t*) UI_VARIABLES[varIdx].DataPtr;
  if (save) {
    *p = _copy;
    return p;
  } else {
    _copy = *p;
    return &_copy;
  }
}

void* copyBool(uint8_t varIdx, void* pData, bool save) 
{
  static bool _copy;
  bool* p = (bool*) UI_VARIABLES[varIdx].DataPtr;
  if (save) {
    *p = _copy ? true : false;
    return p;
  } else {
    _copy = *p ? true : false;
    return &_copy;
  }
}


void* copyU16(uint8_t varIdx, void* pData, bool save) 
{
  static uint16_t _copy;
  uint16_t* p = (uint16_t*) UI_VARIABLES[varIdx].DataPtr;
  if (save) {
    *p = _copy;
    return p;
  } else {
    _copy = *p;
    return &_copy;
  }
}

void* copyFloat(uint8_t varIdx, void* pData, bool save) 
{
  static float _copy;
  float* p = (float*) UI_VARIABLES[varIdx].DataPtr;
  if (save) {
    *p = _copy;
    return p;
  } else {
    _copy = *p;
    return &_copy;
  }
}



void commitTime(void* p) {
  Serial.println(F("save time"));
  RTC.writeTime();
};

void commitConfig(uint8_t varIdx) {
  eepromSaveConfig(g_DeviceConfig.SettingsBank);
}

void commitDevConfig(uint8_t varIdx) {
  updateDeviceConfig();
}


void s_clearLogs(uint8_t varIdx) {
  clearDailyLogs();
}

void queueCommitTime(uint8_t varIdx) {
  g_uiBottomHalf = commitTime;
  g_uiBottomHalfCtx = UI_VARIABLES[varIdx].DataPtr;
}

const TUIStateEntry UI_STATES[] = {
    {'0', NULL, 1, stDefaultEventHandler, NULL},
    {'V', {.numV=0}, 3 ,stSelectVariableHandler, NULL},
    {'W', {.numV=1}, 3,stSelectVariableHandler, NULL},
    {'E', NULL, 4, stEditVariableHandler, NULL},
    {'L', NULL, 9, stDailyLogsHandler, NULL}
    
};

const TUIScreenEntry UI_SCREENS[] PROGMEM = {
    {'\0', NULL, scrSplash},
    {'0', NULL, scrDefault},
    {'0', NULL, scrTime},
    {'V', NULL, scrSelectVariable},
    {'E', NULL, scrEditVariable},
    {'0', NULL, scrBurnInfo},
    {'0', NULL, scrSensors1},
    {'0', NULL, scrSensors2},
    {'0', NULL, scrSensors3},
    {'0', NULL, scrOverrides},
    {'L', NULL, scrLog},
};

const uint8_t N_UI_SCREENS = sizeof(UI_SCREENS) / sizeof(TUIScreenEntry);

const char str_off[] PROGMEM = "OFF/brak";
const char str_on[] PROGMEM = "ON/wlacz";

const char str_cool_1[] PROGMEM = "T1";
const char str_cool_2[] PROGMEM = "T2";
const char str_cool_3[] PROGMEM = "T3 cwu";

const char str_thermo_no[] PROGMEM = "Styk NO";
const char str_thermo_nc[] PROGMEM = "Styk NC";

const char str_firestart_autostop[] PROGMEM = "Tylko wygasza";
const char str_firestart_full[] PROGMEM = "Full auto";

const char str_flowctrl_1[] PROGMEM = "T1 korekcja";
const char str_flowctrl_2[] PROGMEM = "T2 korekcja";
const char str_flowctrl_3[] PROGMEM = "T3 hit&miss";
const char str_flowctrl_4[] PROGMEM = "T4 hit&miss";
const char str_flowctrl_5[] PROGMEM = "T5 hit&miss";
const char str_flowctrl_6[] PROGMEM = "T6 hit&miss";


const char* const values_cool_mode[] PROGMEM = {str_off, str_cool_1, str_cool_2, str_cool_3};
const char* const values_onoff[] PROGMEM = {str_off, str_on};
const char* const values_firestart_mode[] PROGMEM = {str_off, str_firestart_autostop, str_firestart_full};
const char* const values_flow_ctrl_mode[] PROGMEM = {str_off, str_flowctrl_1, str_flowctrl_2, str_flowctrl_3, str_flowctrl_4, str_flowctrl_5, str_flowctrl_6};

const TUIVarEntry UI_VARIABLES[] = {
  {MPSTR("Rok"), VAR_ADVANCED, &RTC.yyyy, 2019, 3000, printUint16, adjustUint16, copyU16, queueCommitTime},
  {MPSTR("Miesiac"), VAR_ADVANCED, &RTC.mm, 1, 12, printUint8, adjustUint8, copyU8, queueCommitTime},
  {MPSTR("Dzien"), VAR_ADVANCED, &RTC.dd, 1, 31, printUint8, adjustUint8, copyU8, queueCommitTime},
  {MPSTR("Godzina"), VAR_ADVANCED, &RTC.h, 1, 23, printUint8, adjustUint8, copyU8, queueCommitTime},
  {MPSTR("Minuta"), VAR_ADVANCED, &RTC.m, 1, 59, printUint8, adjustUint8, copyU8, queueCommitTime},
  
  {MPSTR("Tryb reczny"), 0, getManualControlMode, 0, 1, printVBoolSwitch, adjustBool, copyVBoolSwitch, NULL, {.setBoolF = setManualControlMode}},
  {MPSTR("Stan"), 0, NULL, STATE_P0, STATE_OFF, printState, adjustManualState, copyManualState, NULL, NULL},
  {MPSTR("Pompa CO"),0, PUMP_CO1, 0, 1, printPumpState, adjustPumpState, NULL, NULL},
  {MPSTR("Pompa CWU"),0, PUMP_CWU1, 0, 1, printPumpState, adjustPumpState, NULL, NULL},
  {MPSTR("Pompa obieg"),0, PUMP_CIRC, 0, 1, printPumpState, adjustPumpState, NULL, NULL},
  {MPSTR("Dmuchawa"),0, getCurrentBlowerPower, 0, 256, printVU8_blower, adjustBlowerState, NULL, NULL, NULL},
  {MPSTR("Sila nadmuch"),0, &g_TargetFlow, 0, 256, printUint8, adjustUint8, NULL, NULL, NULL},
  {MPSTR("Podajnik"),0, isFeederOn, 0, 1, printVBoolSwitch, adjustFeederState, NULL, NULL},
  {MPSTR("Zapalarka"),0, isHeaterOn, 0, 1, printVBoolSwitch, adjustHeaterState, NULL, NULL},
  {MPSTR("Temp.CO"),0, &g_CurrentConfig.TCO, 30, 80, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Histereza CO"),0, &g_CurrentConfig.THistCO, 0, 15, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Temp.CWU"),0, &g_CurrentConfig.TCWU, 20, 80, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Temp.CWU2"),0, &g_CurrentConfig.TCWU2, 20, 80, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Histereza CWU"),0, &g_CurrentConfig.THistCwu, 0, 15, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Korekta opalu%"),0, &g_CurrentConfig.FuelCorrection, -99, 99, printint8, adjustint8, copyU8, commitConfig},
  {MPSTR("Pomin rozpal"),VAR_IMMEDIATE, &g_overrideBurning, 0, 1, printBool, adjustBool, NULL, NULL},
  
  {MPSTR("Temp.min.pomp"), VAR_ADVANCED, &g_CurrentConfig.TMinPomp, 30, 80, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Zewn. termostat"), VAR_ADVANCED, &g_CurrentConfig.EnableThermostat, 0, 2, printUint8, adjustUint8, NULL, commitConfig},
  {MPSTR("Zewn ster pomp"), VAR_ADVANCED, &g_CurrentConfig.ExtPumpControlMode, 0, 4, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Zewn ster pieca"), VAR_ADVANCED, &g_CurrentConfig.ExtFurnaceControlMode, 0, 4, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Chlodz. praca m"), VAR_ADVANCED, &g_CurrentConfig.CooloffTimeM10, 0, 250, printUint8_10, adjustUint8, copyU8, commitConfig},
  {MPSTR("Chlodz.przerwa m"), VAR_ADVANCED, &g_CurrentConfig.CooloffPauseM10, 0, 1200, printUint16_10, adjustUint16, copyU16, commitConfig},
  {MPSTR("Chlodz. tryb"), VAR_ADVANCED, &g_CurrentConfig.CooloffMode, 0, 2, printUint8_values, adjustUint8, copyU8, commitConfig, {.strings = values_cool_mode}},
  {MPSTR("Cykl cyrkul. min"), VAR_ADVANCED, &g_CurrentConfig.CircCycleMin, 0, 250, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Cyrkulacja sek"), VAR_ADVANCED, &g_CurrentConfig.CircWorkTimeS, 0, 250, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Wydluz dopal.P2%"), VAR_ADVANCED, &g_CurrentConfig.ReductionP2ExtraTime, 0, 250, printUint8, adjustUint8, copyU8, commitConfig},
  
  {MPSTR("DeltaT"), VAR_ADVANCED, &g_CurrentConfig.TDeltaCO, 0, 15, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("DeltaCWU"), VAR_ADVANCED, &g_CurrentConfig.TDeltaCWU, 0, 15, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Tryb letni"), VAR_ADVANCED, &g_CurrentConfig.SummerMode, 0, 1, printBool, adjustBool, copyBool, commitConfig, {.strings=values_onoff}},
  {MPSTR("Max T podajnika"), VAR_ADVANCED, &g_CurrentConfig.FeederTempLimit, 0, 200, printUint8, adjustUint8, copyU8, commitConfig}, 
  {MPSTR("Wygasniecie po"), VAR_ADVANCED, &g_CurrentConfig.NoHeatAlarmCycles, 0, 60, printUint8, adjustUint8, copyU8, commitConfig}, 
  {MPSTR("Dmuchawa CZ"), VAR_ADVANCED, &g_DeviceConfig.DefaultBlowerCycle, 0, 100, printUint8, adjustUint8, copyU8, commitDevConfig},
  {MPSTR("Dmuchawa Max"), VAR_ADVANCED, &g_CurrentConfig.BlowerMax, 0, 256, printUint8_perc, adjustUint8, copyU8, commitConfig},
  {MPSTR("Kg/h podajnik"), VAR_ADVANCED, &g_CurrentConfig.FuelGrH, 0, 60000, printUint16_1000, adjustUint16, copyU16, commitConfig},
  {MPSTR("MJ/Kg opal"), VAR_ADVANCED, &g_CurrentConfig.FuelHeatValueMJ10, 0, 500, printUint16_10, adjustUint16, copyU16, commitConfig},
  {MPSTR("Automat rozpal"), VAR_ADVANCED, &g_CurrentConfig.FireStartMode, 0, 2, printUint8_values, adjustUint8, copyU8, commitConfig, {.strings = values_firestart_mode}},
  {MPSTR("Cykle rozpalania"), VAR_ADVANCED, &g_CurrentConfig.NumFireStartCycles, 0, 20, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("Zapalarka S max"), VAR_ADVANCED, &g_CurrentConfig.HeaterMaxRunTimeS, 0, 250, printUint8, adjustUint8, copyU8, commitConfig}, 

  
  {MPSTR("Rozp st nad CO"), VAR_ADVANCED, &g_CurrentConfig.FireDetExhDt10, 0, 250, printUint8_10, adjustUint8, copyU8, commitConfig}, 
  {MPSTR("Rozp wzrost TSp"), VAR_ADVANCED, &g_CurrentConfig.FireDetExhIncrD10, 0, 250, printUint8_10, adjustUint8, copyU8, commitConfig}, 
  {MPSTR("Rozp wzrost TCo"), VAR_ADVANCED, &g_CurrentConfig.FireDetCOIncr10, 0, 250, printUint8_10, adjustUint8, copyU8, commitConfig}, 
  {MPSTR("MAF skala"), VAR_ADVANCED, &g_DeviceConfig.AirFlowCoeff, 0, 256, printUint8, adjustUint8, copyU8, commitDevConfig}, 
  {MPSTR("Ster nadmuch"), VAR_ADVANCED, &g_CurrentConfig.AirControlMode, AIRCONTROL_NONE, AIRCONTROL_HITMISS3, printUint8_values, adjustUint8, copyU8, commitConfig, {.strings = values_flow_ctrl_mode }}, 
  
  
  {MPSTR("P0 cykl sek."), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec, 0, 3600, printUint16, adjustUint16, copyU16, commitConfig},
  {MPSTR("P0 podawanie"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P0].FuelSecT10, 0, 600, printFeedRate_WithHeatPower, adjustUint16, copyU16, commitConfig, {.ptr = &g_CurrentConfig.BurnConfigs[STATE_P0].CycleSec}},
  {MPSTR("P0 wegiel co"), VAR_ADVANCED, &g_CurrentConfig.P0FuelFreq, 1, 5, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P0 dmuchawa sek"), VAR_ADVANCED, &g_CurrentConfig.P0BlowerTime, 1, 240, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P0 dmuchawa"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P0].BlowerPower, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P0 flow"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P0].AirFlow, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P0 dmuchawa CZ"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P0].BlowerCycle, 0, 100, printUint8, adjustUint8, copyU8, commitConfig},
  
  {MPSTR("P1 cykl sek."), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P1].CycleSec, 0, 300, printUint16, adjustUint16, copyU16, commitConfig},
  {MPSTR("P1 podawanie"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P1].FuelSecT10, 0, 600, printFeedRate_WithHeatPower, adjustUint16, copyU16, commitConfig, {.ptr = &g_CurrentConfig.BurnConfigs[STATE_P1].CycleSec}},
  {MPSTR("P1 dmuchawa"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P1].BlowerPower, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P1 flow"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P1].AirFlow, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P1 dmuchawa CZ"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P1].BlowerCycle, 0, 100, printUint8, adjustUint8, copyU8, commitConfig},

  {MPSTR("P2 cykl sek."), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P2].CycleSec, 0, 300, printUint16, adjustUint16, copyU16, commitConfig},
  {MPSTR("P2 podawanie"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P2].FuelSecT10, 0, 600, printFeedRate_WithHeatPower, adjustUint16, copyU16, commitConfig, {.ptr = &g_CurrentConfig.BurnConfigs[STATE_P2].CycleSec}},
  {MPSTR("P2 dmuchawa"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P2].BlowerPower, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P2 flow"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P2].AirFlow, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("P2 dmuchawa CZ"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_P2].BlowerCycle, 0, 100, printUint8, adjustUint8, copyU8, commitConfig},

  {MPSTR("RO cykl sek."), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_FIRESTART].CycleSec, 0, 300, printUint16, adjustUint16, copyU16, commitConfig},
  {MPSTR("RO podawanie"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_FIRESTART].FuelSecT10, 0, 600, printFeedRate_WithHeatPower, adjustUint16, copyU16, commitConfig, {.ptr = &g_CurrentConfig.BurnConfigs[STATE_P2].CycleSec}},
  {MPSTR("RO dmuchawa"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_FIRESTART].BlowerPower, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("RO flow"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_FIRESTART].AirFlow, 0, 256, printUint8, adjustUint8, copyU8, commitConfig},
  {MPSTR("RO dmuchawa CZ"), VAR_ADVANCED, &g_CurrentConfig.BurnConfigs[STATE_FIRESTART].BlowerCycle, 0, 100, printUint8, adjustUint8, copyU8, commitConfig},

  {MPSTR("Wygasz po cykl"), VAR_ADVANCED, &g_CurrentConfig.P0CyclesBeforeStandby, 0, 50, printUint8, adjustUint8, copyU8, commitConfig},

  
  {MPSTR("Czuj. CO"), VAR_ADVANCED, TSENS_BOILER, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  {MPSTR("Czuj. CWU"), VAR_ADVANCED, TSENS_CWU, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  {MPSTR("Czuj. podajnika"), VAR_ADVANCED, TSENS_FEEDER, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  {MPSTR("Czuj. powrotu"),VAR_ADVANCED, TSENS_RETURN, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  {MPSTR("Czuj. T zewn"), VAR_ADVANCED, TSENS_EXTERNAL, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  {MPSTR("Czuj. CWU 2"),  VAR_ADVANCED, TSENS_CWU2, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  //{MPSTR("Czuj. dod #1"), VAR_ADVANCED, TSENS_USR1, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  //{MPSTR("Czuj. dod #2"), VAR_ADVANCED, TSENS_USR2, -1, 7, printDallasInfo, adjustInt, copyDallasInfo, commitDevConfig},
  {MPSTR("Wyczysc log"), VAR_ADVANCED, NULL, 0, 1, printVBoolSwitch, adjustBool, copyVBoolSwitch, s_clearLogs},
  {MPSTR("Zestaw ustawien"), VAR_ADVANCED, &g_DeviceConfig.SettingsBank, 0, 2, printUint8, adjustUint8, copyU8, commitDevConfig},
  {MPSTR("Ust.zaawansowane"), VAR_IMMEDIATE, 'W', 0, 1, NULL, adjustUIState, NULL, NULL},
  {MPSTR("Logi"), VAR_IMMEDIATE, 'L', 0, 1, NULL, adjustUIState, NULL, NULL},
  {MPSTR("Wyjdz"), VAR_IMMEDIATE, '0', 0, 1, NULL, adjustUIState, NULL, NULL},
  {MPSTR("Wyjdz"), VAR_IMMEDIATE | VAR_ADVANCED, '0', 0, 1, NULL, adjustUIState, NULL, NULL},
};

const uint8_t N_UI_VARIABLES = sizeof(UI_VARIABLES) / sizeof(TUIVarEntry);
const uint8_t N_UI_STATES = sizeof(UI_STATES) / sizeof(TUIStateEntry);




bool updateVariableFromString(uint8_t varIdx, const char* str) {
  const TUIVarEntry& ent = UI_VARIABLES[varIdx];
  void* editCopy = NULL;
  if (ent.Store != NULL) {
    editCopy = ent.Store(varIdx, ent.DataPtr, false);
  }
  if (editCopy == NULL) return false;
  if (ent.PrintTo == NULL) return false;
  ent.PrintTo(varIdx, editCopy, str, true);
  if (ent.Store != NULL) {
    ent.Store(varIdx, editCopy, true);
  }
}
