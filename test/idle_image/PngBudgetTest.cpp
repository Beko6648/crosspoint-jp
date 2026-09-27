#include <cassert>
#include <limits>

#include "PngDecodeBudget.h"
int main() {
  constexpr size_t decoder = 58464, gray = 1200, band = 252, reserve = 32768;
  constexpr size_t required = decoder + gray + band + reserve + pngbudget::OVERHEAD_BYTES;
  assert(!pngbudget::admits(80116, 34804, decoder, gray, band, reserve));  // X3 first PNG
  assert(!pngbudget::admits(91236, 65524, decoder, gray, band, reserve));  // X3 second PNG
  assert(!pngbudget::admits(97664, 57332, decoder, gray, 228, reserve));   // X4 second PNG
  assert(pngbudget::admits(required, 65524, decoder, gray, band, reserve));
  assert(!pngbudget::admits(required - 1, 65524, decoder, gray, band, reserve));
  assert(!pngbudget::admits(required, decoder - 1, decoder, gray, band, reserve));
  assert(pngbudget::admits(118648, 98292, decoder, gray, band, reserve));  // English fixture
  assert(!pngbudget::admits(std::numeric_limits<size_t>::max(), decoder, decoder, gray, band,
                            std::numeric_limits<size_t>::max()));
}
