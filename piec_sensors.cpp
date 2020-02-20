#include <OneWire.h>
#include <DallasTemperature.h>
#include <Thermocouple.h>
#include <MAX6675_Thermocouple.h>

#include "piec_sensors.h"

#define SCK_PIN 52
#define CS_PIN 49
#define SO_PIN 50

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

void printAddress(DeviceAddress d)
{ 
  char buf[17];
  sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X", d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
  
  Serial.println(buf);
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
  g_thermocouples[0].Sensor = new MAX6675_Thermocouple(SCK_PIN, CS_PIN, SO_PIN);
  g_thermocouples[1].Sensor = NULL; //new MAX6675_Thermocouple(SCK_PIN, CS_PIN, SO_PIN);
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
  int m0 = millis();
  for(int i=0; i<sizeof(g_dallasSensors) / sizeof(TDallasSensor); i++) {
    if (g_dallasSensors[i].Active) {
      g_dallasSensors[i].LastValue = sensors.getTempC(g_dallasSensors[i].Addr);
      g_dallasSensors[i].LastReadMs = m0;
    }
  }

  for (int i=0; i<sizeof(g_thermocouples)/sizeof(TThermocoupleSensor); i++) 
  {
    if (g_thermocouples[i].Sensor != NULL) 
    {
      g_thermocouples[i].LastValue = g_thermocouples[i].Sensor->readCelsius();
      g_thermocouples[i].LastReadMs = m0;
    }
  }
  
  sensors.requestTemperatures();
  int m2 = millis();
  Serial.print("sensors read. t:");
  Serial.print(m2 - m0);
  Serial.print(", t0:");
  Serial.println(g_dallasSensors[0].LastValue);
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
