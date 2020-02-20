#ifndef _MASTERPIEC_H_INCLUDED_
#define _MASTERPIEC_H_INCLUDED_

//zmienne globalne

//temperatura na piecu (woda w piecu)
double TPiec = 0.0;
//woda w bojlerze
double TCwu = 0.0;



typedef struct BurnParams {
    uint8_t CycleSec;
    //czas podawania wegla, * 10 (50 = 5 sekund)
    uint8_t FuelSecT10;
    //moc nadmuchu
    uint8_t BlowerPower;
    uint8_t BlowerCycle; //cykl dmuchawy dla zasilania grupowego. 0 gdy fazowe.
} TBurnParams;

//podtrzymanie - czas podawania, czestosc podawania, czas nadmuchu, czestosc nadmuchu, moc nadmuchu


typedef uint8_t TSTATE;

#define STATE_UNDEFINED 0
#define STATE_P0 1   //podtrzymanie
#define STATE_P1 2   //grzanie z mocą minimalną
#define STATE_P2 3   //grzanie z mocą nominalną
#define STATE_STOP 5 //zatrzymany piec (brak nadmuchu, stop pomp, stop podajnika, brak podtrzymania)
#define STATE_ALARM 6 // alarm - cos się stało, piec zatrzymany albo włączone zabezpieczenie

TSTATE CUR_STATE;  //aktualny stan pieca

#define MAX_POWER_STATES 3 //max liczba konfiguracji dla mocy. 1 - tylko podtrzymanie, 2 - podtrzymanie i praca, 3 - podtrzymanie i 2 moce pracy

void setPumpOn(uint8_t num);
void setPumpOff(uint8_t num);
bool isPumpOn(uint8_t num);
bool isPumpEnabled(uint8_t num);

//uruchomienie podajnika
void setFeederOn();
//zatrzymanie podajnika
void setFeederOff();
//czy podajnik działa
bool isFeederOn();

void setBlowerPower(uint8_t power);
uint8_t getCurrentBlowerPower();

//ostatni odczyt z podanego termometru
double getLastTempSensorRead(uint8_t sensor);
//czy termometr jest dostępny
bool   isTempSensorEnabled(uint8_t sensor);




#define PUMP_CO1 0
#define PUMP_CWU1 1
#define PUMP_CO2 2
#define PUMP_CO3 3

#define TEMP_BOILER 0   //czujnik temp wody
#define TEMP_CWU 1      //czujnik temp cwu
#define TEMP_RETURN 2   //czujnik temp powrotu
#define TEMP_EXHAUST 3  //czujnik spalin
#define TEMP_EXTERNAL 4 //czujnik temp zewn
#define TEMP_FUEL 5     //temp podajnika
#define TEMP_BURNER 6   //temp palnika
#define TEMP_USER1  7   //dodatkowy czujnik 1
#define TEMP_USER2  8   //dodatkowy czujnik 2

#define MAX_TEMP_SENSOR 10


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

//cyklicznie, na koncu cyklu - wysylamy prosbe o odczyt wartosci sensorow
void requestSensorUpdate();

//odczytuje wskazania sensorów i aktualizuje zmienne na tej podstawie
void readSensorValues();

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

extern TControlConfiguration g_CurrentConfig;
//restore saved configuration on program init
//from a specified slot. 0 is the default slot
void eepromRestoreConfig(uint8_t configSlot);

//store current configuration in specified config slot
void eepromSaveConfig(uint8_t configSlot);

//reset config to default
void eepromResetConfig(uint8_t configSlot);




#endif
