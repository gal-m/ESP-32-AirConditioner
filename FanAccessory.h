#ifndef FAN_ACCESSORY_H
#define FAN_ACCESSORY_H

#include "HomeSpan.h"
#include "IRController.h"

class FanAccessory : public Service::Fan {
private:
  SpanCharacteristic *active;
  IRController *irController;
  SpanCharacteristic *swingMode;
  SpanCharacteristic *currentFanState;
  SpanCharacteristic *fanRotationSpeed;
  SpanCharacteristic *rotationDirection;

public:
  FanAccessory(IRController *irCtrl);
  boolean update();
};

#endif