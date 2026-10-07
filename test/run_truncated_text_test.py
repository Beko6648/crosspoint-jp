#!/usr/bin/env python3
"""Compare the production truncation function against the previous algorithm.

Host metrics model UI character advances; real font/device rendering is a
separate acceptance check. Run with CXX pointing to a native C++ compiler.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
renderer = (root / "lib/GfxRenderer/GfxRenderer.cpp").read_text(encoding="utf-8")
start = renderer.index("std::string GfxRenderer::truncatedText(")
end = renderer.index("std::vector<std::string> GfxRenderer::wrappedText(", start)
production = renderer[start:end]
source = r'''
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>
struct EpdFontFamily { enum Style { REGULAR, BOLD }; };
struct GfxRenderer {
  mutable unsigned calls = 0;
  int getTextWidth(int fontId, const char* text, EpdFontFamily::Style style) const {
    ++calls;
    int width = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
      if ((*p & 0xc0) == 0x80) continue;
      // Include missing/zero-width glyphs, ASCII and multibyte UI characters.
      width += *p == '~' ? 0 : (*p < 128 ? 5 + *p % 9 : 21);
    }
    return (width * (style == EpdFontFamily::BOLD ? 2 : 1) * fontId + 1) / 2;
  }
  std::string truncatedText(int, const char*, int, EpdFontFamily::Style) const;
};
std::string previous(const GfxRenderer& renderer, int fontId, const char* text,
                     int maxWidth, EpdFontFamily::Style style) {
  if (!text || maxWidth <= 0) return "";
  std::string item = text;
  const char* ellipsis = "\xe2\x80\xa6";
  if (renderer.getTextWidth(fontId, item.c_str(), style) <= maxWidth) return item;
  while (!item.empty() && renderer.getTextWidth(fontId, (item + ellipsis).c_str(), style) >= maxWidth) {
    size_t pos = item.size() - 1;
    while (pos > 0 && (static_cast<unsigned char>(item[pos]) & 0xc0) == 0x80) --pos;
    item.resize(pos);
  }
  return item.empty() ? ellipsis : item + ellipsis;
}
''' + production + r'''
int main() {
  GfxRenderer renderer;
  unsigned checks = 0;
  const auto check = [&](const char* text, int width, int fontId, EpdFontFamily::Style style) {
    const auto expected = previous(renderer, fontId, text, width, style);
    const auto actual = renderer.truncatedText(fontId, text, width, style);
    assert(actual == expected);
    ++checks;
  };
  const std::vector<std::string> cases = {
    "", "a", "~", "~~~~", "ASCII title", u8"日本語の長い章タイトル",
    u8"日本語 ABC 123…", u8"𠮷野家と📚の題名", u8"か\u3099き\u3099", "a~~~b"
  };
  for (const auto& text : cases)
    for (int fontId : {1, 2, 3})
      for (auto style : {EpdFontFamily::REGULAR, EpdFontFamily::BOLD})
        for (int width = -1; width <= 600; ++width) check(text.c_str(), width, fontId, style);
  check(nullptr, 100, 2, EpdFontFamily::REGULAR);
  std::mt19937 random(3573);
  const std::vector<std::string> characters = {"a", "W", " ", "~", u8"本", u8"𠮷", u8"📚", u8"\u3099"};
  for (int i = 0; i < 10000; ++i) {
    std::string text;
    const unsigned length = random() % 300;
    for (unsigned j = 0; j < length; ++j) text += characters[random() % characters.size()];
    check(text.c_str(), random() % 1500, 1 + random() % 3,
          random() % 2 ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD);
  }
  std::string longTitle;
  for (int i = 0; i < 1000; ++i) longTitle += u8"本";
  renderer.calls = 0;
  const auto expected = previous(renderer, 2, longTitle.c_str(), 300, EpdFontFamily::REGULAR);
  const unsigned oldCalls = renderer.calls;
  renderer.calls = 0;
  assert(renderer.truncatedText(2, longTitle.c_str(), 300, EpdFontFamily::REGULAR) == expected);
  assert(renderer.calls <= 12);
  std::cout << "PASS: " << checks << " comparisons; 1000-character title width calls "
            << oldCalls << " -> " << renderer.calls << "\n";
}
'''
with tempfile.TemporaryDirectory(prefix="yomuka-truncated-text-") as directory:
    path = Path(directory)
    cpp = path / "test.cpp"
    cpp.write_text(source, encoding="utf-8")
    exe = path / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-O2", "-Wall",
                    "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
