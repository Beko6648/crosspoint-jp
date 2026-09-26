#pragma once
#include <cstddef>

namespace imagerenderbudget {
// Conservative headroom, not a reservation or a guarantee against fragmentation.
// Read buffering is <=4KiB for supported reader widths. Allocation overhead
// includes the rounded BW chunks, file state and small render temporaries.
constexpr size_t READ_BYTES = 4096;
constexpr size_t OVERHEAD_BYTES = 4096;
constexpr size_t RESERVE_BYTES = 32 * 1024;
constexpr size_t EXTRA_BYTES = READ_BYTES + OVERHEAD_BYTES + RESERVE_BYTES;
// BW chunks request at most 8000 bytes. Total free alone cannot establish fit.
constexpr size_t MIN_BLOCK_BYTES = 8192;
inline bool needsRecovery(size_t frameBytes, size_t freeBytes, size_t largestBlock) {
  return largestBlock < MIN_BLOCK_BYTES || freeBytes < frameBytes || freeBytes - frameBytes < EXTRA_BYTES;
}

// Keep the staged policy independently testable; no memory is allocated here.
// The caller provides snapshots and optional diagnostics after each release.
template <class Fonts, class ReadHeap, class Observe>
unsigned recover(size_t frameBytes, Fonts* fonts, ReadHeap readHeap, Observe observe) {
  auto heap = readHeap();
  if (!fonts || !needsRecovery(frameBytes, heap.first, heap.second)) return 0;
  fonts->clearCache();
  observe(1);
  heap = readHeap();
  if (!needsRecovery(frameBytes, heap.first, heap.second)) return 1;
  fonts->freeKernLigatureData();
  observe(2);
  return 2;
}
}  // namespace imagerenderbudget
