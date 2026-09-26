#pragma once

#if defined(LIBRARY_PROFILE)
#include <Arduino.h>
#include <Logging.h>

// Boundary snapshots are not allocation peaks; bootMin is boot-wide.
class LibraryProfileScope {
 public:
  explicit LibraryProfileScope(const char* stage)
      : stage(stage), started(millis()), freeBefore(ESP.getFreeHeap()), maxBefore(ESP.getMaxAllocHeap()) {}
  ~LibraryProfileScope() {
    LOG_INF("LIBPERF", "stage=%s ms=%lu free=%lu->%lu max=%lu->%lu bootMin=%lu", stage, millis() - started, freeBefore,
            static_cast<unsigned long>(ESP.getFreeHeap()), maxBefore, static_cast<unsigned long>(ESP.getMaxAllocHeap()),
            static_cast<unsigned long>(ESP.getMinFreeHeap()));
  }

 private:
  const char* stage;
  unsigned long started;
  unsigned long freeBefore;
  unsigned long maxBefore;
};
#define LIBRARY_PROFILE_SCOPE(stage) LibraryProfileScope libraryProfileScope(stage)
#else
#define LIBRARY_PROFILE_SCOPE(stage) ((void)0)
#endif
