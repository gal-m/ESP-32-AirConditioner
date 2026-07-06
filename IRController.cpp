#include "IRController.h"

#include <algorithm>

namespace {
constexpr int kMinTargetTemperature = 16;
constexpr int kMaxTargetTemperature = 31;
constexpr int kDefaultTargetTemperature = 22;

int clampInt(int value, int minValue, int maxValue) {
  return std::max(minValue, std::min(value, maxValue));
}

float clampFloat(float value, float minValue, float maxValue) {
  return std::max(minValue, std::min(value, maxValue));
}

template <typename T>
void setCharacteristicIfChanged(SpanCharacteristic *characteristic, T value, bool notify = true) {
  if (characteristic && characteristic->getVal<T>() != value) {
    characteristic->setVal(value, notify);
  }
}

stdAc::fanspeed_t fanSpeedFromHomeKit(int fanSpeed) {
  if (fanSpeed <= 0) {
    return stdAc::fanspeed_t::kAuto;
  }
  if (fanSpeed <= 25) {
    return stdAc::fanspeed_t::kMin;
  }
  if (fanSpeed <= 50) {
    return stdAc::fanspeed_t::kMedium;
  }
  if (fanSpeed <= 75) {
    return stdAc::fanspeed_t::kHigh;
  }
  return stdAc::fanspeed_t::kMax;
}

int fanSpeedToHomeKit(stdAc::fanspeed_t fanSpeed) {
  switch (fanSpeed) {
    case stdAc::fanspeed_t::kAuto:
      return 0;
    case stdAc::fanspeed_t::kMin:
    case stdAc::fanspeed_t::kLow:
      return 25;
    case stdAc::fanspeed_t::kMedium:
      return 50;
    case stdAc::fanspeed_t::kHigh:
    case stdAc::fanspeed_t::kMediumHigh:
      return 75;
    case stdAc::fanspeed_t::kMax:
      return 100;
    default:
      return 0;
  }
}

int targetStateToHomeKit(const stdAc::state_t &state) {
  if (!state.power || state.mode == stdAc::opmode_t::kOff) {
    return 0;
  }

  switch (state.mode) {
    case stdAc::opmode_t::kHeat:
      return 1;
    case stdAc::opmode_t::kCool:
      return 2;
    case stdAc::opmode_t::kAuto:
      return 3;
    default:
      return 3;
  }
}

int currentStateToHomeKit(const stdAc::state_t &state) {
  if (!state.power || state.mode == stdAc::opmode_t::kOff) {
    return 0;
  }

  switch (state.mode) {
    case stdAc::opmode_t::kHeat:
      return 1;
    case stdAc::opmode_t::kCool:
      return 2;
    default:
      return 0;
  }
}

bool isSwingEnabled(const stdAc::state_t &state) {
  return state.swingv != stdAc::swingv_t::kOff || state.swingh != stdAc::swingh_t::kOff;
}
}  // namespace

IRController::IRController(uint16_t sendPin, uint16_t recvPin, uint16_t captureBufferSize, uint8_t timeout, bool debug)
  : irsend(sendPin), irrecv(recvPin, captureBufferSize, timeout, debug), acController(sendPin) {
  IRac::initState(&lastState);
  IRac::initState(&pendingState);
}

void IRController::beginSend() {
  irsend.begin();
  loadDryingSettings();
  loadIdentifiedProtocols();
  loadLastState();
}

void IRController::beginReceive() {
  irrecv.enableIRIn();
}

void IRController::setThermostatCharacteristics(SpanCharacteristic *targetState, SpanCharacteristic *targetTemp,
                                                SpanCharacteristic *currentState) {
  this->targetState = targetState;
  this->targetTemp = targetTemp;
  this->currentState = currentState;
  updateHomeKitFromIR();
}

void IRController::setFanCharacteristics(SpanCharacteristic *active, SpanCharacteristic *fanSpeed,
                                         SpanCharacteristic *swingMode, SpanCharacteristic *currentFanState) {
  this->fanActive = active;
  this->fanSpeed = fanSpeed;
  this->swingMode = swingMode;
  this->currentFanState = currentFanState;
  updateHomeKitFromIR();
}

