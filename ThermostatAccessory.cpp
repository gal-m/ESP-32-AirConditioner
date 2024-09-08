
#include "ThermostatAccessory.h"
#include <DHT.h>
#define DHT_PIN 16  // DHT11 sensor pin
#define DHT_TYPE DHT11

ThermostatAccessory::ThermostatAccessory(IRController *irCtrl)
  : Service::Thermostat(), irController(irCtrl) {

  dht = new DHT(DHT_PIN, DHT_TYPE);
  dht->begin();
  currentTemp = new Characteristic::CurrentTemperature(0);
  unit = new Characteristic::TemperatureDisplayUnits(0, true);
  targetTemp = new Characteristic::TargetTemperature(22, true);
  currentHumidity = new Characteristic::CurrentRelativeHumidity(0);
  targetState = new Characteristic::TargetHeatingCoolingState(0, true);
  currentState = new Characteristic::CurrentHeatingCoolingState(0, true);
  targetTemp->setRange(16, 31, 1);
  irController->setThermostatCharacteristics(targetState, targetTemp);
}

void ThermostatAccessory::loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastReadTime >= readInterval) {
    lastReadTime = currentTime;
    readTemperatureAndHumidity();
  }
  irController->handleIR();
}

void ThermostatAccessory::readTemperatureAndHumidity() {
  float temperature = dht->readTemperature();
  float humidity = dht->readHumidity();

  if (!isnan(temperature)) {
    currentTemp->setVal(temperature);
  }

  if (!isnan(humidity)) {
    currentHumidity->setVal(humidity);
  }
}

boolean ThermostatAccessory::update() {
  bool power = targetState->getNewVal() != 0;
  int mode = targetState->getNewVal();
  int temp = targetTemp->getNewVal();
  irController->sendThermostatCommand(power, mode, temp);
  return true;
}
