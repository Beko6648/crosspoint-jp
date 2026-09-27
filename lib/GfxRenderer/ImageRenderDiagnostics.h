#pragma once

// Opt-in only. Fixed storage avoids allocating diagnostic records on heap or
// adding a large frame to the render task stack. Samples are printed after
// rendering; existing application logs and other tasks still affect timing.
#if defined(IMAGE_RENDER_MEMORY_DIAGNOSTICS)
#include <Arduino.h>
#include <Logging.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace imagerenderdiag {
#if defined(IMAGE_RENDER_MEMORY_DIAGNOSTICS)
struct Sample {
  const char* stage;
  uint32_t atMs, freeBytes, maxAlloc, minFree, detail;
};
inline Sample samples[64];
inline unsigned count = 0, dropped = 0;
inline TaskHandle_t owner = nullptr;

inline void mark(const char* stage, uint32_t detail = 0) {
  if (!owner || owner != xTaskGetCurrentTaskHandle()) return;
  if (count == 64) {
    ++dropped;
    return;
  }
  samples[count++] = {stage, millis(), ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getMinFreeHeap(), detail};
}

class PageScope {
  bool enabled;
  int spine, page;

 public:
  PageScope(bool hasImages, int spineIndex, int pageIndex)
      : enabled(hasImages && !owner), spine(spineIndex), page(pageIndex) {
    if (!enabled) return;
    count = dropped = 0;
    owner = xTaskGetCurrentTaskHandle();
    mark("page-begin");
  }
  ~PageScope() {
    if (!enabled) return;
    mark("page-end");
    owner = nullptr;  // No sampling while serial output is being emitted.
    LOG_DBG("IRM", "spine=%d page=%d samples=%u dropped=%u storage=%u", spine, page, count, dropped,
            (unsigned)sizeof(samples));
    for (unsigned i = 0; i < count; ++i) {
      const auto& s = samples[i];
      LOG_DBG("IRM", "at=%u stage=%s free=%u maxAlloc=%u minFree=%u detail=%u", (unsigned)s.atMs, s.stage,
              (unsigned)s.freeBytes, (unsigned)s.maxAlloc, (unsigned)s.minFree, (unsigned)s.detail);
    }
  }
  PageScope(const PageScope&) = delete;
  PageScope& operator=(const PageScope&) = delete;
};
#else
inline void mark(const char*, unsigned = 0) {}
class PageScope {
 public:
  PageScope(bool, int, int) {}
};
#endif

// Declare before FsFile: destructor samples after the file has closed on every
// return path. Labels must be string literals because output is deferred.
class CacheScope {
 public:
  explicit CacheScope(unsigned mode) { mark("cache-enter", mode); }
  ~CacheScope() { mark("cache-exit"); }
};
}  // namespace imagerenderdiag
