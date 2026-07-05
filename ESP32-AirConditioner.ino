/////// Pairing Code  112-23-344
#include "HomeSpan.h"
#include "IRController.h"
#include "FanAccessory.h"
#include "CustomWebInterface.h"
#include "ThermostatAccessory.h"
#include "DryingShutdownAccessory.h"

#define STATUS_LED_PIN 48     // pin for status LED
const uint16_t sendPin = 4;   // Define the GPIO pin for the IR LED
const uint16_t recvPin = 15;  // Pin where the IR receiver is connected
const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 2048;
const uint8_t kTimeout = 15;


FanAccessory *fanAccessory = nullptr;
ThermostatAccessory *thermostatAccessory = nullptr;
DryingShutdownAccessory *dryingAccessory = nullptr;
IRController irController(sendPin, recvPin, kCaptureBufferSize, kTimeout, true);

void setup() {
  Serial.begin(kBaudRate);

  irController.beginReceive();
  irController.beginSend();
  homeSpan.setApSSID("ESP32 Ac Controller");
  homeSpan.setApPassword("123456789");
  homeSpan.setStatusPixel(STATUS_LED_PIN, 240, 100, 5);
  homeSpan.setHostNameSuffix("-ac-controller");

  homeSpan.setPortNum(1201);  // change port number for HomeSpan so we can use port 80 for the Web Server
  // homeSpan.enableOTA();       // enable OTA updates
  homeSpan.setWifiCallback(setWebInterface);
  homeSpan.begin(Category::Bridges, "AC_Bridge");

  // Set a custom HomeKit pairing code
  homeSpan.setPairingCode("11223344");

  // homeSpan.enableWebLog(10, "pool.ntp.org", "UTC+3");
  // homeSpan.setApTimeout(300);
  homeSpan.enableAutoStartAP();


  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();

  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::Name("Air Conditioner");
  new Characteristic::Model("ESP32 AC Model");
  new Characteristic::FirmwareRevision("1.0.0");
  thermostatAccessory = new ThermostatAccessory(&irController);
  fanAccessory = new FanAccessory(&irController);

  if (irController.isDryingBeforeShutdownEnabled()) {
    dryingAccessory = new DryingShutdownAccessory(&irController);
  }
}

void loop() {
  homeSpan.poll();
  webServerLoop();
}

void setWebInterface() {
  setupWebInterface(irController);
}