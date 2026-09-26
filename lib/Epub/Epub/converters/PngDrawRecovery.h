#pragma once

#include <cstddef>

namespace pngdrawrecovery {
// Only foreground decode may evict optional vertical glyph data. The next
// vertical text render reloads it; cache-only work preserves reader fonts.
template <typename Fonts, typename ReadHeap>
bool recover(bool drawing, size_t minimumFree, size_t decoderBytes, Fonts* fonts, ReadHeap readHeap) {
  if (!drawing || !fonts) return false;
  const auto heap = readHeap();
  if (heap.first >= minimumFree && heap.second >= decoderBytes) return false;
  fonts->releaseSdFontVerticalGlyphs();
  return true;
}
}  // namespace pngdrawrecovery