void IRController::handleIR() {
  decode_results results;

  if (!irrecv.decode(&results)) {
    return;
  }

  String savedProtocol = getProtocol();
  String detectedProtocol = typeToString(results.decode_type);
  Serial.println("Received signal from: " + detectedProtocol);

  if (detectedProtocol == "UNKNOWN" || detectedProtocol.isEmpty()) {
    Serial.println("Ignored invalid or unknown protocol: " + detectedProtocol);
    irrecv.resume();
    return;
  }

  if (!IRac::isProtocolSupported(results.decode_type)) {
    Serial.println("Detected protocol is not supported by IRac. Ignoring.");
    irrecv.resume();
    return;
  }

  if (std::find(identifiedProtocols.begin(), identifiedProtocols.end(), detectedProtocol) == identifiedProtocols.end()) {
    identifiedProtocols.push_back(detectedProtocol);
    saveIdentifiedProtocols();
  }

  if (!savedProtocol.isEmpty() && detectedProtocol != savedProtocol) {
    Serial.println("Detected protocol does not match the saved protocol. Ignoring.");
    irrecv.resume();
    return;
  }

  stdAc::state_t decodedState;
  const stdAc::state_t *previousState = lastStateValid ? &lastState : nullptr;
  if (IRAcUtils::decodeToState(&results, &decodedState, previousState)) {
    lastState = decodedState;
    lastStateValid = true;
    saveLastState();

    if (savedProtocol.isEmpty()) {
      saveProtocol(detectedProtocol.c_str());
      Serial.println("First valid and supported protocol detected and saved: " + detectedProtocol);
    }

    updateHomeKitFromIR();
  } else {
    Serial.println("Detected protocol could not be converted to a common AC state.");
  }

  irrecv.resume();
}

bool IRController::sendThermostatCommand(bool power, int mode, int temp) {
  stdAc::state_t newState;
  if (!prepareStateForCommand(&newState)) {
    return false;
  }

  newState.power = power;
  newState.degrees = clampInt(temp, kMinTargetTemperature, kMaxTargetTemperature);

  if (!power || mode == 0) {
    newState.power = false;
    newState.mode = stdAc::opmode_t::kOff;
  } else {
    switch (mode) {
      case 1:
        newState.mode = stdAc::opmode_t::kHeat;
        break;
      case 2:
        newState.mode = stdAc::opmode_t::kCool;
        break;
      case 3:
        newState.mode = stdAc::opmode_t::kAuto;
        break;
      default:
        Serial.println("Invalid HomeKit thermostat mode.");
        return false;
    }
  }

  return queueCommand(newState);
}

bool IRController::sendFanCommand(int fanSpeed, bool swing, bool active) {
  stdAc::state_t newState;
  if (!prepareStateForCommand(&newState)) {
    return false;
  }

  if (!active) {
    newState.power = false;
    newState.mode = stdAc::opmode_t::kOff;
  } else {
    newState.power = true;
    if (newState.mode == stdAc::opmode_t::kOff) {
      newState.mode = stdAc::opmode_t::kFan;
    }
  }

  newState.fanspeed = fanSpeedFromHomeKit(fanSpeed);
  newState.swingv = swing ? stdAc::swingv_t::kAuto : stdAc::swingv_t::kOff;
  newState.swingh = swing ? stdAc::swingh_t::kAuto : stdAc::swingh_t::kOff;

  return queueCommand(newState);
}

bool IRController::queueCommand(const stdAc::state_t &newState) {
  std::lock_guard<std::mutex> lock(commandMutex);
  pendingState = newState;
  pendingCommand = true;
  return true;
}

void IRController::processPendingCommand() {
  stdAc::state_t commandState;

  {
    std::lock_guard<std::mutex> lock(commandMutex);
    if (!pendingCommand) {
      return;
    }

    commandState = pendingState;
    pendingCommand = false;
  }

  executeCommand(commandState);
}

