#pragma once

#include <esp_task_wdt.h>

inline void resetTaskWatchdogIfSubscribed() {
  static const bool currentTaskIsSubscribed = esp_task_wdt_status(nullptr) == ESP_OK;
  if (currentTaskIsSubscribed) {
    esp_task_wdt_reset();
  }
}
