#ifndef _BURN_CONTROL_H_INCLUDED_
#define _BURN_CONTROL_H_INCLUDED_

typedef uint8_t TSTATE;

#define STATE_P0 0   //podtrzymanie
#define STATE_P1 1   //grzanie z mocą minimalną
#define STATE_P2 2   //grzanie z mocą nominalną
#define STATE_STOP 3 //tryb ręczny - zatrzymany piec - sterowanie automatyczne powinno zaprzestać działalności 
#define STATE_ALARM 4 // alarm - cos się stało, piec zatrzymany albo włączone zabezpieczenie
#define STATE_REDUCE1 5 //tryb przejścia na niższy stan P2 => P1 => P0. zadaniem tego trybu jest dopalenie pozostałego węgla. W tym celu musimy wiedzieć z jakiego stanu wyszlismy do reduce
#define STATE_REDUCE2 6 //tryb przejścia na niższy stan P1 => P0 
#define STATE_UNDEFINED 255

#define MAX_POWER_STATES 3 //max liczba konfiguracji dla mocy. 1 - tylko podtrzymanie, 2 - podtrzymanie i praca, 3 - podtrzymanie i 2 moce pracy



//konfiguracja jednego z poziomów mocy
typedef struct BurnParams {
    uint16_t CycleSec; //total cycle, including the fueling time, in seconds. podtrzymanie: okres przedmuchu. podawanie wegla co 1, 2, 3 przedmuchy
    //czas podawania wegla, * 10 (50 = 5 sekund)
    uint16_t FuelSecT10;
    //moc nadmuchu
    uint8_t BlowerPower;
    uint8_t BlowerCycle; //cykl dmuchawy dla zasilania grupowego. 0 gdy fazowe.
} TBurnParams;


//zestaw ustawień pieca (aktualna konfiguracja). Nie zawiera bieżących wartości.
typedef struct ControlConfiguration {
  uint16_t Magic; //should always be 0x6502
  uint8_t TCO; //co temp 
  uint8_t TCWU;  //cwu temp
  uint8_t TCWU2; //cwu2 temp
  uint8_t TMinPomp; //minimum pump run temp.
  uint8_t THistCwu; //histereza cwu
  uint8_t THistCO;  //histereza co
  uint8_t TDeltaCO; //delta co - temp powyzej zadanej przy ktorej przejdzie w podtrzymanie
  uint8_t TDeltaCWU; //delta cwu - temp powyżej bojlera do ktorej rozgrzewamy piec
  uint8_t P0BlowerTime; //czas pracy dmuchawy w podtrzymaniu
  uint8_t P0FuelFreq; //podawanie wegla co x cykli przedmuchu
  bool    SummerMode; //tryb letni
  bool    HomeThermostat;
  TBurnParams BurnConfigs[MAX_POWER_STATES]; //first one [0] is the podtrzymanie
  uint8_t DallasAddress[8][8]; //dallas sensor addresses. if zero - sensor not present
  
} TControlConfiguration;

//bieżąca konfiguracja pieca
extern TControlConfiguration g_CurrentConfig;

typedef struct BurnTransition {
  TSTATE From;
  TSTATE To;
  bool (*fCondition)();
  void (*fAction)(); //akcja wykonywana przy tym przejściu
} TBurnTransition;

typedef struct BurnStateConfig {
  TSTATE State;
  char Code;
  void (*fInitialize)(TSTATE s);
  void (*fLoop)();
} TBurnStateConfig;

//initialize the burning automatic algorithm
void initializeBurningLoop();
//periodically run burn control task to handle automatic burning alg.
void burnControlTask();
//wlacz/wylacz tryb automatyczny
void setManualControlMode(bool b);
//czy mamy tryb automatyczny
bool getManualControlMode();
void forceState(TSTATE st);

extern const TBurnTransition  BURN_TRANSITIONS[];
extern const TBurnStateConfig BURN_STATES[];
extern const uint8_t N_BURN_TRANSITIONS;
extern const uint8_t N_BURN_STATES;
extern TSTATE g_BurnState;


//czas wejscia w bieżący BURN stan, ms
extern unsigned long g_CurStateStart;
extern float  g_CurStateStartTempCO; //temp pieca w momencie wejscia w bież. stan.
extern unsigned long g_CurBurnCycleStart; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia

//
// Globalne zmienne reprezentujące bieżący stan pieca, temperatury, dmuchawy itp
// po to żeby np moduł UI mógł sobie je wyswietlać. 
//

extern float g_AktTempZadana; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
extern float g_TempCO;
extern float g_TempCWU; 
extern float g_TempPowrot;  //akt. temp. powrotu
extern float g_TempSpaliny; //akt. temp. spalin
extern float g_TempFeeder;
extern float g_TempBurner; //temp palnika

extern TSTATE g_BurnState;  //aktualny stan grzania
extern bool   g_HomeThermostatOn;  //true - termostat pokojowy kazał zaprzestać grzania
extern float g_TempZewn; //aktualna temp. zewn


#endif