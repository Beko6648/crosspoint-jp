#pragma once

#include <algorithm>
#include <cstdint>

// Area coverage for packed 2-bit glyphs (0 = paper, 3 = ink). Integer
// coordinates use the destination dimensions as units, so no source row or
// column is skipped. Call only for nonempty glyphs and destination dimensions.
inline uint8_t downsampleGlyphInk(const uint8_t* bitmap, int width, int height, int drawWidth, int drawHeight, int x,
                                  int y) {
  const int left = x * width;
  const int right = left + width;
  const int top = y * height;
  const int bottom = top + height;
  uint32_t ink = 0;
  for (int sy = top / drawHeight; sy < (bottom + drawHeight - 1) / drawHeight; ++sy) {
    const int weightY = std::min(bottom, (sy + 1) * drawHeight) - std::max(top, sy * drawHeight);
    for (int sx = left / drawWidth; sx < (right + drawWidth - 1) / drawWidth; ++sx) {
      const int weightX = std::min(right, (sx + 1) * drawWidth) - std::max(left, sx * drawWidth);
      const int position = sy * width + sx;
      const uint8_t value = (bitmap[position / 4] >> ((3 - position % 4) * 2)) & 3;
      ink += value * weightX * weightY;
    }
  }
  const uint32_t area = width * height;
  return static_cast<uint8_t>((ink + area / 2) / area);
}
