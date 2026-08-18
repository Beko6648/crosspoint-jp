#pragma once

// Opt-in diagnostics for Issue #18. Keep all calls compiled out of normal
// firmware; enable only with -DISSUE18_DIAGNOSTICS=1 in the debug environment.
#if ISSUE18_DIAGNOSTICS

#include <Arduino.h>
#include <Logging.h>

namespace Issue18Diagnostics {

inline void logMemory(const char* event, const char* detail = "") {
  LOG_INF("I18D", "%s detail=%s free=%u maxAlloc=%u minFree=%u", event, detail, ESP.getFreeHeap(),
          ESP.getMaxAllocHeap(), ESP.getMinFreeHeap());
}

}  // namespace Issue18Diagnostics

#else

namespace Issue18Diagnostics {
inline void logMemory(const char*, const char* = "") {}
}  // namespace Issue18Diagnostics

#endif
