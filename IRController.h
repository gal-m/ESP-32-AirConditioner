#ifndef IRCONTROLLER_H_
#define IRCONTROLLER_H_


#include <mutex>
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
  void processPendingCommand();
  String getProtocol();
  bool completeShutdown();
  void saveDryingSettings();
  void loadDryingSettings();
  void saveIdentifiedProtocols();
  void loadIdentifiedProtocols();
  void deleteIdentifiedProtocols();
  bool startDryingBeforeShutdown();
  int getDryingDelayMinutes() const;
  int getDryingDelayInSeconds() const;
  std::vector<String> identifiedProtocols;
  void saveProtocol(const char *protocol);
  bool setProtocol(const String &protocol);
  bool isDryingBeforeShutdownEnabled() const;
  std::vector<String> getIdentifiedProtocols();
  bool sendFanCommand(int fanSpeed, bool swing, bool active);
  bool sendThermostatCommand(bool power, int mode, int temp);
  void enableDryingBeforeShutdown(bool enable, int delayMinutes);
  void setFanCharacteristics(SpanCharacteristic *active, SpanCharacteristic *fanSpeed,
                             SpanCharacteristic *swingMode, SpanCharacteristic *currentFanState);
  void setThermostatCharacteristics(SpanCharacteristic *targetState, SpanCharacteristic *targetTemp,
                                    SpanCharacteristic *currentState);
  IRController(uint16_t sendPin, uint16_t recvPin, uint16_t captureBufferSize, uint8_t timeout, bool debug);

private:
  IRsend irsend;
  IRrecv irrecv;
  IRac acController;
  void saveLastState();
  void loadLastState();
  bool initializeStateFromProtocol(const String &protocol);
  bool prepareStateForCommand(stdAc::state_t *state);
  bool queueCommand(const stdAc::state_t &newState);
  bool executeCommand(const stdAc::state_t &newState);
  Preferences preferences;
  stdAc::state_t lastState;
  stdAc::state_t pendingState;
  void updateHomeKitFromIR();
  bool lastStateValid = false;
  bool pendingCommand = false;
  int dryingDelayMinutes = 40;
  bool dryingInProgress = false;
  std::mutex commandMutex;
  SpanCharacteristic *fanActive = nullptr;
  SpanCharacteristic *fanSpeed = nullptr;
  SpanCharacteristic *swingMode = nullptr;
  SpanCharacteristic *targetTemp = nullptr;
  SpanCharacteristic *targetState = nullptr;
  SpanCharacteristic *currentState = nullptr;
  SpanCharacteristic *currentFanState = nullptr;
  bool dryingBeforeShutdownEnabled = true;
};

#endif  // IRCONTROLLER_H_
