#include <GlyphDownsample.h>

#include <cassert>
#include <vector>

static void set(std::vector<uint8_t>& data, int p, int value) { data[p / 4] |= value << ((3 - p % 4) * 2); }

int main() {
  // Every possible single-pixel rule must survive 18pt -> 16pt. Nearest
  // sampling used to omit source rows 8, 17, 26 and 35 for this glyph size.
  const int w = 37, h = 38, dw = 33, dh = 34;
  for (int row = 0; row < h; ++row) {
    std::vector<uint8_t> data((w * h + 3) / 4);
    for (int x = 0; x < w; ++x) set(data, row * w + x, 3);
    bool visible = false;
    for (int y = 0; y < dh; ++y) visible |= downsampleGlyphInk(data.data(), w, h, dw, dh, 16, y) > 0;
    assert(visible);
  }
  for (int col = 0; col < w; ++col) {
    std::vector<uint8_t> data((w * h + 3) / 4);
    for (int y = 0; y < h; ++y) set(data, y * w + col, 3);
    bool visible = false;
    for (int x = 0; x < dw; ++x) visible |= downsampleGlyphInk(data.data(), w, h, dw, dh, x, 16) > 0;
    assert(visible);
  }
  // Odd packed widths, edge pixels, constant tones and identity scaling.
  for (int tone = 0; tone <= 3; ++tone) {
    std::vector<uint8_t> data((w * h + 3) / 4);
    for (int p = 0; p < w * h; ++p) set(data, p, tone);
    for (int y = 0; y < dh; ++y)
      for (int x = 0; x < dw; ++x) assert(downsampleGlyphInk(data.data(), w, h, dw, dh, x, y) == tone);
  }
  std::vector<uint8_t> data((w * h + 3) / 4);
  for (int p = 0; p < w * h; ++p) set(data, p, p % 4);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) assert(downsampleGlyphInk(data.data(), w, h, w, h, x, y) == (y * w + x) % 4);
}
