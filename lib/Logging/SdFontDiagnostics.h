#pragma once

// Opt-in SD-font memory diagnostics for constrained-device investigations.
// Enable only with -DSD_FONT_DIAGNOSTICS=1 in a development build.
#if SD_FONT_DIAGNOSTICS

#include <Arduino.h>
#include <Logging.h>
#include <esp_heap_caps.h>

#include <cstdio>

namespace SdFontDiagnostics {

struct Context {
  char family[48] = "";
  uint8_t pointSize = 0;
  int spine = -1;
  int page = -1;
};

inline Context context;

struct HeapSnapshot {
  uint32_t free = 0;
  uint32_t largest8 = 0;
  uint32_t minimum8 = 0;
};

struct FontLoadRecord {
  char family[48] = "";
  uint8_t pointSize = 0;
  HeapSnapshot before{};
  HeapSnapshot after{};
  uint32_t elapsedUs = 0;
  bool valid = false;
  bool succeeded = false;
  bool replayed = false;
};

inline FontLoadRecord fontLoadRecord;

inline void setContext(const char* family, const uint8_t pointSize, const int spine, const int page) {
  snprintf(context.family, sizeof(context.family), "%s", family ? family : "");
  context.pointSize = pointSize;
  context.spine = spine;
  context.page = page;
}

inline HeapSnapshot captureHeap() {
  HeapSnapshot snapshot;
  // A render task can allocate between the individual heap API calls. Sample
  // free space on both sides and accept only a self-consistent sequence.
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    const uint32_t freeBefore = ESP.getFreeHeap();
    snapshot.largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    snapshot.minimum8 = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    const uint32_t freeAfter = ESP.getFreeHeap();
    if (snapshot.largest8 <= freeBefore && snapshot.largest8 <= freeAfter) {
      snapshot.free = freeAfter;
      return snapshot;
    }
  }

  // If allocations keep changing during all retries, obtain the three values
  // through one heap-info query so the diagnostic row remains comparable.
  multi_heap_info_t info{};
  heap_caps_get_info(&info, MALLOC_CAP_8BIT);
  snapshot.free = info.total_free_bytes;
  snapshot.largest8 = info.largest_free_block;
  snapshot.minimum8 = info.minimum_free_bytes;
  return snapshot;
}

inline uint32_t nowUs() { return micros(); }

inline void logSnapshot(const char* event, const char* family, const uint8_t pointSize, const HeapSnapshot& heap,
                        const uint32_t elapsedUs, const bool includeElapsed, const bool replayed) {
  if (includeElapsed) {
    LOG_INF("SFD", "event=%s family=%s pt=%u style=0x00 spine=-1 page=-1 free=%u largest8=%u min8=%u elapsed_us=%u%s",
            event, family, pointSize, heap.free, heap.largest8, heap.minimum8, elapsedUs,
            replayed ? " replayed=1" : "");
    return;
  }
  LOG_INF("SFD", "event=%s family=%s pt=%u style=0x00 spine=-1 page=-1 free=%u largest8=%u min8=%u%s", event, family,
          pointSize, heap.free, heap.largest8, heap.minimum8, replayed ? " replayed=1" : "");
}

inline void rememberFontLoadBefore() {
  snprintf(fontLoadRecord.family, sizeof(fontLoadRecord.family), "%s", context.family);
  fontLoadRecord.pointSize = context.pointSize;
  fontLoadRecord.before = captureHeap();
  fontLoadRecord.valid = true;
  fontLoadRecord.succeeded = false;
  fontLoadRecord.replayed = false;
  logSnapshot("font_load_before", fontLoadRecord.family, fontLoadRecord.pointSize, fontLoadRecord.before, 0, false,
              false);
}

inline void rememberFontLoadAfter(const bool succeeded, const uint32_t elapsedUs) {
  fontLoadRecord.after = captureHeap();
  fontLoadRecord.elapsedUs = elapsedUs;
  fontLoadRecord.succeeded = succeeded;
  logSnapshot(succeeded ? "font_load_after" : "font_load_failed", fontLoadRecord.family, fontLoadRecord.pointSize,
              fontLoadRecord.after, elapsedUs, true, false);
}

inline void replayFontLoad() {
  if (!fontLoadRecord.valid || fontLoadRecord.replayed) return;
  logSnapshot("font_load_before", fontLoadRecord.family, fontLoadRecord.pointSize, fontLoadRecord.before, 0, false,
              true);
  logSnapshot(fontLoadRecord.succeeded ? "font_load_after" : "font_load_failed", fontLoadRecord.family,
              fontLoadRecord.pointSize, fontLoadRecord.after, fontLoadRecord.elapsedUs, true, true);
  fontLoadRecord.replayed = true;
}

inline void logMemory(const char* event, const uint8_t styleMask = 0, const uint32_t elapsedUs = 0,
                      const bool includeElapsed = false) {
  const HeapSnapshot heap = captureHeap();
  if (includeElapsed) {
    LOG_INF("SFD", "event=%s family=%s pt=%u style=0x%02X spine=%d page=%d free=%u largest8=%u min8=%u elapsed_us=%u",
            event, context.family, context.pointSize, styleMask, context.spine, context.page, heap.free, heap.largest8,
            heap.minimum8, elapsedUs);
    return;
  }
  LOG_INF("SFD", "event=%s family=%s pt=%u style=0x%02X spine=%d page=%d free=%u largest8=%u min8=%u", event,
          context.family, context.pointSize, styleMask, context.spine, context.page, heap.free, heap.largest8,
          heap.minimum8);
}

}  // namespace SdFontDiagnostics

#define SD_FONT_DIAG_CONTEXT(family, pointSize, spine, page) \
  SdFontDiagnostics::setContext((family), (pointSize), (spine), (page))
#define SD_FONT_DIAG_LOG(event, styleMask) SdFontDiagnostics::logMemory((event), (styleMask))
#define SD_FONT_DIAG_NOW_US() SdFontDiagnostics::nowUs()
#define SD_FONT_DIAG_LOG_AFTER(event, styleMask, startedAt) \
  SdFontDiagnostics::logMemory((event), (styleMask), SdFontDiagnostics::nowUs() - (startedAt), true)
#define SD_FONT_DIAG_FONT_LOAD_BEFORE() SdFontDiagnostics::rememberFontLoadBefore()
#define SD_FONT_DIAG_FONT_LOAD_AFTER(succeeded, startedAt) \
  SdFontDiagnostics::rememberFontLoadAfter((succeeded), SdFontDiagnostics::nowUs() - (startedAt))
#define SD_FONT_DIAG_REPLAY_FONT_LOAD() SdFontDiagnostics::replayFontLoad()

#else

#define SD_FONT_DIAG_CONTEXT(...) ((void)0)
#define SD_FONT_DIAG_LOG(...) ((void)0)
#define SD_FONT_DIAG_NOW_US() 0U
#define SD_FONT_DIAG_LOG_AFTER(...) ((void)0)
#define SD_FONT_DIAG_FONT_LOAD_BEFORE(...) ((void)0)
#define SD_FONT_DIAG_FONT_LOAD_AFTER(...) ((void)0)
#define SD_FONT_DIAG_REPLAY_FONT_LOAD(...) ((void)0)

#endif
