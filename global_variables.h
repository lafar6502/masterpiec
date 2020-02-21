#ifndef _GLOBAL_VARIABLES_H_INCLUDED_
#define _GLOBAL_VARIABLES_H_INCLUDED_

typedef uint8_t TSTATE;

#define PUMP_CO1 0
#define PUMP_CWU1 1
#define PUMP_CO2 2
#define PUMP_CIRC 3





#define STATE_UNDEFINED 0
#define STATE_P0 1   //podtrzymanie
#define STATE_P1 2   //grzanie z mocą minimalną
#define STATE_P2 3   //grzanie z mocą nominalną
#define STATE_STOP 5 //tryb ręczny - zatrzymany piec - sterowanie automatyczne powinno zaprzestać działalności 
#define STATE_ALARM 6 // alarm - cos się stało, piec zatrzymany albo włączone zabezpieczenie
#define STATE_REDUCE 7 //tryb przejścia na niższy stan P2 => P1 => P0. zadaniem tego trybu jest dopalenie pozostałego węgla. W tym celu musimy wiedzieć z jakiego stanu wyszlismy do reduce

#define MAX_POWER_STATES 3 //max liczba konfiguracji dla mocy. 1 - tylko podtrzymanie, 2 - podtrzymanie i praca, 3 - podtrzymanie i 2 moce pracy



//konfiguracja jednego z poziomów mocy
typedef struct BurnParams {
    uint8_t CycleSec;
    //czas podawania wegla, * 10 (50 = 5 sekund)
    uint8_t FuelSecT10;
    //moc nadmuchu
    uint8_t BlowerPower;
    uint8_t BlowerCycle; //cykl dmuchawy dla zasilania grupowego. 0 gdy fazowe.
} TBurnParams;


//zestaw ustawień pieca (aktualna konfiguracja). Nie zawiera bieżących wartości.
typedef struct ControlConfiguration {
  uint8_t TCO;
  uint8_t TCWU;
  uint8_t TMinPomp;
  uint8_t THistCwu; //histereza cwu
  uint8_t THistCO;  //histereza co
  uint8_t TDeltaCO; //delta co - temp powyzej zadanej przy ktorej przejdzie w podtrzymanie
  uint8_t TDeltaCWU; //delta cwu - temp powyżej bojlera do ktorej rozgrzewamy piec
  uint16_t T10PodtrzymaniePrzedmuch; //czas cyklu przedmuchu w podtrzymaniu, * 10 (100 = 10 sek)
  uint16_t T10PodtrzymanieDmuch; //czas pracy dmuchawy w podtrzymaniu
  uint8_t P0MocDm; //P0 moc dmuchawy w podtrzymaniu
  
  TBurnParams BurnConfigs[MAX_POWER_STATES];
  
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
  void (*fLoop)();
  void (*fInitialize)();
} TBurnStateConfig;

extern const TBurnTransition  BURN_TRANSITIONS[];
extern const TBurnStateConfig BURN_STATES[];
extern TSTATE g_BurnState;



//
// Globalne zmienne reprezentujące bieżący stan pieca, temperatury, dmuchawy itp
// po to żeby np moduł UI mógł sobie je wyswietlać. 
//

extern float g_AktTempZadana; //aktualnie zadana temperatura pieca (która może być wyższa od temp. zadanej w konfiguracji bo np grzejemy CWU)
extern float g_TempCO;
extern float g_TempCWU; 
extern float g_TempPowrot;  //akt. temp. powrotu
extern float g_TempSpaliny; //akt. temp. spalin
extern float g_TempPodajnik;
extern TSTATE g_BurnState;  //aktualny stan grzania
extern bool   g_TermostatStop;  //true - termostat pokojowy kazał zaprzestać grzania
extern float g_TempZewn; //aktualna temp. zewn

//czas wejscia w bieżący stan, ms
extern unsigned long g_CurStateStart;
extern float  g_CurStateStartTempCO; //temp pieca w momencie wejscia w bież. stan.
extern unsigned long g_CurBurnCycleStart; //timestamp, w ms, w ktorym rozpoczelismy akt. cykl palenia
extern uint8_t  g_CurrentBlowerPower;

#endif
