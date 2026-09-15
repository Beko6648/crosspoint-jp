#pragma once

#include <Arduino.h>
#include <Logging.h>

#include <cstddef>
#include <limits>
#include <new>

// Admission control for the exception-disabled STL used by layout. A failed
// probe is recoverable; reserve()/assign() themselves are NOT fallible APIs.
// This is deliberately conservative, including allocator overhead and room for
// the caller. It cannot protect against unrelated concurrent heap exhaustion.
namespace LayoutMemory {
constexpr size_t HEADROOM = 4096;

inline size_t add(size_t a, size_t b) {
  return b > std::numeric_limits<size_t>::max() - a ? std::numeric_limits<size_t>::max() : a + b;
}
inline size_t multiply(size_t count, size_t bytes) {
  return bytes && count > std::numeric_limits<size_t>::max() / bytes ? std::numeric_limits<size_t>::max()
                                                                     : count * bytes;
}
inline bool admit(size_t bytes, const char* stage) {
  const size_t request = add(bytes, HEADROOM);
  const size_t free = ESP.getFreeHeap();
  const size_t largest = ESP.getMaxAllocHeap();
  if (request <= free && request <= largest) {
    auto* probe = new (std::nothrow) unsigned char[request];
    if (probe) {
      delete[] probe;
      return true;
    }
  }
  LOG_ERR("PTX", "Layout stopped: %s need=%u free=%u largest=%u", stage, static_cast<unsigned>(request),
          static_cast<unsigned>(free), static_cast<unsigned>(largest));
  return false;
}
}  // namespace LayoutMemory
