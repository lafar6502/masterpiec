#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Rotary.h"
#include <TimerThree.h>
#include "ui_handler.h"


LiquidCrystal_I2C lcd(0x27, 16, 2);


Rotary _rot(HW_ENCODER_PINA, HW_ENCODER_PINB);
int _encoderPos = 0;
volatile uint32_t _hbCountSinceLastEvent = 0;
bool _idleReported = false;
int _lastButtonState = LOW;
bool _holdReported = false;
void (*g_uiBottomHalf)(void*);
void* g_uiBottomHalfCtx;

void uiHeartbeatProc() 
{
  _hbCountSinceLastEvent++;
  if (g_uiBottomHalf != NULL) 
  {
    //bottom half is set up, let's wait
    if (_hbCountSinceLastEvent > 10000) {
      g_uiBottomHalf = NULL;
    }
    return;  
  }
  unsigned char result = _rot.process();
  if (result == DIR_CW) 
  {
    _encoderPos++;
    _idleReported = false;
    processUIEvent(UI_EV_UP, 1);
  } 
  else if (result == DIR_CCW) 
  {
    _encoderPos--;
    _idleReported = false;
    processUIEvent(UI_EV_DOWN, -1);
  }
  
  if (result != DIR_NONE) {
    
  }
  
  if (HW_ENCODER_PINBTN != 0) 
  {
    int v = digitalRead(HW_ENCODER_PINBTN);
    uint32_t c = _hbCountSinceLastEvent;
    if (v != _lastButtonState) 
    {
      _hbCountSinceLastEvent = 0;
      _holdReported = false;
      _idleReported = false;
      if (v == LOW) 
      {
        processUIEvent(UI_EV_BTNRELEASE, 0);
      } 
      else if (v == HIGH) 
      {
        processUIEvent(UI_EV_BTNPRESS, 0);
      }
    } 
    else 
    {
      if (!_holdReported && v == HIGH && c > 3000) 
      {
        _holdReported = true;
        processUIEvent(UI_EV_BTNHOLD, 0);
      }
    }
    _lastButtonState = v;
  }
  
  if (_hbCountSinceLastEvent > 5000 && !_idleReported) {
    processUIEvent(UI_EV_IDLE, 0);
    _idleReported = true;
  }
}

int32_t getEncoderPos() {
  return _encoderPos;
}

void initializeEncoder()
{
  _hbCountSinceLastEvent = 0;
  _idleReported = false;
  _encoderPos = 0;
  pinMode(HW_ENCODER_PINA, INPUT_PULLUP);
  pinMode(HW_ENCODER_PINB, INPUT_PULLUP);
  if (HW_ENCODER_PINBTN != 0) pinMode(HW_ENCODER_PINBTN, INPUT_PULLUP);
  
  Timer3.initialize(1000);  
  _rot = Rotary(HW_ENCODER_PINA, HW_ENCODER_PINB);
  Timer3.attachInterrupt(uiHeartbeatProc);
  
}

char* g_DisplayBuf[DISPLAY_TEXT_LINES];
char* g_DisplayBuf2[DISPLAY_TEXT_LINES];

void initializeDisplay() {
  lcd.init();
  lcd.backlight();

  for(int i=0; i<DISPLAY_TEXT_LINES; i++) {
    g_DisplayBuf[i] = new char[DISPLAY_TEXT_LEN + 1];
    g_DisplayBuf2[i] = new char[DISPLAY_TEXT_LEN + 1];
  }
}

void clearDipslayBuf() {
  for(int i=0; i<DISPLAY_TEXT_LINES; i++) {
    memset(g_DisplayBuf[i], 0, DISPLAY_TEXT_LEN + 1);
  }
}

void eraseDisplayToEnd(char* buf) {
  bool f = false;
  for(int i=0; i<=DISPLAY_TEXT_LEN; i++) 
  {
    if (f) 
      buf[i] = ' ';
    else if (buf[i] == '\0') {
      f = true;
      buf[i] = ' ';
    }
  }
  buf[DISPLAY_TEXT_LEN] = 0;
}

void processUIEvent(uint8_t event, int8_t arg) 
{
  uint8_t cs = g_CurrentUIState;
  uint8_t cv = g_CurrentUIView;
  
  if (event != UI_EV_IDLE) 
  {
    _hbCountSinceLastEvent = 0;
    _idleReported = false;
  }

  //assert(g_CurrentUIState >= 0 && g_CurrentUIState < sizeof(UI_STATES) / sizeof(TUIStateEntry));

  if (UI_STATES[g_CurrentUIState].HandleEvent != NULL) UI_STATES[g_CurrentUIState].HandleEvent(event, arg);

  if (cs != g_CurrentUIState || cv != g_CurrentUIView) {
    Serial.print(F("ev:"));
    Serial.print(event);
    Serial.print(F(" St:"));
    Serial.print(g_CurrentUIState);
    Serial.print(F(" v:"));
    Serial.print(g_CurrentUIView);
    Serial.print(F(" enc:"));
    Serial.println(getEncoderPos());  
  }
  
}

void updateView() {
  
  //assert(g_CurrentUIState >= 0 && g_CurrentUIState < sizeof(UI_STATES) / sizeof(TUIStateEntry));
  if (UI_STATES[g_CurrentUIState].UpdateView != NULL) 
  {
    clearDipslayBuf();
    UI_STATES[g_CurrentUIState].UpdateView();
  }
  else 
  {
    if (UI_SCREENS[g_CurrentUIView].UpdateView != NULL) 
    {
      clearDipslayBuf();
      UI_SCREENS[g_CurrentUIView].UpdateView(g_CurrentUIView, g_DisplayBuf);
      eraseDisplayToEnd(g_DisplayBuf[0]);
      eraseDisplayToEnd(g_DisplayBuf[1]);
      if (strncmp(g_DisplayBuf[0], g_DisplayBuf2[0], DISPLAY_TEXT_LEN) != 0 || strncmp(g_DisplayBuf[1], g_DisplayBuf2[1], DISPLAY_TEXT_LEN) != 0)
      {
        memcpy(g_DisplayBuf2[0], g_DisplayBuf[0], DISPLAY_TEXT_LEN+1);
        memcpy(g_DisplayBuf2[1], g_DisplayBuf[1], DISPLAY_TEXT_LEN+1);
        
        lcd.setCursor(0,0);
        lcd.print(g_DisplayBuf2[0]);
        lcd.setCursor(0, 1);
        lcd.print(g_DisplayBuf2[1]);
      }
      
    }
  }
}

void changeUIState(char code) {
  uint8_t oldState = g_CurrentUIState;
  for(int i=0; i<N_UI_STATES; i++) {
    if (UI_STATES[i].Code == code) {
      g_CurrentUIState = i;
      g_CurrentUIView = UI_STATES[i].DefaultView;
      if (UI_STATES[g_CurrentUIState].HandleEvent != NULL) UI_STATES[g_CurrentUIState].HandleEvent(UI_EV_INITSTATE, oldState);
      Serial.print(F("ui state:"));
      Serial.print(g_CurrentUIState);
      Serial.print(F(", v "));
      Serial.println(g_CurrentUIView);
      return;
    }
  }
  Serial.print(F("ui state not found: "));
  Serial.println(code);
}
