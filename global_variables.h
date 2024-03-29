#ifndef _GLOBAL_VARIABLES_H_INCLUDED_
#define _GLOBAL_VARIABLES_H_INCLUDED_

#include "burn_control.h"

#include "varholder.h"

#define DAILY_LOG_ENTRIES 7

typedef struct {
  uint16_t FeederTotalSec;
  uint16_t P1TotalSec2; //number of P1 seconds, divided by 2
  uint16_t P2TotalSec2; //number of P2 seconds, divided by 2
  uint16_t P0TotalSec2; //we're missing one bit here, so we store numbr of seconds div by 2
  uint8_t MDay; //day of month...1..31
} TDailyLogEntry;


typedef struct {
  unsigned long Ms;
  float Val;
} TReading;

typedef struct {
  unsigned long Ms;
  uint16_t Val;
} TIntReading;

extern TDailyLogEntry g_DailyLogEntries[];
extern CircularBuffer<float> g_lastCOReads;
extern CircularBuffer<float> g_lastBurnTransitions;

void loggingInit();
void clearDailyLogs();
void loggingTask();
float calculateHeatPowerFor(float feedTimePerCycle, int cycleLength);
float calculateFuelWeightKg(unsigned long feedTimeSec);
void commandHandlingTask();
void processCommand(char*);
bool updateVariableFromString(uint8_t varIdx, const char* str);
//czy mamy tryb automatyczny
bool getManualControlMode();

bool isFlowTooHigh();
bool isDebugTime();
#endif