bool IRController::executeCommand(const stdAc::state_t &newState) {
  if (newState.protocol == decode_type_t::UNKNOWN || !IRac::isProtocolSupported(newState.protocol)) {
    Serial.println("No supported AC protocol/state saved. Cannot send command.");
    return false;
  }

  stdAc::state_t previousState = lastState;
  const stdAc::state_t *previousStatePtr = lastStateValid ? &previousState : nullptr;

  irrecv.pause();
  delay(5);
  bool success = acController.sendAc(newState, previousStatePtr);
  delay(5);
  irrecv.resume();

  if (!success) {
    Serial.println("Failed to send AC command.");
    return false;
  }

  lastState = newState;
  lastStateValid = true;
  saveLastState();
  updateHomeKitFromIR();
  return true;
}

void IRController::updateHomeKitFromIR() {
  if (!lastStateValid) {
    return;
  }

  float safeTargetTemp = clampFloat(lastState.degrees, kMinTargetTemperature, kMaxTargetTemperature);
  setCharacteristicIfChanged<float>(targetTemp, safeTargetTemp);
  setCharacteristicIfChanged<int>(targetState, targetStateToHomeKit(lastState));
  setCharacteristicIfChanged<int>(currentState, currentStateToHomeKit(lastState));
  setCharacteristicIfChanged<int>(fanActive, lastState.power ? 1 : 0);
  setCharacteristicIfChanged<int>(currentFanState, lastState.power ? 2 : 0);
  setCharacteristicIfChanged<int>(fanSpeed, fanSpeedToHomeKit(lastState.fanspeed));
  setCharacteristicIfChanged<int>(swingMode, isSwingEnabled(lastState) ? 1 : 0);
}

void IRController::saveProtocol(const char *protocol) {
  preferences.begin("IRController", false);
  preferences.putString("protocol", protocol);
  preferences.end();
}

String IRController::getProtocol() {
  preferences.begin("IRController", true);
  String protocol = preferences.getString("protocol", "");
  preferences.end();
  return protocol;
}

std::vector<String> IRController::getIdentifiedProtocols() {
  return identifiedProtocols;
}

bool IRController::setProtocol(const String &protocol) {
  decode_type_t decodeType = strToDecodeType(protocol.c_str());

  if (protocol.isEmpty() || decodeType == decode_type_t::UNKNOWN || !IRac::isProtocolSupported(decodeType)) {
    Serial.println("Selected protocol is not supported by IRac.");
    return false;
  }

  saveProtocol(protocol.c_str());

  if (!lastStateValid || lastState.protocol != decodeType) {
    IRac::initState(&lastState);
    lastState.protocol = decodeType;
    lastState.degrees = targetTemp ? targetTemp->getVal<float>() : kDefaultTargetTemperature;
    lastState.degrees = clampFloat(lastState.degrees, kMinTargetTemperature, kMaxTargetTemperature);
    lastStateValid = true;
    saveLastState();
    updateHomeKitFromIR();
  }

  return true;
}

void IRController::deleteIdentifiedProtocols() {
  identifiedProtocols.clear();
  {
    std::lock_guard<std::mutex> lock(commandMutex);
    pendingCommand = false;
  }

  preferences.begin("IRController", false);
  preferences.remove("identifiedProtocols");
  preferences.remove("protocol");
  preferences.remove("lastState");
  preferences.end();

  IRac::initState(&lastState);
  lastStateValid = false;
}

void IRController::saveIdentifiedProtocols() {
  preferences.begin("IRController", false);

  String protocolsString;
  for (size_t i = 0; i < identifiedProtocols.size(); i++) {
    protocolsString += identifiedProtocols[i];
    if (i < identifiedProtocols.size() - 1) {
      protocolsString += ",";
    }
  }

  preferences.putString("identifiedProtocols", protocolsString);
  preferences.end();
}

void IRController::loadIdentifiedProtocols() {
  preferences.begin("IRController", true);
  String protocolsString = preferences.getString("identifiedProtocols", "");
  preferences.end();

  identifiedProtocols.clear();

  if (protocolsString.length() > 0) {
    int start = 0;
    int end = protocolsString.indexOf(',');

    while (end != -1) {
      identifiedProtocols.push_back(protocolsString.substring(start, end));
      start = end + 1;
      end = protocolsString.indexOf(',', start);
    }

    identifiedProtocols.push_back(protocolsString.substring(start));
  }
}

