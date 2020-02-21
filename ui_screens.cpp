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

void scrDefaultEventHandler(uint8_t ev, uint8_t arg) {
  
  if (ev == UI_EV_UP) {
    g_CurrentUIView = (g_CurrentUIView + 1) % 2;
  } else if (ev == UI_EV_DOWN) {
    g_CurrentUIView = (g_CurrentUIView - 1) % 2;
  }
  else if (ev == UI_EV_BTNPRESS) {
    
  }
}

void adjustUint8(uint8_t varIdx, int8_t increment) {
  const TUIVarEntry* pv = UI_VARIABLES + varIdx;
  
}

void printUint8(uint8_t varIdx, char* buf) {
  
}

void commitTime(uint8_t varIdx) {
  RTC.writeTime();
}

const TUIStateEntry UI_STATES[] = {
    {'0', NULL, scrDefaultEventHandler, NULL}
};

const TUIScreenEntry UI_SCREENS[] = {
    {'\0', scrSplash, NULL },
    {'1', scrDefault, NULL }
};

const TUIVarEntry UI_VARIABLES[] = {
  {NULL, 0, NULL, 0, 0, NULL, NULL}
};
