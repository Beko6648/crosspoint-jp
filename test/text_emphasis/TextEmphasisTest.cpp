#include <GfxRenderer.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>

#include "Epub/blocks/TextBlock.h"
#include "Epub/css/CssParser.h"

// Link only the production emphasis renderer; unrelated baseline ruby layout is not under test here.
int TextBlock::rubyFontId = 0;
bool TextBlock::hasRuby() const {
  for (const auto& s : rubyTexts)
    if (!s.empty()) return true;
  return false;
}
int TextBlock::getVerticalRubyRightOverflow(const GfxRenderer&, int, int) { return 0; }
int TextBlock::getHorizontalRubyTopInset(const GfxRenderer&, int) { return 0; }

TextBlock block(std::string text, TextEmphasis mark, bool vertical = false) {
  return TextBlock({text}, {0}, {EpdFontFamily::REGULAR}, {}, {0}, vertical, {}, {}, {mark});
}
int ink(const GfxRenderer& r) { return std::count(r.pixels.begin(), r.pixels.end(), 0); }
int main(int argc, char** argv) {
  using E = TextEmphasis;
  E e = E::None;
  assert(textEmphasis::parse("open sesame", e) && e == E::OpenSesame);
  assert(textEmphasis::parse("SESAME FILLED", e) && e == E::FilledSesame);
  assert(textEmphasis::parse("none", e) && e == E::None);
  assert(textEmphasis::parse("open", e) && textEmphasis::codepoint(e, true) == 0xfe46 &&
         textEmphasis::codepoint(e, false) == 0x25e6);
  assert(textEmphasis::parse("filled sesame black", e, true) && e == E::FilledSesame);
  for (auto s : {"", "invalid", "inherit", "none dot", "open filled", "dot circle", "'x'"})
    assert(!textEmphasis::parse(s, e));
  CssStyle parent, child;
  parent.emphasis = E::FilledSesame;
  parent.emphasisDefined = true;
  child.emphasis = E::None;
  child.emphasisDefined = true;
  parent.applyOver(child);
  assert(parent.emphasis == E::None && parent.anySet());
  parent.reset();
  assert(!parent.emphasisDefined);
  for (auto name : {"text-emphasis", "text-emphasis-style", "-epub-text-emphasis", "-epub-text-emphasis-style",
                    "-webkit-text-emphasis", "-webkit-text-emphasis-style"}) {
    const auto style = CssParser::parseInlineStyle(std::string(name) + ":open sesame !important");
    assert(style.emphasisDefined && style.emphasis == E::OpenSesame);
  }
  FsFile css;
  Storage.openFileForWrite("TEST", "/source", css);
  const std::string rules =
      ".mark{text-emphasis:filled sesame;color:black}.off{text-emphasis:none}.other{width:40%;font-weight:bold}";
  css.write(reinterpret_cast<const uint8_t*>(rules.data()), rules.size());
  CssParser parser("/cache");
  parser.setCacheSourceFingerprint(123);
  assert(parser.loadFromStream(css));
  assert(parser.saveToCache());
  CssParser restored("/cache");
  restored.setCacheSourceFingerprint(123);
  assert(restored.loadFromCache());
  assert(restored.resolveStyle("span", "mark").emphasis == E::FilledSesame);
  assert(restored.resolveStyle("span", "off").emphasisDefined &&
         restored.resolveStyle("span", "off").emphasis == E::None);
  assert(restored.resolveStyle("span", "other").imageWidth.value == 40);
  std::string cachePath;
  for (const auto& item : Storage.files)
    if (item.first.find("/cache/") == 0) cachePath = item.first;
  assert(!cachePath.empty());
  const auto good = *Storage.files[cachePath];
  for (size_t length = 0; length < good.size(); ++length) {
    Storage.files[cachePath] = std::make_shared<std::vector<uint8_t>>(good.begin(), good.begin() + length);
    CssParser truncated("/cache");
    truncated.setCacheSourceFingerprint(123);
    assert(!truncated.loadFromCache());
  }
  Storage.files[cachePath] = std::make_shared<std::vector<uint8_t>>(good);
  (*Storage.files[cachePath])[0] = 7;
  assert(!restored.loadFromCache());
  for (auto cp : {0x20, 0x3000, 0x3001, 0x3002, 0x3099, 0xfe0f, 0xe0100, 0xfffc}) assert(!textEmphasis::eligible(cp));
  for (auto cp : {0x3005, 0x4e00, 0x3042, 'A' + 0, '3' + 0}) assert(textEmphasis::eligible(cp));
  GfxRenderer missing;
  block("A", E::FilledSesame).renderEmphasis(missing, 1, 20, 30);
  assert(ink(missing) > 0);
  assert(missing.fonts[1].probes[0xfe45] == 1 && missing.fonts[1].probes[0x2022] == 1);
  GfxRenderer open;
  block("A", E::OpenSesame).renderEmphasis(open, 1, 20, 30);
  assert(ink(open) > 0 && ink(open) < ink(missing));
  GfxRenderer fallback;
  fallback.fonts[1].glyphs[0x2022] = {};
  block("AAAA", E::FilledSesame).renderEmphasis(fallback, 1, 20, 30);
  assert(ink(fallback) > 0 && fallback.fonts[1].probes[0xfe45] == 1 && fallback.fonts[1].probes[0x2022] == 1);
  GfxRenderer exact;
  exact.fonts[1].glyphs[0xfe45] = {};
  block("A", E::FilledSesame).renderEmphasis(exact, 1, 20, 30);
  assert(ink(exact) > 0 && exact.fonts[1].probes[0x2022] == 0);
  auto reusable = block("A", E::FilledSesame);
  GfxRenderer changedFont;
  reusable.renderEmphasis(changedFont, 1, 20, 30);
  const auto before = changedFont.pixels;
  std::fill(changedFont.pixels.begin(), changedFont.pixels.end(), 255);
  changedFont.fonts[1].glyphs[0xfe45] = {};
  reusable.renderEmphasis(changedFont, 1, 20, 30);
  assert(before != changedFont.pixels);
  TextBlock withRuby({"A"}, {0}, {EpdFontFamily::REGULAR}, {}, {0}, false, {"ruby"}, {}, {E::FilledDot});
  TextBlock::rubyFontId = 2;
  assert(withRuby.annotationTopInset(missing, 1) > block("A", E::FilledDot).annotationTopInset(missing, 1));
  TextBlock::rubyFontId = 0;
  GfxRenderer rubyOff;
  withRuby.renderEmphasis(rubyOff, 1, 20, 30);
  assert(ink(rubyOff) > 0);
  GfxRenderer longRun;
  block(std::string(52, 'A'), E::FilledDot).renderEmphasis(longRun, 1, 20, 30);
  assert(ink(longRun) == ink(missing) * 52);
  GfxRenderer punct;
  block(" ,.!", E::FilledDot).renderEmphasis(punct, 1, 20, 30);
  assert(ink(punct) == 0);
  GfxRenderer scan;
  scan.cache.scanning = true;
  block("A", E::FilledDot).renderEmphasis(scan, 1, 20, 30);
  assert(ink(scan) == 0);
  auto v = block("A", E::FilledDot, true);
  assert(v.annotationRightOverflow(missing, 1, 25) > 0 && v.annotationTopInset(missing, 1) > 0);
  GfxRenderer vertical;
  v.renderEmphasis(vertical, 1, 20, 30);
  assert(ink(vertical) > 0);
  GfxRenderer verticalRun;
  block("ABC", E::FilledDot, true).renderEmphasis(verticalRun, 1, 20, 30);
  assert(ink(verticalRun) == ink(missing) * 3);
  if (argc > 1) {
    GfxRenderer sheet;
    block("FILLED", E::FilledSesame).renderEmphasis(sheet, 1, 20, 30);
    block("OPEN", E::OpenSesame).renderEmphasis(sheet, 1, 20, 70);
    sheet.fonts[1].glyphs[0x2022] = {};
    block("GLYPH", E::FilledSesame).renderEmphasis(sheet, 1, 20, 110);
    std::ofstream out(argv[1], std::ios::binary);
    out << "P5\n" << GfxRenderer::W << " " << GfxRenderer::H << "\n255\n";
    out.write(reinterpret_cast<const char*>(sheet.pixels.data()), sheet.pixels.size());
  }
  std::cout << "PASS: CSS/cache roundtrip and truncation, inheritance, font fallback, geometric fallback, 52 "
               "characters, punctuation, scan and vertical placement\n";
}
