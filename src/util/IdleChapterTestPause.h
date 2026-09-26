#pragma once
#include <cstdint>
// Diagnostic-only cooperative pause. Never sleeps or retains a render lock
// between loop calls. Arm only after a page has been written to the temp cache.
class IdleChapterTestPause {
  bool armed = false;
  uint32_t started = 0;

 public:
  void reset() { armed = false; }
  bool arm(uint32_t now, unsigned pages) {
    if (armed || pages == 0) return false;
    armed = true;
    started = now;
    return true;
  }
  bool waiting(uint32_t now, uint32_t duration) const {
    return armed && static_cast<uint32_t>(now - started) < duration;
  }
};
