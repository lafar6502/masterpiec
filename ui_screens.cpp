#include <arduino.h>
#include "global_variables.h"
#include "ui_handler.h"
#include "boiler_control.h"
#include <MD_DS1307.h>

#define UISTATE_MAIN 0
#define UISTATE_STARTUP 1


uint16_t g_CurrentlyEditedVariable;
uint8_t g_CurrentUIState = UISTATE_MAIN;
uint8_t g_CurrentUIView = 1;


typedef union TempVarData {
  
} TTempBuffer;

void scrSplash(uint8_t idx, char* lines[])
{
  
  sprintf(lines[0], "MASTERPIEC");
  sprintf(lines[1], "v 0.0.1 LAFAR6502");
}

void scrDefault(uint8_t idx, char* lines[]) 
{
  char buf1[10], buf2[10], buf3[10];
  dtostrf(g_AktTempZadana,2, 0, buf1);
  dtostrf(g_TempCO,3, 1, buf2);
  sprintf(buf3, "CW:");
  dtostrf(g_TempCWU,3, 1, buf3);
  
  sprintf(lines[0], "T:%s/%s  B:%s", buf2, buf1, buf3);
  
  sprintf(lines[1], "S%c %d%%", BURN_STATES[g_BurnState].Code, getCurrentBlowerPower()); 
  
}

void scrTime(uint8_t idx, char* lines[]) {
  sprintf(lines[0], "%04d.%02d.%02d      ", RTC.yyyy, RTC.mm, RTC.dd, RTC.h, RTC.m);
  sprintf(lines[1], "%02d:%02d:%02d      ", RTC.h, RTC.m, RTC.s);
}

void scrDefaultEventHandler(uint8_t ev, uint8_t arg) {
  
  if (ev == UI_EV_UP) {
    g_CurrentUIView = (g_CurrentUIView + 1) % N_UI_SCREENS;
  } else if (ev == UI_EV_DOWN) {
    g_CurrentUIView = g_CurrentUIView < 1 ? N_UI_SCREENS - 1 : g_CurrentUIView - 1;
  }
  else if (ev == UI_EV_BTNPRESS) {
    
  }
}

void scrSelectVariableHandler(uint8_t ev, uint8_t arg) 
{
  const TUIStateEntry* ps = UI_STATES + g_CurrentUIState;
  
}

void scrSelectVariablePrint()
{
  const TUIStateEntry* ps = UI_STATES + g_CurrentUIState;
}

void adjustUint8(uint8_t varIdx, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  uint8_t* pd = (uint8_t*) pv->DataPtr;
  uint8_t v2 = *pd + increment;
  if (v2 < pv->Min) v2 = (uint8_t) pv->Max;
  if (v2 > pv->Max) v2 = (uint8_t) pv->Min;
  *pd = v2;
}

void printUint8(uint8_t varIdx, char* buf) {
  uint8_t* pv = (uint8_t*) UI_VARIABLES[varIdx].DataPtr;
  sprintf(buf, "%d", *pv);
}

void commitTime(uint8_t varIdx, bool save) {
  if (save)
    RTC.writeTime();
  else
    RTC.readTime();
}

const TUIStateEntry UI_STATES[] = {
    {'0', NULL, scrDefaultEventHandler, NULL},
    {'V', {.NumVal=0}, scrSelectVariableHandler, NULL},
    {'W', {.NumVal=1}, scrSelectVariableHandler, NULL},
    
};

const TUIScreenEntry UI_SCREENS[] = {
    {'\0', scrSplash, NULL },
    {'1', scrDefault, NULL },
    {'T', scrTime, NULL},
};

const uint8_t N_UI_SCREENS = sizeof(UI_SCREENS) / sizeof(TUIScreenEntry);

const TUIVarEntry UI_VARIABLES[] = {
  {"Rok", 0, &RTC.yyyy, 2019, 3000, printUint8, adjustUint8, commitTime},
  {"Miesiac", 0, &RTC.mm, 1, 12, printUint8, adjustUint8, commitTime},
  {"Dzien", 0, &RTC.dd, 1, 31, printUint8, adjustUint8, commitTime},
  {"Godzina", 0, &RTC.h, 1, 23, printUint8, adjustUint8, commitTime},
  {"Minuta", 0, &RTC.m, 1, 59, printUint8, adjustUint8, commitTime},
  
  {NULL, 0, NULL, 0, 0, NULL, NULL}
};

const uint8_t N_UI_VARIABLES = sizeof(UI_VARIABLES) / sizeof(TUIVarEntry);
const uint8_t N_UI_STATES = sizeof(UI_STATES) / sizeof(TUIStateEntry);
