#ifndef _GLOBAL_VARIABLES_H_INCLUDED_
#define _GLOBAL_VARIABLES_H_INCLUDED_

#include "burn_control.h"

#define PUMP_CO1 0
#define PUMP_CWU1 1
#define PUMP_CO2 2
#define PUMP_CIRC 3

#define DAILY_LOG_ENTRIES 7

typedef struct {
  uint16_t FeederTotalSec;
  uint16_t P1TotalSec;
  uint16_t P2TotalSec;
} TDailyLogEntry;

extern TDailyLogEntry g_DailyLogEntries[];

void loggingInit();
void loggingTask();



#endif
