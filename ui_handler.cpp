#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Rotary.h"
#include <TimerThree.h>
#include "ui_handler.h"


LiquidCrystal_I2C lcd(0x27, 16, 2);


Rotary _rot(HW_ENCODER_PINA, HW_ENCODER_PINB);
int _encoderPos = 0;
uint8_t _encButtonPin = 0;
volatile uint32_t _hbCountSinceLastEvent = 0;
bool _idleReported = false;
int _lastButtonState = LOW;
bool _holdReported = false;

void uiHeartbeatProc() 
{
  _hbCountSinceLastEvent++;
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
  
  if (_encButtonPin != 0) 
  {
    int v = digitalRead(_encButtonPin);
    uint32_t c = _hbCountSinceLastEvent;
    if (v != _lastButtonState) 
    {
      _holdReported = false;
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
  
  if (_hbCountSinceLastEvent > 5000 && !_idleReported && _lastButtonState == LOW) {
    processUIEvent(UI_EV_IDLE, 0);
    _idleReported = true;
  }
}

int32_t getEncoderPos() {
  return _encoderPos;
}

void initializeEncoder(uint8_t inputAPin, uint8_t inputBPin, uint8_t buttonPin)
{
  _hbCountSinceLastEvent = 0;
  _idleReported = false;
  _encoderPos = 0;
  _encButtonPin = buttonPin;
  pinMode(inputAPin, INPUT_PULLUP);
  pinMode(inputBPin, INPUT_PULLUP);
  if (buttonPin != 0) pinMode(buttonPin, INPUT_PULLUP);
  
  Timer3.initialize(1000);  
  _rot = Rotary(inputAPin, inputBPin);
  Timer3.attachInterrupt(uiHeartbeatProc);
  
}

char* g_DisplayBuf[DISPLAY_TEXT_LINES];

void initializeDisplay() {
  lcd.init();
  lcd.backlight();

  for(int i=0; i<DISPLAY_TEXT_LINES; i++) {
    g_DisplayBuf[i] = new char[DISPLAY_TEXT_LEN + 1];
  }
}

void clearDipslayBuf() {
  for(int i=0; i<DISPLAY_TEXT_LINES; i++) {
    memset(g_DisplayBuf[i], 0, DISPLAY_TEXT_LEN + 1);
  }
}

void processUIEvent(uint8_t event, int8_t arg) 
{
  _hbCountSinceLastEvent = 0;
  uint8_t cs = g_CurrentUIState;
  uint8_t cv = g_CurrentUIView;
  
  if (event != UI_EV_IDLE) _idleReported = false;

  //assert(g_CurrentUIState >= 0 && g_CurrentUIState < sizeof(UI_STATES) / sizeof(TUIStateEntry));

  if (UI_STATES[g_CurrentUIState].HandleEvent != NULL) UI_STATES[g_CurrentUIState].HandleEvent(event, arg);

  if (cs != g_CurrentUIState || cv != g_CurrentUIView) {
    Serial.print("ev:");
    Serial.print(event);
    Serial.print(" St:");
    Serial.print(g_CurrentUIState);
    Serial.print(" v:");
    Serial.print(g_CurrentUIView);
    Serial.print(" enc:");
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
      lcd.setCursor(0,0);
      lcd.print(g_DisplayBuf[0]);
      lcd.setCursor(0, 1);
      lcd.print(g_DisplayBuf[1]);
    }
  }
}
