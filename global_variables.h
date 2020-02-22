#ifndef _GLOBAL_VARIABLES_H_INCLUDED_
#define _GLOBAL_VARIABLES_H_INCLUDED_

#include "burn_control.h"

#define PUMP_CO1 0
#define PUMP_CWU1 1
#define PUMP_CO2 2
#define PUMP_CIRC 3







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


#endif
