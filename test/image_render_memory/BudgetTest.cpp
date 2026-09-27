#include <cassert>
#include <limits>
#include <utility>

#include "ImageRenderBudget.h"
struct Fonts {
  unsigned glyphs = 0, tables = 0;
  size_t free = 0, largest = 0, gain = 0;
  void clearCache() {
    ++glyphs;
    free += gain;
  }
  void freeKernLigatureData() {
    ++tables;
    free += 20000;
    largest = 65524;
  }
};
int main() {
  using namespace imagerenderbudget;
  assert(needsRecovery(52272, 77924, 32756));
  assert(needsRecovery(48000, 85708, 47092));
  assert(!needsRecovery(52272, 118996, 94196));
  assert(!needsRecovery(48000, 124924, 102388));
  assert(!needsRecovery(52272, 52272 + EXTRA_BYTES, MIN_BLOCK_BYTES));
  assert(needsRecovery(52272, 52272 + EXTRA_BYTES - 1, MIN_BLOCK_BYTES));
  assert(needsRecovery(48000, 140000, MIN_BLOCK_BYTES - 1));
  assert(needsRecovery(std::numeric_limits<size_t>::max(), 100000, 65524));
  Fonts f;
  unsigned observed = 0;
  auto read = [&] { return std::make_pair(f.free, f.largest); };
  auto note = [&](unsigned stage) { observed = stage; };
  f.free = 120000;
  f.largest = 65524;
  assert(recover(52272, &f, read, note) == 0 && f.glyphs == 0 && f.tables == 0);
  f.free = 78000;
  f.gain = 20000;
  assert(recover(52272, &f, read, note) == 1 && f.glyphs == 1 && f.tables == 0 && observed == 1);
  f.free = 78000;
  f.gain = 0;
  assert(recover(52272, &f, read, note) == 2 && f.glyphs == 2 && f.tables == 1 && observed == 2);
  f.free = 140000;
  f.largest = 3000;
  assert(recover(52272, &f, read, note) == 2 && f.tables == 2);  // fragmentation also recovers
  f.free = 1000;
  assert(recover(52272, static_cast<Fonts*>(nullptr), read, note) == 0);
}
