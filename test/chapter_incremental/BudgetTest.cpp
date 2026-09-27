#include <cassert>
#include <utility>

#include "Epub/IdleChapterBudget.h"
struct Fonts {
  int clears = 0;
  void clearCache() { ++clears; }
};
int main() {
  Fonts f;
  size_t free = 100 * 1024, block = 16 * 1024;
  auto read = [&] { return std::make_pair(free, block); };
  assert(!idlechapterbudget::recover(96 * 1024, 16 * 1024, &f, read));
  assert(f.clears == 0);
  free = 79 * 1024;
  assert(idlechapterbudget::recover(96 * 1024, 16 * 1024, &f, read));
  assert(f.clears == 1);
  // No loop/retry when clearing does not recover enough memory.
  assert(free == 79 * 1024);
  free = 110 * 1024;
  block = 8 * 1024;
  assert(idlechapterbudget::recover(96 * 1024, 16 * 1024, &f, read));
  assert(f.clears == 2);
  block = 16 * 1024;
  assert(!idlechapterbudget::recover(96 * 1024, 16 * 1024, &f, read));
  assert(!idlechapterbudget::recover(96 * 1024, 16 * 1024, static_cast<Fonts*>(nullptr), read));
}
