#pragma once

#include <Arduino.h>
#include <Logging.h>

#ifdef IDLE_CHAPTER_STAGE_DIAGNOSTICS
namespace {
// Samples at scope boundaries; low-water is boot-wide, not a local allocation peak.
class ChapterStageProbe {
 public:
  explicit ChapterStageProbe(const char* label)
      : label(label),
        freeBefore(ESP.getFreeHeap()),
        maxBefore(ESP.getMaxAllocHeap()),
        lowBefore(ESP.getMinFreeHeap()),
        started(millis()) {}
  ~ChapterStageProbe() {
    const auto elapsed = millis() - started;
    const auto freeAfter = ESP.getFreeHeap();
    const auto maxAfter = ESP.getMaxAllocHeap();
    const auto lowAfter = ESP.getMinFreeHeap();
    LOG_INF("NCP", "%s ms=%lu free=%u->%u max=%u->%u bootMin=%u->%u", label, static_cast<unsigned long>(elapsed),
            freeBefore, freeAfter, maxBefore, maxAfter, lowBefore, lowAfter);
  }

 private:
  const char* label;
  uint32_t freeBefore, maxBefore, lowBefore, started;
};
}  // namespace
#endif
