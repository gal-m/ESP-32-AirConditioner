#include "IRController.h"

IRController::IRController(uint16_t sendPin, uint16_t recvPin, uint16_t captureBufferSize, uint8_t timeout, bool debug)
  : irsend(sendPin), irrecv(recvPin, captureBufferSize, timeout, debug), acController(sendPin) {
}

void IRController::beginSend() {
  irsend.begin();
  loadLastState();
  loadDryingSettings();
  loadIdentifiedProtocols();
}

void IRController::beginReceive() {
  irrecv.enableIRIn();
}

void IRController::setThermostatCharacteristics(SpanCharacteristic *targetState, SpanCharacteristic *targetTemp) {
  this->targetState = targetState;
  this->targetTemp = targetTemp;
}

void IRController::setFanCharacteristics(SpanCharacteristic *fanSpeed, SpanCharacteristic *swingMode) {
  this->fanSpeed = fanSpeed;
  this->swingMode = swingMode;
}

void IRController::handleIR() {
  decode_results results;
  if (irrecv.decode(&results)) {
    stdAc::state_t currentState;
    String savedProtocol = getProtocol();

    // Detect the current protocol
    String detectedProtocol = typeToString(results.decode_type);
    Serial.println("Received signal from: " + detectedProtocol);


    // Ignore "UNKNOWN" protocols
    if (detectedProtocol != "UNKNOWN" && !detectedProtocol.isEmpty()) {
      if (std::find(identifiedProtocols.begin(), identifiedProtocols.end(), detectedProtocol) == identifiedProtocols.end()) {
        // Add detected protocol to identified protocols list if not already present
        identifiedProtocols.push_back(detectedProtocol);
        saveIdentifiedProtocols();  // Save protocols after adding a new one
      }

      if (savedProtocol.isEmpty()) {
        // If no protocol is saved, check if the detected protocol is valid and supported by IRac
        if (IRac::isProtocolSupported(results.decode_type)) {
          // Save the first valid and supported detected protocol
          saveProtocol(detectedProtocol.c_str());
          Serial.println("First valid and supported protocol detected and saved: " + detectedProtocol);
        } else {
          Serial.println("Detected protocol is not supported by IRac. Ignoring.");
        }
      } else {
        if (detectedProtocol == savedProtocol) {
          Serial.println("Using saved protocol: " + savedProtocol);
          if (IRAcUtils::decodeToState(&results, &currentState, &lastState)) {
            lastState = currentState;
            saveLastState();  // Save the updated lastState
            updateHomeKitFromIR();
          }
        } else {
          Serial.println("Detected protocol does not match the saved protocol. Ignoring.");
        }
      }
    } else {
      Serial.println("Ignored invalid or unknown protocol: " + detectedProtocol);
    }
    irrecv.resume();
  }
}

void IRController::sendThermostatCommand(bool power, int mode, int temp) {
  stdAc::state_t newState = lastState;
  newState.power = power;
  newState.degrees = temp;

  switch (mode) {
    // case 0:  // Auto
    //   newState.mode = static_cast<stdAc::opmode_t>(0);
    //   break;
    case 1:  // Heating
      newState.mode = stdAc::opmode_t::kHeat;
      break;
    case 2:  // Cooling
      newState.mode = stdAc::opmode_t::kCool;
      break;
    case 3:  // Fan
      newState.mode = stdAc::opmode_t::kFan;
      break;
    default:
      Serial.println("Invalid mode. Defaulting to Auto.");
      // newState.mode = static_cast<stdAc::opmode_t>(0);
      break;
  }
  sendCommand(newState);
}

void IRController::sendFanCommand(int fanSpeed, bool swing) {
  stdAc::state_t newState = lastState;
  int mappedFanSpeed = (fanSpeed == 0) ? 0 : (fanSpeed <= 33) ? 1
                                           : (fanSpeed <= 66) ? 3
                                                              : 5;
  newState.fanspeed = static_cast<stdAc::fanspeed_t>(mappedFanSpeed);

  stdAc::swingv_t swingv = stdAc::swingv_t::kAuto;
  stdAc::swingh_t swingh = stdAc::swingh_t::kAuto;

  if (swing) {
    swingv = stdAc::swingv_t::kOff;
    swingh = stdAc::swingh_t::kOff;
  }

  newState.swingv = swingv;
  newState.swingh = swingh;
  sendCommand(newState);
}

void IRController::sendCommand(stdAc::state_t newState) {
  String savedProtocol = getProtocol();

  // Check if a protocol is saved before sending a command
  if (savedProtocol.isEmpty()) {
    Serial.println("No protocol saved. Cannot send command.");
    return;  // Exit the function if no protocol is saved
  }

  irrecv.pause();
  delay(10);

  // Check if lastState is valid, if not, load the saved lastState
  if (!lastStateValid) {
    loadLastState();
  }
  if (acController.sendAc(newState, &lastState)) {
    lastState = newState;
    saveLastState();  // Save the updated lastState
  } else {
    Serial.println("Failed to send AC command.");
  }
  delay(10);
  irrecv.resume();
}


