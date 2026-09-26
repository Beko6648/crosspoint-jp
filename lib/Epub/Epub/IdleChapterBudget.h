#pragma once
#include <cstddef>
namespace idlechapterbudget {
// Cover the temporary selector scan and a few matching rules before the
// existing post-CSS admission test. This is not a lowered parser reserve.
constexpr size_t CSS_LOAD_ALLOWANCE = 4 * 1024;
template <class Fonts, class ReadHeap>
bool recover(size_t postCssReserve, size_t minimumBlock, Fonts* fonts, ReadHeap readHeap) {
  const auto heap = readHeap();
  if (!fonts || (heap.first >= postCssReserve + CSS_LOAD_ALLOWANCE && heap.second >= minimumBlock)) return false;
  // Only rebuildable glyph/advance data. Preserve kern/ligature and vertical data.
  fonts->clearCache();
  return true;
}
}  // namespace idlechapterbudget
