#include <freertos/portmacro.h>
#include <cstdint>

#ifndef TASKS_H
#define TASKS_H

void measureTriosTaskCode(void* parameter);
void lightTimerTaskCode(void* parameter);
void mainTimingControlTaskCode(void* parameter);
void buttonTaskCode(void* parameter);
void simcomControllerTaskCode(void* parameter);
void serialReadTaskCode(void* parameter);
void batteryMeasureTaskCode(void* parameter);
void WiFiTaskCode(void* parameter);
void cameraTaskCode(void* parameter);

#endif // TASKS_H
