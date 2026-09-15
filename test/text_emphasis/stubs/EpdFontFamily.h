#pragma once
#include <array>
#include <cstdint>
#include <map>
struct EpdGlyph {
  uint8_t width = 5, height = 5;
};
struct EpdFontData {
  bool is2Bit = false;
};
class EpdFontFamily {
 public:
  enum Style : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
    UNDERLINE = 4,
    STRIKETHROUGH = 8,
    SUPERSCRIPT = 16,
    SUBSCRIPT = 32,
    SCRIPT_MASK = SUPERSCRIPT | SUBSCRIPT
  };
  static constexpr uint8_t FONT_SELECT_MASK = BOLD | ITALIC;
  std::map<uint32_t, EpdGlyph> glyphs;
  mutable std::map<uint32_t, int> probes;
  EpdFontData data;
  const EpdGlyph* getGlyphExact(uint32_t cp) const {
    probes[cp]++;
    auto it = glyphs.find(cp);
    return it == glyphs.end() ? nullptr : &it->second;
  }
  const EpdFontData* getData() const { return &data; }
};
