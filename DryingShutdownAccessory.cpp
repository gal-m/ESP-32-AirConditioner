
#include "DryingShutdownAccessory.h"



DryingShutdownAccessory::DryingShutdownAccessory(IRController *irCtrl)
  : irController(irCtrl) {

  inUse = new Characteristic::InUse(0);
  active = new Characteristic::Active(0);
  valveType = new Characteristic::ValveType(0);
  enabled = new Characteristic::IsConfigured(1);
  setDuration = new Characteristic::SetDuration(10);
  remainingDuration = new Characteristic::RemainingDuration(0);
  name = new Characteristic::ConfiguredName("Drying Shutdown");
}

boolean DryingShutdownAccessory::update() {

  if (active->updated()) {

    if (active->getNewVal()) {
      if (!irController->startDryingBeforeShutdown()) {
        return false;
      }

      inUse->setVal(1);
      setDuration->setVal(irController->getDryingDelayInSeconds());
      remainingDuration->setVal(setDuration->getVal());

    } else {
      if (!irController->completeShutdown()) {
        return false;
      }

      inUse->setVal(0);
      remainingDuration->setVal(0);
    }
  }
  return true;
}

void DryingShutdownAccessory::poll() {

  if (active->getVal()) {
    int remainingTime = setDuration->getVal() - active->timeVal() / 1000;

    if (remainingTime <= 0) {
      if (irController->completeShutdown()) {
        active->setVal(0);
        inUse->setVal(0);
        remainingDuration->setVal(0);
      }
    } else if (remainingTime < remainingDuration->getVal()) {
      remainingDuration->setVal(remainingTime, false);
    }
  }
}
