#pragma once
#include <cstddef>
#include <initializer_list>

// Conservative file/cache/allocator allowance, in addition to caller headroom.
// This is admission accounting, not a heap reservation or a peak-use guarantee.
namespace pngbudget {
constexpr size_t OVERHEAD_BYTES = 4 * 1024;
inline bool admits(size_t freeBytes, size_t largestBlock, size_t decoder, size_t gray, size_t band,
                   size_t reserve) {
  if (largestBlock < decoder || largestBlock < gray || largestBlock < band) return false;
  for (size_t bytes : {decoder, gray, band, OVERHEAD_BYTES, reserve}) {
    if (freeBytes < bytes) return false;
    freeBytes -= bytes;
  }
  return true;
}
}  // namespace pngbudget
