#ifndef _UI_HANDLER_H_INCLUDED_
#define _UI_HANDLER_H_INCLUDED_

#include "hwsetup.h"

//ui - display and user input handlers

//zdarzenia ui
#define UI_EV_DOWN 2   //w lewo (dodatkowy parametr = increment)
#define UI_EV_UP 3     //w prawo (dodatkowy parametry = increment)
#define UI_EV_BTNPRESS 4 //button pressed. dodatkowy parametr = który
#define UI_EV_BTNDBLCLICK 5   //double button click (first btnpress is reported, then btndblclick)
#define UI_EV_IDLE  6     //reported once after several seconds of inactivity
#define UI_EV_BTNRELEASE 7 //reported when button is released
#define UI_EV_BTNHOLD 8 //hold a button for 3 sec
#define UI_EV_INITSTATE 9 //just for initing new ui state

void processUIEvent(uint8_t event, int8_t arg);


void initializeEncoder();
void initializeDisplay();

//this is supposed to be called from the main loop
//to update the ui view
void updateView();

int32_t getEncoderPos();

typedef bool (*BoolFun)();
typedef void (*SetBoolFun)(bool);
typedef uint8_t (*U8Fun)();
typedef void (*SetU8Fun)(uint8_t);
typedef uint16_t (*U16Fun)();
typedef void (*SetU16Fun)(uint16_t);
typedef float (*FFun)();
typedef void (*SetFFun)(float);


typedef union {
  void* ptr;
  SetBoolFun setBoolF;
  SetU8Fun  setU8F;
  SetU16Fun setU16F;
  SetFFun setFF; 
  uint8_t u8V;
  uint16_t u16V;
  int   numV;
  float fV;
} VarHolder;

typedef struct UIStateEntry {
  char Code;
  VarHolder Data;
  uint8_t DefaultView;
  void (*HandleEvent)(uint8_t event, uint8_t arg);
  void (*UpdateView_St)();
} TUIStateEntry;

typedef struct UIScreenEntry {
  char Code;
  VarHolder Data;
  //lines - array of text line buffers that will be passed to you. You can sprint to these lines. Warning: max number of lines depends on the display DISPLAY_TEXT_LINES DISPLAY_TEXT_LEN
  void (*UpdateView)(uint8_t screen, char* lines[]);
  
} TUIScreenEntry;

extern const TUIStateEntry UI_STATES[];
extern const TUIScreenEntry UI_SCREENS[];
extern const uint8_t N_UI_SCREENS;
extern const uint8_t N_UI_STATES;
extern const uint8_t N_UI_VARIABLES;

extern char* g_DisplayBuf[];

#define VAR_EDITABLE 1
#define VAR_ADVANCED 2
#define VAR_CONFIG  4 //variable is a configuration entry
#define VAR_IMMEDIATE 8 //variable is adjusted immediately, without save
#define VAR_INPLACE 16 //variable can be edited in place, without making a temporary copy

typedef struct UIVariableEntry {
  char Name[20]; //we could declare fixed size array and put everything in progmem. names will never be longer than display width - 16 or 20
  uint16_t Flags; //flagi
  void* DataPtr; //variable pointer or some other context data
  float Min; //minimum val.
  float Max; //max val
  void (*PrintTo)(uint8_t varIdx, void* dataPtr, char* buf, bool parseString); //function to print the value to the buffer. parseString -> reverse operation, buf has string data and dataPtr is place to put the parsed value
  void (*Adjust)(uint8_t varIdx, void* dataPtr, int8_t increment); //call to adjust changes the value by specified increment. commit - saves the value immediately, false - we're just editing a temp value. call with increment=0, commit=false -> we cancel the edit. call with increment=0,commit=true to save the edit
  void* (*Store)(uint8_t varIdx, void* dataPtr, uint8_t save); //call to commit the value(store). if save == false then we want to cancel edit and return to original value.
  void (*Commit)(uint8_t varIdx); //additional function to call after saving
  VarHolder Data;
} TUIVarEntry;

extern const TUIVarEntry UI_VARIABLES[];

extern uint16_t g_CurrentlyEditedVariable;
extern uint8_t g_CurrentUIState;
extern uint8_t g_CurrentUIView;

extern void (*g_uiBottomHalf)(void*);
extern void* g_uiBottomHalfCtx;


void changeUIState(char code);

void updateVariableFromText(uint8_t varIdx, const char* valueText);

#endif
