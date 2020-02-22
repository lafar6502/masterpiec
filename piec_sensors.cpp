#include <OneWire.h>
#include <DallasTemperature.h>
#include <Thermocouple.h>
#include <MAX6675_Thermocouple.h>
#include "hwsetup.h"
#include "piec_sensors.h"



struct TDallasSensor {
  DeviceAddress Addr;
  float LastValue;
  int   LastReadMs;
  bool  Active;
};

struct TThermocoupleSensor {
  Thermocouple* Sensor;
  float LastValue;
  int   LastReadMs;
};

OneWire oneWire(DALLAS_SENSOR_DATA_PIN);
DallasTemperature sensors(&oneWire);  

static TDallasSensor g_dallasSensors[MAX_DALLAS_SENSORS];
static TThermocoupleSensor g_thermocouples[MAX_THERMOCOUPLES];

void getDallasAddress(uint8_t idx, uint8_t buf[])
{
  if (idx >= MAX_DALLAS_SENSORS) return;
  memcpy(buf, g_dallasSensors[idx].Addr, 8);
}

void swapDallasAddress(uint8_t idx1, uint8_t idx2)
{
  if (idx1 >= MAX_DALLAS_SENSORS || idx2 >= MAX_DALLAS_SENSORS) return;
  if (idx1 == idx2) return;
  TDallasSensor tmp = g_dallasSensors[idx1];
  g_dallasSensors[idx1] = g_dallasSensors[idx2];
  g_dallasSensors[idx2] = tmp;
}

bool ensureDallasSensorAtIndex(uint8_t idx, uint8_t addr[8]) 
{
  if (idx >= MAX_DALLAS_SENSORS) return false;
  for(uint8_t i=0; i<MAX_DALLAS_SENSORS; i++) 
  {
    if (memcmp(g_dallasSensors[i].Addr, addr, 8) == 0) {
      if (i == idx) return true;
      swapDallasAddress(idx, i);
      return true;
    } 
  }
}

void printAddress(DeviceAddress d)
{ 
  char buf[17];
  sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X", d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
  
  Serial.println(buf);
}

void printDallasInfo(uint8_t idx, char* buf)
{
  uint8_t* d  = g_dallasSensors[idx].Addr;
  sprintf(buf, "%d%c%02X%02X%02X%02X%02X %d", idx, g_dallasSensors[idx].Active ? '.':'!', d[3], d[4], d[5], d[6], d[7], g_dallasSensors[idx].Active ? (uint8_t) g_dallasSensors[idx].LastValue * 10 : 0);
}

int findDallasIndex(uint8_t addr[8])
{
  for(uint8_t i=0; i<MAX_DALLAS_SENSORS; i++) {
    if (memcmp(addr, g_dallasSensors[i].Addr, 8) == 0) return i;
  }
  return -1;
}

void initializeDallasSensors() {
  sensors.begin();

  Serial.print("Scanning OneWire Dallas sensors...");
  Serial.print("Found ");
  int deviceCount = sensors.getDeviceCount();
  Serial.print(deviceCount, DEC);
  Serial.println(" devices.");
  Serial.println("");
  DeviceAddress tmp;
  sensors.setWaitForConversion(false);  
  Serial.println("dallas requesting temp");
  int m = millis();
  sensors.requestTemperatures();
  for (int i = 0;  i < deviceCount;  i++)
  {
    Serial.print("Sensor ");
    Serial.print(i+1);
    Serial.print(". ");
    sensors.getAddress(tmp, i);
    sensors.setResolution(tmp, 10);
    float tempC = sensors.getTempC(tmp);
    Serial.print("T=");
    Serial.print(tempC);
    Serial.print(" : ");
    printAddress(tmp);
    memcpy(g_dallasSensors[i].Addr, tmp, 8);
    g_dallasSensors[i].Active = true;
    g_dallasSensors[i].LastValue = tempC;
    g_dallasSensors[i].LastReadMs = m;
  }
  int m2 = millis() - m;
  Serial.print("Dallas scan done. time ms: ");
  Serial.println(m2);
}



void initializeMax6675Sensors() 
{
  g_thermocouples[0].Sensor = new MAX6675_Thermocouple(MAX6675_0_SCK_PIN, MAX6675_0_CS_PIN, MAX6675_0_SO_PIN);
  g_thermocouples[1].Sensor = MAX6675_1_SCK_PIN == 0 ? NULL : new MAX6675_Thermocouple(MAX6675_1_SCK_PIN, MAX6675_1_CS_PIN, MAX6675_1_SO_PIN);
  
  for (int i=0; i<sizeof(g_thermocouples)/sizeof(TThermocoupleSensor); i++) {
    if (g_thermocouples[i].Sensor != NULL) {
      int m = millis();
      double celsius = g_thermocouples[i].Sensor->readCelsius();
      int m2 = millis() - m;
      Serial.print("MAX6675/");
      Serial.print(i);
      Serial.print(" T:");
      Serial.print(celsius);
      Serial.print(", ms:");
      Serial.println(m2);
    }
  }
}


void refreshSensorReadings() {
  unsigned long m0 = millis();
  static unsigned long last_tc_read=0;
  static unsigned long last_da_read = 0;
  if (m0 - last_da_read > 1000)
  {
    for(int i=0; i<sizeof(g_dallasSensors) / sizeof(TDallasSensor); i++) {
      if (g_dallasSensors[i].Active) {
        g_dallasSensors[i].LastValue = sensors.getTempC(g_dallasSensors[i].Addr);
        if (g_dallasSensors[i].LastValue == DEVICE_DISCONNECTED_C) {
          g_dallasSensors[i].Active = false;
          Serial.print("Dallas sensor disconnected ");
          Serial.print(i);
          printAddress(g_dallasSensors[i].Addr);
        }
        g_dallasSensors[i].LastReadMs = m0;
      }
    }
    sensors.requestTemperatures();
    last_da_read = m0;
  }
  
  if (m0 - last_tc_read > 5000)
  {
    for (int i=0; i<sizeof(g_thermocouples)/sizeof(TThermocoupleSensor); i++) 
    {
      if (g_thermocouples[i].Sensor != NULL) 
      {
        g_thermocouples[i].LastValue = g_thermocouples[i].Sensor->readCelsius();
        g_thermocouples[i].LastReadMs = m0;
      }
    }
    last_tc_read = m0;
  }
  
  //int m2 = millis();
  //Serial.print("sensors read. t:");
  //Serial.print(m2 - m0);
  //Serial.print(", t0:");
  //Serial.println(g_dallasSensors[0].LastValue);
}

bool isDallasEnabled(uint8_t idx) 
{
  if (idx >= sizeof(g_dallasSensors) / sizeof(TDallasSensor)) return false;
  return g_dallasSensors[idx].Active;
}

bool isThermocoupleEnabled(uint8_t idx)
{
    if (idx >= sizeof(g_thermocouples)/sizeof(TThermocoupleSensor)) return false;
    return g_thermocouples[idx].Sensor != NULL;
}

float getLastDallasValue(uint8_t idx) {
  if (idx >= sizeof(g_dallasSensors) / sizeof(TDallasSensor)) return 0.0;
  return g_dallasSensors[idx].LastValue;
}

float getLastThermocoupleValue(uint8_t idx)
{
    if (idx >= sizeof(g_thermocouples)/sizeof(TThermocoupleSensor)) return false;
    return g_thermocouples[idx].Sensor == NULL ? 0.0 : g_thermocouples[idx].LastValue;
}
