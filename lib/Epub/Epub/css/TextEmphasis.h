#pragma once

#include <cstdint>
#include <string_view>

// Persist the author's choice, not a font-dependent fallback glyph.
enum class TextEmphasis : uint8_t {
  None,
  FilledDot,
  OpenDot,
  FilledCircle,
  OpenCircle,
  FilledSesame,
  OpenSesame,
  FilledTriangle,
  OpenTriangle,
  FilledDoubleCircle,
  OpenDoubleCircle,
  AutoFilled,
  AutoOpen
};

namespace textEmphasis {
constexpr bool valid(uint8_t value) { return value <= static_cast<uint8_t>(TextEmphasis::AutoOpen); }
constexpr bool open(TextEmphasis value) {
  return value == TextEmphasis::OpenDot || value == TextEmphasis::OpenCircle || value == TextEmphasis::OpenSesame ||
         value == TextEmphasis::OpenTriangle || value == TextEmphasis::OpenDoubleCircle ||
         value == TextEmphasis::AutoOpen;
}
constexpr uint32_t codepoint(TextEmphasis value, bool vertical) {
  switch (value) {
    case TextEmphasis::FilledDot:
      return 0x2022;
    case TextEmphasis::OpenDot:
      return 0x25e6;
    case TextEmphasis::FilledCircle:
      return 0x25cf;
    case TextEmphasis::OpenCircle:
      return 0x25cb;
    case TextEmphasis::FilledSesame:
      return 0xfe45;
    case TextEmphasis::OpenSesame:
      return 0xfe46;
    case TextEmphasis::FilledTriangle:
      return 0x25b2;
    case TextEmphasis::OpenTriangle:
      return 0x25b3;
    case TextEmphasis::FilledDoubleCircle:
      return 0x25c9;
    case TextEmphasis::OpenDoubleCircle:
      return 0x25ce;
    case TextEmphasis::AutoFilled:
      return vertical ? 0xfe45 : 0x2022;
    case TextEmphasis::AutoOpen:
      return vertical ? 0xfe46 : 0x25e6;
    default:
      return 0;
  }
}

// Whitespace, punctuation and nonspacing characters do not get their own mark.
constexpr bool eligible(uint32_t cp) {
  if (cp == 0x3005 || cp == 0x3006 || cp == 0x3007 || cp == 0x303b || cp == 0x303c) return true;
  return cp > 0x20 && cp != 0xa0 && cp != 0xad && cp != 0x3000 && cp != 0xfffc && cp != 0x7f && cp != 0x30fb &&
         cp != 0x309b && cp != 0x309c && !(cp >= 0x2000 && cp <= 0x206f) && !(cp >= 0x3001 && cp <= 0x303f) &&
         !(cp >= 0x300 && cp <= 0x36f) && !(cp >= 0xfe00 && cp <= 0xfe0f) && !(cp >= 0xe0100 && cp <= 0xe01ef) &&
         cp != 0x3099 && cp != 0x309a && !(cp >= 0xff61 && cp <= 0xff65) && cp != 0xff9e && cp != 0xff9f &&
         !(cp >= 0xff01 && cp <= 0xff0f) && !(cp >= 0xff1a && cp <= 0xff20) && !(cp >= 0xff3b && cp <= 0xff40) &&
         !(cp >= 0xff5b && cp <= 0xff60) && !(cp >= '!' && cp <= '/') && !(cp >= ':' && cp <= '@') &&
         !(cp >= '[' && cp <= '`') && !(cp >= '{' && cp <= '~');
}

inline bool equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char c = a[i];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c != b[i]) return false;
  }
  return true;
}

inline bool hexColor(std::string_view token) {
  if (token.empty() || token.front() != '#') return false;
  if (token.size() != 4 && token.size() != 5 && token.size() != 7 && token.size() != 9) return false;
  for (size_t i = 1; i < token.size(); ++i) {
    const char c = token[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

// Unsupported declarations are ignored, never turned into accidental sesame marks.
inline bool parse(std::string_view value, TextEmphasis& result, bool shorthand = false) {
  bool hasFill = false, isOpen = false, hasShape = false, none = false;
  TextEmphasis shape = TextEmphasis::AutoFilled;
  while (!value.empty()) {
    const size_t start = value.find_first_not_of(" \t\r\n\f");
    if (start == std::string_view::npos) break;
    value.remove_prefix(start);
    const size_t end = value.find_first_of(" \t\r\n\f");
    const auto token = value.substr(0, end);
    value.remove_prefix(end == std::string_view::npos ? value.size() : end);
    if (equal(token, "none")) {
      if (none || hasFill || hasShape) return false;
      none = true;
    } else if (equal(token, "open") || equal(token, "filled")) {
      if (hasFill || none) return false;
      hasFill = true;
      isOpen = equal(token, "open");
    } else {
      TextEmphasis candidate;
      if (equal(token, "dot"))
        candidate = TextEmphasis::FilledDot;
      else if (equal(token, "circle"))
        candidate = TextEmphasis::FilledCircle;
      else if (equal(token, "sesame"))
        candidate = TextEmphasis::FilledSesame;
      else if (equal(token, "triangle"))
        candidate = TextEmphasis::FilledTriangle;
      else if (equal(token, "double-circle"))
        candidate = TextEmphasis::FilledDoubleCircle;
      else if (shorthand && (equal(token, "black") || equal(token, "currentcolor") || equal(token, "red") ||
                             equal(token, "blue") || hexColor(token)))
        continue;
      else
        return false;
      if (hasShape || none) return false;
      hasShape = true;
      shape = candidate;
    }
  }
  if (!none && !hasFill && !hasShape) return false;
  result = none ? TextEmphasis::None : static_cast<TextEmphasis>(static_cast<uint8_t>(shape) + (isOpen ? 1 : 0));
  return true;
}
}  // namespace textEmphasis
