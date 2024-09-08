#ifndef THERMOSTAT_ACCESSORY_H
#define THERMOSTAT_ACCESSORY_H

#include <DHT.h>
#include "HomeSpan.h"
#include "IRController.h"



class ThermostatAccessory : public Service::Thermostat {
private:
  DHT *dht;
  IRController *irController;
  SpanCharacteristic *unit;
  SpanCharacteristic *targetTemp;
  SpanCharacteristic *targetState;
  SpanCharacteristic *currentTemp;
  SpanCharacteristic *currentState;
  SpanCharacteristic *currentHumidity;
  unsigned long lastReadTime = 0;
  const unsigned long readInterval = 2000;

public:
  void loop();
  boolean update();
  void readTemperatureAndHumidity();
  ThermostatAccessory(IRController *irCtrl);
};

#endif