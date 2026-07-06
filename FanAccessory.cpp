#include "FanAccessory.h"

FanAccessory::FanAccessory(IRController *irCtrl)
  : irController(irCtrl) {

  active = new Characteristic::Active(0, true);
  swingMode = new Characteristic::SwingMode(0, true);
  fanRotationSpeed = new Characteristic::RotationSpeed(25, true);
  currentFanState = new Characteristic::CurrentFanState(0);
  fanRotationSpeed->setRange(0, 100, 25);
  irController->setFanCharacteristics(active, fanRotationSpeed, swingMode, currentFanState);
}

boolean FanAccessory::update() {
  bool isActive = active->updated() ? active->getNewVal() : active->getVal();
  int fanSpeed = fanRotationSpeed->updated() ? fanRotationSpeed->getNewVal() : fanRotationSpeed->getVal();
  bool swing = swingMode->updated() ? swingMode->getNewVal() : swingMode->getVal();

  if (isActive && fanSpeed == 0) {
    fanSpeed = 25;
    fanRotationSpeed->setVal(fanSpeed);
  }

  return irController->sendFanCommand(fanSpeed, swing, isActive);
}
