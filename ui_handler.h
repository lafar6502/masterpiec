#ifndef _UI_HANDLER_H_INCLUDED_
#define _UI_HANDLER_H_INCLUDED_

//ui - display and user input handlers

//zdarzenia ui
#define UI_EV_DOWN 2   //w lewo (dodatkowy parametr = increment)
#define UI_EV_UP 3     //w prawo (dodatkowy parametry = increment)
#define UI_EV_BTNPRESS 4 //button pressed. dodatkowy parametr = który
#define UI_EV_BTNDBLCLICK 5   //double button click (first btnpress is reported, then btndblclick)
#define UI_EV_IDLE  6     //reported once after several seconds of inactivity
#define UI_EV_BTNRELEASE 7 //reported when button is released
#define UI_EV_BTNHOLD 8 //hold a button for 3 sec

void processUIEvent(uint8_t event, int8_t arg);


void initializeEncoder(uint8_t inputAPin, uint8_t inputBPin, uint8_t buttonPin);
void initializeDisplay();
void updateView();

int32_t getEncoderPos();


typedef struct UIStateEntry {
  char Code;
  void* Ctx;
  void (*HandleEvent)(uint8_t event, uint8_t arg);
  void (*UpdateView)();
  
} TUIStateEntry;

typedef struct UIScreenEntry {
  char Code;
  void (*UpdateView)(uint8_t screen);
  void* Ctx;
} TUIScreenEntry;

extern const TUIStateEntry UI_STATES[];
extern const TUIScreenEntry UI_SCREENS[];

#define VAR_EDITABLE 1
#define VAR_ADVANCED 2
#define VAR_CONFIG  4 //variable is a configuration entry
#define VAR_IMMEDIATE 8 //variable is adjusted immediately, without save

typedef struct UIVariableEntry {
  char* Name;
  uint16_t Flags;
  void* Ptr; //variable pointer or some other context data
  void (*PrintTo)(void*, char* buf, uint8_t len);
} TUIVarEntry;

extern const TUIVarEntry UI_VARIABLES[];

extern uint16_t g_CurrentlyEditedVariable;
extern uint8_t g_CurrentUIState;
extern uint8_t g_CurrentUIView;

#endif
