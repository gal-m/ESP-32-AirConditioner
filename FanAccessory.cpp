#include "FanAccessory.h"

FanAccessory::FanAccessory(IRController *irCtrl)
  : irController(irCtrl) {

  active = new Characteristic::Active(0, true);
  swingMode = new Characteristic::SwingMode(0, true);
  fanRotationSpeed = new Characteristic::RotationSpeed(25, true);
  currentFanState = new Characteristic::CurrentFanState(0, true);
  rotationDirection = new Characteristic::RotationDirection(0, true);
  fanRotationSpeed->setRange(0, 100, 33);
  irController->setFanCharacteristics(fanRotationSpeed, rotationDirection);
}

boolean FanAccessory::update() {
  int fanSpeed = fanRotationSpeed->getNewVal();
  bool direction = rotationDirection->getNewVal();
  irController->sendFanCommand(fanSpeed, direction);
  return true;
}