#pragma once

#if defined(SINGLE_CACHE_PROFILE)
#include <Arduino.h>
#include <Logging.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace SingleCacheProfile {
enum Kind { AdvanceFull, AdvanceOther, VerticalLayout, PageWrite, Count };
inline uint64_t elapsed[Count]{};
inline uint32_t calls[Count]{};
inline TaskHandle_t owner = nullptr;

struct SectionScope {
  int spine;
  explicit SectionScope(int value) : spine(value) {
    for (int i = 0; i < Count; ++i) { elapsed[i] = 0; calls[i] = 0; }
    owner = xTaskGetCurrentTaskHandle();
  }
  ~SectionScope() {
    owner = nullptr;
    LOG_INF("SCP", "section=%d advance_full_us=%llu calls=%u advance_other_us=%llu calls=%u "
                   "vertical_us=%llu calls=%u page_write_us=%llu calls=%u",
            spine, static_cast<unsigned long long>(elapsed[AdvanceFull]), calls[AdvanceFull],
            static_cast<unsigned long long>(elapsed[AdvanceOther]), calls[AdvanceOther],
            static_cast<unsigned long long>(elapsed[VerticalLayout]), calls[VerticalLayout],
            static_cast<unsigned long long>(elapsed[PageWrite]), calls[PageWrite]);
  }
};

struct Timer {
  Kind kind;
  bool active;
  uint32_t start;
  explicit Timer(Kind value) : kind(value), active(owner && owner == xTaskGetCurrentTaskHandle()),
                               start(active ? micros() : 0) {}
  ~Timer() {
    if (active) { elapsed[kind] += static_cast<uint32_t>(micros() - start); ++calls[kind]; }
  }
};
}  // namespace SingleCacheProfile
#endif
