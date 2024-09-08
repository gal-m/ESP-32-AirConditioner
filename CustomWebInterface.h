#ifndef CUSTOM_WEB_INTERFACE_H
#define CUSTOM_WEB_INTERFACE_H

#include <WebServer.h>
#include "IRController.h"

void webServerLoop();
void setupWebInterface(IRController &irController);
#endif
