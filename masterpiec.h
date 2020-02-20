#ifndef _MASTERPIEC_H_INCLUDED_
#define _MASTERPIEC_H_INCLUDED_

#include "global_variables.h"




typedef struct BurnTransition {
  TSTATE From;
  TSTATE To;
  bool (*fCondition)();
  void (*fAction)(); //akcja wykonywana przy tym przejściu
} TBurnTransition;

typedef struct BurnStateConfig {
  TSTATE State;
  char Code;
  void (*fLoop)();
  void (*fInitialize)();
} TBurnStateConfig;

extern const TBurnTransition  BURN_TRANSITIONS[];
extern const TBurnStateConfig BURN_STATES[];
extern TSTATE g_BurnState;

//funkcja wołana cykliczne w celu obsługi 
//procesu palenia (sterowanie podajnikiem, dmuchawą, przejścia miedzy mocami)
void burningProc();

//funkcja wołana cyklicznie w celu obsługi zmiany stanu pomp
void updatePumpStatus();

//funkcja wołana cyklicznie w celu aktualizacji wyswietlacza
void updateUI();


//odczytuje wskazania sensorów i aktualizuje zmienne na tej podstawie
void processSensorValues();

//restore saved configuration on program init
//from a specified slot. 0 is the default slot
void eepromRestoreConfig(uint8_t configSlot);

//store current configuration in specified config slot
void eepromSaveConfig(uint8_t configSlot);

//reset config to default
void eepromResetConfig(uint8_t configSlot);

///przejscie w dany stan bez sprawdzania war. początkowych
void forceState(TSTATE state);

#endif
