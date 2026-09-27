#pragma once
#include "FreeRTOS.h"
inline int firstTask, secondTask;
inline TaskHandle_t task = &firstTask;
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return task; }
