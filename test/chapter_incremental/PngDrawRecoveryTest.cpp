#include <cassert>
#include <utility>

#include "Epub/converters/PngDrawRecovery.h"
struct Fonts {
  int calls = 0;
  void releaseSdFontVerticalGlyphs() { ++calls; }
};
int main() {
  Fonts f;
  auto healthy = [] { return std::make_pair(120000u, 65524u); };
  auto fragmented = [] { return std::make_pair(119180u, 57332u); };
  auto totalLow = [] { return std::make_pair(60000u, 59000u); };
  assert(!pngdrawrecovery::recover(true, 61440, 58464, &f, healthy));
  assert(!pngdrawrecovery::recover(false, 61440, 58464, &f, fragmented));
  assert(!pngdrawrecovery::recover(true, 61440, 58464, static_cast<Fonts*>(nullptr), fragmented));
  assert(f.calls == 0);
  assert(pngdrawrecovery::recover(true, 61440, 58464, &f, fragmented));
  assert(f.calls == 1);
  assert(pngdrawrecovery::recover(true, 61440, 58464, &f, totalLow));
  assert(f.calls == 2);
  auto boundary = [] { return std::make_pair(61440u, 58464u); };
  assert(!pngdrawrecovery::recover(true, 61440, 58464, &f, boundary));
  assert(f.calls == 2);
}