void IRController::updateHomeKitFromIR() {
  if (targetTemp->getVal() != lastState.degrees) {
    targetTemp->setVal(lastState.degrees);
  }

  if (lastState.power || (targetState->getVal() != static_cast<int>(lastState.mode))) {
    switch (lastState.mode) {
      case stdAc::opmode_t::kAuto:  // Auto Mode
        targetState->setVal(3);
        break;
      case stdAc::opmode_t::kHeat:  // Heating Mode
        targetState->setVal(1);
        break;
      case stdAc::opmode_t::kCool:  // Cooling Mode
        targetState->setVal(2);
        break;
        // case stdAc::opmode_t::kFan:  // Fan Mode
        //   targetState->setVal(3);
        //   break;

      default:
        Serial.println("HomeKit Unrecognized mode received.");
        break;
    }

    int fanNewSpeed;
    switch (static_cast<int>(lastState.fanspeed)) {
      case 0:  // Auto
        fanNewSpeed = 0;
        break;
      case 1:  // Low
        fanNewSpeed = 25;
        break;
      case 3:  // Medium
        fanNewSpeed = 50;
        break;
      case 5:  // High
        fanNewSpeed = 100;
        break;
      default:
        fanNewSpeed = 0;
        break;
    }
    if (fanSpeed->getVal() != fanNewSpeed) {
      fanSpeed->setVal(fanNewSpeed);
    }

    int newSwingMode = 1;
    if (lastState.swingv != stdAc::swingv_t::kOff || lastState.swingh != stdAc::swingh_t::kOff) {
      newSwingMode = 0;
    }

    if (swingMode->getVal() != newSwingMode) {
      swingMode->setVal(newSwingMode);
    }

  } else {
    targetState->setVal(0);
  }
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

void IRController::setProtocol(const String &protocol) {
  saveProtocol(protocol.c_str());
}

void IRController::deleteIdentifiedProtocols() {
  identifiedProtocols.clear();
  preferences.begin("IRController", false);
  preferences.remove("identifiedProtocols");
  preferences.remove("protocol");
  preferences.remove("lastState");
  preferences.end();
}

void IRController::saveIdentifiedProtocols() {
  preferences.begin("IRController", false);

  // Join the identifiedProtocols vector into a single string separated by commas
  String protocolsString;
  for (size_t i = 0; i < identifiedProtocols.size(); i++) {
    protocolsString += identifiedProtocols[i];
    if (i < identifiedProtocols.size() - 1) {
      protocolsString += ",";  // Add comma delimiter between protocols
    }
  }

  // Save the serialized string to preferences
  preferences.putString("identifiedProtocols", protocolsString);
  preferences.end();
}

void IRController::loadIdentifiedProtocols() {
  preferences.begin("IRController", true);  // Open preferences in read-only mode

  // Get the serialized protocols string
  String protocolsString = preferences.getString("identifiedProtocols", "");
  preferences.end();

  identifiedProtocols.clear();  // Clear any existing protocols

  // Split the string into individual protocols based on commas
  if (protocolsString.length() > 0) {
    int start = 0;
    int end = protocolsString.indexOf(',');

    while (end != -1) {
      identifiedProtocols.push_back(protocolsString.substring(start, end));
      start = end + 1;
      end = protocolsString.indexOf(',', start);
    }

    // Add the last protocol after the last comma
    identifiedProtocols.push_back(protocolsString.substring(start));
  }
}

void IRController::saveLastState() {
  preferences.begin("IRController", false);
  preferences.putBytes("lastState", &lastState, sizeof(lastState));
  preferences.end();
}

void IRController::loadLastState() {
  preferences.begin("IRController", true);
  size_t size = preferences.getBytes("lastState", &lastState, sizeof(lastState));
  if (size == sizeof(lastState)) {
    lastStateValid = true;
  } else {
    lastStateValid = false;
  }
  preferences.end();
}

void IRController::saveDryingSettings() {
  preferences.begin("IRController", false);
  preferences.putBool("dryingEnabled", dryingBeforeShutdownEnabled);
  preferences.putInt("dryingDelay", dryingDelayMinutes);
  preferences.end();
  delay(1000);
  ESP.restart();
}

void IRController::loadDryingSettings() {
  preferences.begin("IRController", true);

  if (!preferences.isKey("dryingEnabled")) {
    dryingBeforeShutdownEnabled = true;  // Default to enabled
  } else {
    dryingBeforeShutdownEnabled = preferences.getBool("dryingEnabled", true);
  }

  if (!preferences.isKey("dryingDelay")) {
    dryingDelayMinutes = 40;  // Default delay of 40 minutes
  } else {
    dryingDelayMinutes = preferences.getInt("dryingDelay", 40);
  }

  preferences.end();
  // saveDryingSettings();
}



void IRController::enableDryingBeforeShutdown(bool enable, int delayMinutes) {
  dryingBeforeShutdownEnabled = enable;
  dryingDelayMinutes = delayMinutes;
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

void IRController::startDryingBeforeShutdown() {
  dryingInProgress = true;
  stdAc::state_t newState = lastState;
  targetState->setVal(0);
  fanSpeed->setVal(100);
  newState.mode = stdAc::opmode_t::kFan;
  newState.fanspeed = static_cast<stdAc::fanspeed_t>(5);
  sendCommand(newState);
}

void IRController::completeShutdown() {
  stdAc::state_t newState = lastState;
  newState.power = 0;
  sendCommand(newState);
}

