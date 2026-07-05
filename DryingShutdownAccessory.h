#ifndef DRYING_SHUTDOWN_ACCESSORY_H
#define DRYING_SHUTDOWN_ACCESSORY_H


#include "HomeSpan.h"
#include "IRController.h"


class DryingShutdownAccessory : public Service::Valve {
private:
  SpanCharacteristic *name;
  IRController *irController;
  SpanCharacteristic *enabled;
  SpanCharacteristic *valveType;
  SpanCharacteristic *setDuration;
  SpanCharacteristic *remainingDuration;

public:
  void loop();
  boolean update();
  SpanCharacteristic *inUse;
  SpanCharacteristic *active;
  DryingShutdownAccessory(IRController *irCtrl);
};

#endif
