#ifndef IRCONTROLLER_H_
#define IRCONTROLLER_H_


#include <vector>
#include <IRac.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "HomeSpan.h"
#include <Preferences.h>
#include <IRremoteESP8266.h>

class IRController {
public:
  void handleIR();
  void beginSend();
  void beginReceive();
  String getProtocol();
  void completeShutdown();
  void saveDryingSettings();
  void loadDryingSettings();
  void saveIdentifiedProtocols();
  void loadIdentifiedProtocols();
  void deleteIdentifiedProtocols();
  void startDryingBeforeShutdown();
  int getDryingDelayMinutes() const;
  int getDryingDelayInSeconds() const;
  std::vector<String> identifiedProtocols;
  void saveProtocol(const char *protocol);
  void setProtocol(const String &protocol);
  bool isDryingBeforeShutdownEnabled() const;
  std::vector<String> getIdentifiedProtocols();
  void sendFanCommand(int fanSpeed, bool swing);
  void sendThermostatCommand(bool power, int mode, int temp);
  void enableDryingBeforeShutdown(bool enable, int delayMinutes);
  void setFanCharacteristics(SpanCharacteristic *fanSpeed, SpanCharacteristic *swingMode);
  void setThermostatCharacteristics(SpanCharacteristic *targetState, SpanCharacteristic *targetTemp);
  IRController(uint16_t sendPin, uint16_t recvPin, uint16_t captureBufferSize, uint8_t timeout, bool debug);

private:
  IRsend irsend;
  IRrecv irrecv;
  IRac acController;
  void saveLastState();
  void loadLastState();
  Preferences preferences;
  stdAc::state_t lastState;
  void updateHomeKitFromIR();
  bool lastStateValid = false;
  int dryingDelayMinutes = 40;
  bool dryingInProgress = false;
  SpanCharacteristic *fanSpeed;
  SpanCharacteristic *swingMode;
  SpanCharacteristic *targetTemp;
  SpanCharacteristic *targetState;
  bool dryingBeforeShutdownEnabled = true;
  void sendCommand(stdAc::state_t newState);
};

#endif  // IRCONTROLLER_H_