void IRController::saveLastState() {
  preferences.begin("IRController", false);
  preferences.putBytes("lastState", &lastState, sizeof(lastState));
  preferences.end();
  lastStateValid = true;
}

void IRController::loadLastState() {
  preferences.begin("IRController", true);
  size_t size = preferences.getBytes("lastState", &lastState, sizeof(lastState));
  preferences.end();

  if (size == sizeof(lastState) &&
      lastState.protocol != decode_type_t::UNKNOWN &&
      IRac::isProtocolSupported(lastState.protocol)) {
    lastStateValid = true;
    return;
  }

  IRac::initState(&lastState);
  lastStateValid = false;
  initializeStateFromProtocol(getProtocol());
}

bool IRController::initializeStateFromProtocol(const String &protocol) {
  decode_type_t decodeType = strToDecodeType(protocol.c_str());

  if (protocol.isEmpty() || decodeType == decode_type_t::UNKNOWN || !IRac::isProtocolSupported(decodeType)) {
    return false;
  }

  IRac::initState(&lastState);
  lastState.protocol = decodeType;
  lastState.degrees = targetTemp ? targetTemp->getVal<float>() : kDefaultTargetTemperature;
  lastState.degrees = clampFloat(lastState.degrees, kMinTargetTemperature, kMaxTargetTemperature);
  lastStateValid = true;
  return true;
}

bool IRController::prepareStateForCommand(stdAc::state_t *state) {
  if (!state) {
    return false;
  }

  if (!lastStateValid && !initializeStateFromProtocol(getProtocol())) {
    Serial.println("No saved AC state/protocol. Capture a supported remote command first.");
    return false;
  }

  if (lastState.protocol == decode_type_t::UNKNOWN || !IRac::isProtocolSupported(lastState.protocol)) {
    Serial.println("Saved AC protocol is not supported.");
    return false;
  }

  *state = lastState;
  return true;
}

void IRController::saveDryingSettings() {
  preferences.begin("IRController", false);
  preferences.putBool("dryingEnabled", dryingBeforeShutdownEnabled);
  preferences.putInt("dryingDelay", dryingDelayMinutes);
  preferences.end();
}

void IRController::loadDryingSettings() {
  preferences.begin("IRController", true);
  dryingBeforeShutdownEnabled = preferences.getBool("dryingEnabled", true);
  dryingDelayMinutes = preferences.getInt("dryingDelay", 40);
  preferences.end();

  dryingDelayMinutes = clampInt(dryingDelayMinutes, 1, 60);
}

void IRController::enableDryingBeforeShutdown(bool enable, int delayMinutes) {
  dryingBeforeShutdownEnabled = enable;
  dryingDelayMinutes = clampInt(delayMinutes, 1, 60);
  saveDryingSettings();
}

bool IRController::isDryingBeforeShutdownEnabled() const {
  return dryingBeforeShutdownEnabled;
}

int IRController::getDryingDelayMinutes() const {
  return dryingDelayMinutes;
}

int IRController::getDryingDelayInSeconds() const {
  return dryingDelayMinutes * 60;
}

bool IRController::startDryingBeforeShutdown() {
  stdAc::state_t newState;
  if (!prepareStateForCommand(&newState)) {
    return false;
  }

  dryingInProgress = true;
  setCharacteristicIfChanged<int>(targetState, 0);
  setCharacteristicIfChanged<int>(fanActive, 1);
  setCharacteristicIfChanged<int>(currentFanState, 2);
  setCharacteristicIfChanged<int>(fanSpeed, 100);

  newState.power = true;
  newState.mode = stdAc::opmode_t::kFan;
  newState.fanspeed = stdAc::fanspeed_t::kMax;
  return queueCommand(newState);
}

bool IRController::completeShutdown() {
  stdAc::state_t newState;
  if (!prepareStateForCommand(&newState)) {
    return false;
  }

  dryingInProgress = false;
  newState.power = false;
  newState.mode = stdAc::opmode_t::kOff;
  return queueCommand(newState);
}
