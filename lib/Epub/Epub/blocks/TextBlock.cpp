#include "TextBlock.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <Serialization.h>
#include <Utf8.h>
#include <VerticalTextUtils.h>

#include <algorithm>
#include <climits>
static std::vector<std::string> splitUtf8Chars(const std::string& text) {
  std::vector<std::string> chars;

  const char* p = text.c_str();

  while (*p) {
    const char* start = p;
    const unsigned char c = static_cast<unsigned char>(*p);

    if ((c & 0x80) == 0) {
      p += 1;
    } else if ((c & 0xE0) == 0xC0) {
      p += 2;
    } else if ((c & 0xF0) == 0xE0) {
      p += 3;
    } else {
      p += 4;
    }

    chars.emplace_back(start, p - start);
  }

  return chars;
}
int TextBlock::rubyFontId = 0;

void TextBlock::collectCodepoints(std::vector<uint32_t>& out, size_t max) const {
  if (max == 0 || out.size() >= max) {
    return;
  }

  for (const auto& word : words) {
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(word.c_str());
    uint32_t cp;
    while ((cp = utf8NextCodepoint(&ptr))) {
      // Check if already exists (simple linear search, OK for small sets)
      bool exists = false;
      for (uint32_t existing : out) {
        if (existing == cp) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        out.push_back(cp);
        if (out.size() >= max) {
          return;
        }
      }
    }
  }
}

bool TextBlock::hasRuby() const {
  for (const auto& rt : rubyTexts) {
    if (!rt.empty() && !isRubyContinuation(rt)) return true;
  }
  return false;
}

int TextBlock::getVerticalRubyRightOverflow(const GfxRenderer& renderer, const int bodyFontId,
                                            const int layoutColumnWidth) {
  if (rubyFontId == 0) return 0;

  int bodyColumnWidth = renderer.getTextAdvanceX(bodyFontId, "\xe4\xb8\x80", EpdFontFamily::REGULAR);
  if (bodyColumnWidth <= 0) bodyColumnWidth = renderer.getLineHeight(bodyFontId);
  const int rubyColumnWidth = renderer.getLineHeight(rubyFontId);
  const bool isBizudLikeFont = bodyColumnWidth == 29 && rubyColumnWidth == 17;
  const int gap = isBizudLikeFont ? 2 : 1;
  const int rubyBaseOffset = isBizudLikeFont ? bodyColumnWidth : bodyColumnWidth * 70 / 100;
  const int occupiedRight = rubyBaseOffset + gap + rubyColumnWidth;
  return std::max(0, occupiedRight - layoutColumnWidth);
}

int TextBlock::getHorizontalRubyTopInset(const GfxRenderer& renderer, const int bodyFontId) {
  if (rubyFontId == 0) return 0;

  const int bodyLineHeight = renderer.getLineHeight(bodyFontId);
  const int rubyLineHeight = renderer.getLineHeight(rubyFontId);
  const bool isBizudLikeFont = bodyLineHeight == 29 && rubyLineHeight == 17;
  const int gap = isBizudLikeFont ? 2 : 1;
  const int rubyBaseOffset = isBizudLikeFont ? bodyLineHeight : bodyLineHeight * 70 / 100;
  constexpr int rubyViewportSafety = 2;
  const int naturalRubyY = bodyLineHeight - rubyBaseOffset - rubyLineHeight - gap;
  return std::max(0, rubyViewportSafety - naturalRubyY);
}

void TextBlock::render(const GfxRenderer& renderer, const int fontId, const int x, const int y,
                       const int viewportWidth, const int viewportHeight, const int viewportLeft,
                       const int viewportTop, const int rubyOffsetX, const int rubyOffsetY) const {
  // Validate iterator bounds before rendering
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size()) {
    LOG_ERR("TXB", "Render skipped: size mismatch (words=%u, xpos=%u, styles=%u)\n", (uint32_t)words.size(),
            (uint32_t)wordXpos.size(), (uint32_t)wordStyles.size());
    return;
  }

  const int effectiveFontId = (blockStyle.fontId != 0) ? blockStyle.fontId : fontId;
  const bool rubyFontIsSd = (rubyFontId != 0) && renderer.isSdCardFont(rubyFontId);
  /*
  // ルビフォントのグリフをプリロード（SDカードフォントの場合）
  if (rubyFontId != 0 && hasRuby() && renderer.isSdCardFont(rubyFontId)) {
    std::string allRuby;
    for (const auto& rt : rubyTexts) {
      if (!rt.empty()) allRuby += rt;
    }
    if (!allRuby.empty()) {
      renderer.ensureSdCardFontReady(rubyFontId, allRuby.c_str());
    }
  }
*/

  // Compute column width once for Sideways/TateChuYoko centering
  int columnWidth = 0;
  if (isVertical) {
    // Use advance of CJK reference character "一" (U+4E00) as column width
    columnWidth = renderer.getTextAdvanceX(effectiveFontId, "\xe4\xb8\x80", EpdFontFamily::REGULAR);
    if (columnWidth <= 0) columnWidth = renderer.getLineHeight(effectiveFontId);
  }

  for (size_t i = 0; i < words.size(); i++) {
    const EpdFontFamily::Style currentStyle = wordStyles[i];
#if DEBUG_RUBY_RENDER
  const char* rubyForLog = "";
  if (i < rubyTexts.size()) {
    rubyForLog = rubyTexts[i].c_str();
  }

  LOG_INF(
      "TXB",
      "[WORD_RUBY_MAP] i=%u word=%s ruby=%s rubySize=%u wordsSize=%u vertical=%d",
      static_cast<unsigned>(i),
      words[i].c_str(),
      rubyForLog,
      static_cast<unsigned>(rubyTexts.size()),
      static_cast<unsigned>(words.size()),
      isVertical ? 1 : 0
  );
#endif
    if (isVertical && i < wordYpos.size()) {
      // 縦書きモード: VerticalBehaviorに応じて描画方法を分岐
      const char* w = words[i].c_str();
      const int wx = x + wordXpos[i];
      const int wy = y + wordYpos[i];

      // Classify: replicate the logic from ChapterHtmlSlimParser::flushPartWordBuffer
      const auto* p = reinterpret_cast<const unsigned char*>(w);
      uint32_t firstCp = utf8NextCodepoint(&p);
      const bool isSingleCodepoint = (firstCp != 0 && *p == '\0');

      // 縦書きでは「＝」と半角長音符「ｰ」を90度回転する。
      const bool forceSidewaysSymbol =
          isSingleCodepoint &&
          (firstCp == 0xFF1D ||  // ＝ FULLWIDTH EQUALS SIGN
           firstCp == 0xFF70);   // ｰ HALFWIDTH KATAKANA-HIRAGANA PROLONGED SOUND MARK

      const bool isSingleCjk =
          isSingleCodepoint &&
          !forceSidewaysSymbol &&
          VerticalTextUtils::isUprightInVertical(firstCp);

      if (isSingleCjk) {
        renderer.drawTextVertical(effectiveFontId, wx, wy, w, true, currentStyle);
      } else {
        bool allDigits = true;
        int asciiCount = 0;

        for (const char* c = w; *c; c++) {
          if ((static_cast<uint8_t>(*c) & 0xC0) != 0x80) {
            asciiCount++;
          }
          if (*c < '0' || *c > '9') {
            allDigits = false;
          }
        }

        if (allDigits && asciiCount <= 2) {
          // TateChuYoko: draw horizontally, centered in the column
          const int textW = renderer.getTextAdvanceX(effectiveFontId, w, currentStyle);
          const int centerOffset = (columnWidth - textW) / 2;
          renderer.drawText(effectiveFontId, wx + centerOffset, wy, w, true, currentStyle);
        } else {
          // Sideways: draw rotated 90° CW, centered in the column.
          const int vertShift = renderer.getFontAscenderSize(effectiveFontId) / 3;
          renderer.drawTextSideways(effectiveFontId, wx, wy + vertShift, w, true, currentStyle, columnWidth);
        }
      }

      if (i < rubyTexts.size() && !rubyTexts[i].empty() && !isRubyContinuation(rubyTexts[i])) {
        #if DEBUG_RUBY_RENDER
        LOG_INF(
            "TXB",
            "[RUBY_RENDER_CHECK] i=%u rubyFontId=%d word=%s ruby=%s isSingleCjk=%d x=%d y=%d",
            static_cast<unsigned>(i),
            rubyFontId,
            words[i].c_str(),
            rubyTexts[i].c_str(),
            isSingleCjk ? 1 : 0,
            wx,
            wy
        );
        #endif
      }

                  // 縦書きルビ描画
      if (rubyFontId != 0 && i < rubyTexts.size() && !rubyTexts[i].empty() &&
          !isRubyContinuation(rubyTexts[i])) {
        #if DEBUG_RUBY_RENDER
        LOG_INF(
            "TXB",
            "[RUBY_DRAW] i=%u rubyFontId=%d word=%s ruby=%s x=%d y=%d",
            static_cast<unsigned>(i),
            rubyFontId,
            words[i].c_str(),
            rubyTexts[i].c_str(),
            wx,
            wy
        );
        #endif
        if (rubyFontIsSd) {
          renderer.ensureSdCardFontReady(rubyFontId, rubyTexts[i].c_str());
        }

        const int rubyColumnWidth = renderer.getLineHeight(rubyFontId);

        // BIZUD系は列幅が大きく、従来位置でちょうどよい。
        // その他SDフォントは columnWidth のままだと離れすぎるため本文側へ寄せる。
        const bool isBizudLikeFont = (columnWidth == 29 && rubyColumnWidth == 17);

        const int gap = isBizudLikeFont ? 2 : 1;
        const int rubyBaseOffset = isBizudLikeFont ? columnWidth : columnWidth * 70 / 100;

        const int viewportRight = viewportLeft + viewportWidth;
        const int rightBaseX = wx + rubyBaseOffset + gap;
        const int leftBaseX = wx - rubyColumnWidth - gap;
        int rubyBaseX = rightBaseX;
        if (viewportWidth > 0) {
          // Apply the user adjustment to both candidates, then choose the side
          // with less viewport overflow. Equal candidates keep standard right-side ruby.
          const int adjustedRightX = rightBaseX + rubyOffsetX;
          const int adjustedLeftX = leftBaseX + rubyOffsetX;
          const int rightOverflow = std::max(0, viewportLeft - adjustedRightX) +
                                    std::max(0, adjustedRightX + rubyColumnWidth - viewportRight);
          const int leftOverflow =
              std::max(0, viewportLeft - adjustedLeftX) + std::max(0, adjustedLeftX + rubyColumnWidth - viewportRight);
          if (leftOverflow < rightOverflow) rubyBaseX = leftBaseX;
        }
        const int minRubyX = viewportWidth > 0 ? viewportLeft : 0;
        const int maxRubyX = viewportWidth > 0 ? std::max(minRubyX, viewportRight - rubyColumnWidth) : INT_MAX;
        const int rubyX = std::clamp(rubyBaseX + rubyOffsetX, minRubyX, maxRubyX);

        const int rubyTextHeight =
            std::max(rubyColumnWidth,
                     renderer.getTextAdvanceYVertical(rubyFontId, rubyTexts[i].c_str(), EpdFontFamily::REGULAR));
        constexpr int rubyViewportSafety = 2;
        const int minRubyY = viewportHeight > 0 ? viewportTop + rubyViewportSafety : 0;
        const int maxRubyY =
            viewportHeight > 0
                ? std::max(minRubyY, viewportTop + viewportHeight - rubyTextHeight - rubyViewportSafety)
                : INT_MAX;
        const int rubyY = std::clamp(wy + rubyOffsetY, minRubyY, maxRubyY);

        renderer.setRubyFastAaBoost(true);
        renderer.drawTextVertical(
            rubyFontId,
            rubyX,
            rubyY,
            rubyTexts[i].c_str(),
            true,
            EpdFontFamily::REGULAR
        );
        renderer.setRubyFastAaBoost(false);
      }
    } else {
      const int wordX = wordXpos[i] + x;
      renderer.drawText(effectiveFontId, wordX, y, words[i].c_str(), true, currentStyle);
      // 横書きルビ描画
      if (rubyFontId != 0 && i < rubyTexts.size() && !rubyTexts[i].empty() &&
          !isRubyContinuation(rubyTexts[i])) {
        if (rubyFontIsSd) {
          renderer.ensureSdCardFontReady(rubyFontId, rubyTexts[i].c_str());
        }

        size_t rubyBaseEnd = i;
        while (rubyBaseEnd + 1 < rubyTexts.size() && isRubyContinuation(rubyTexts[rubyBaseEnd + 1])) {
          rubyBaseEnd++;
        }

        // Center over the complete <ruby> base, not only its first CJK token.
        const int lastBaseAdvance = renderer.getTextAdvanceX(
            effectiveFontId, words[rubyBaseEnd].c_str(), wordStyles[rubyBaseEnd]);
        const int baseWidth = wordXpos[rubyBaseEnd] - wordXpos[i] + lastBaseAdvance;
        const int rubyWidth =
            renderer.getTextAdvanceX(rubyFontId, rubyTexts[i].c_str(), EpdFontFamily::REGULAR);
        const int viewportRight = viewportLeft + viewportWidth;
        const int minRubyX = viewportWidth > 0 ? viewportLeft : 0;
        const int maxRubyX = viewportWidth > 0 ? std::max(minRubyX, viewportRight - rubyWidth) : INT_MAX;
        const int rubyX = std::clamp(wordX + (baseWidth - rubyWidth) / 2 + rubyOffsetX, minRubyX, maxRubyX);
        const int bodyLineHeight = renderer.getLineHeight(effectiveFontId);
        const int rubyLineHeight = renderer.getLineHeight(rubyFontId);

        // Mirror the vertical ruby spacing correction onto the horizontal cross-axis.
        // BIZUD-like metrics keep the original separation; other SD fonts move closer to the body text.
        const bool isBizudLikeFont = (bodyLineHeight == 29 && rubyLineHeight == 17);
        const int gap = isBizudLikeFont ? 2 : 1;
        const int rubyBaseOffset = isBizudLikeFont ? bodyLineHeight : bodyLineHeight * 70 / 100;
        constexpr int rubyViewportSafety = 2;
        const int minRubyY = viewportHeight > 0 ? viewportTop + rubyViewportSafety : 0;
        const int maxRubyY =
            viewportHeight > 0
                ? std::max(minRubyY, viewportTop + viewportHeight - rubyLineHeight - rubyViewportSafety)
                : INT_MAX;
        const int rubyY = std::clamp(y + bodyLineHeight - rubyBaseOffset - rubyLineHeight - gap + rubyOffsetY,
                                     minRubyY, maxRubyY);
        renderer.drawText(rubyFontId, rubyX, rubyY, rubyTexts[i].c_str(), true, EpdFontFamily::REGULAR);
      }

      if ((currentStyle & EpdFontFamily::UNDERLINE) != 0) {
        const std::string& w = words[i];
        const int fullWordWidth = renderer.getTextWidth(effectiveFontId, w.c_str(), currentStyle);
        // y is the top of the text line; add ascender to reach baseline, then offset 2px below
        const int underlineY = y + renderer.getFontAscenderSize(effectiveFontId) + 2;

        int startX = wordX;
        int underlineWidth = fullWordWidth;

        // if word starts with em-space ("\xe2\x80\x83"), account for the additional indent before drawing the line
        if (w.size() >= 3 && static_cast<uint8_t>(w[0]) == 0xE2 && static_cast<uint8_t>(w[1]) == 0x80 &&
            static_cast<uint8_t>(w[2]) == 0x83) {
          const char* visiblePtr = w.c_str() + 3;
          const int prefixWidth = renderer.getTextAdvanceX(effectiveFontId, "\xe2\x80\x83", currentStyle);
          const int visibleWidth = renderer.getTextWidth(effectiveFontId, visiblePtr, currentStyle);
          startX = wordX + prefixWidth;
          underlineWidth = visibleWidth;
        }

        renderer.drawLine(startX, underlineY, startX + underlineWidth, underlineY, true);
      }
    }
  }

  // Draw full-width separator line below the block (used for h1/h2 headings).
  // Suppressed in vertical mode: horizontal lines are inappropriate for tategaki.
  if (blockStyle.drawSeparatorBelow && viewportWidth > 0 && !isVertical) {
    const int separatorY = y + renderer.getLineHeight(effectiveFontId) + 2;
    renderer.drawLine(0, separatorY, viewportWidth, separatorY, true);
  }
}

bool TextBlock::serialize(FsFile& file) const {
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size()) {
    LOG_ERR("TXB", "Serialization failed: size mismatch (words=%u, xpos=%u, styles=%u)\n", words.size(),
            wordXpos.size(), wordStyles.size());
    return false;
  }

  // Word data
  serialization::writePod(file, static_cast<uint16_t>(words.size()));
  for (const auto& w : words) serialization::writeString(file, w);
  for (auto x : wordXpos) serialization::writePod(file, x);
  for (auto s : wordStyles) serialization::writePod(file, s);

  // Style (alignment + margins/padding/indent)
  serialization::writePod(file, blockStyle.alignment);
  serialization::writePod(file, blockStyle.textAlignDefined);
  serialization::writePod(file, blockStyle.marginTop);
  serialization::writePod(file, blockStyle.marginBottom);
  serialization::writePod(file, blockStyle.marginLeft);
  serialization::writePod(file, blockStyle.marginRight);
  serialization::writePod(file, blockStyle.paddingTop);
  serialization::writePod(file, blockStyle.paddingBottom);
  serialization::writePod(file, blockStyle.paddingLeft);
  serialization::writePod(file, blockStyle.paddingRight);
  serialization::writePod(file, blockStyle.textIndent);
  serialization::writePod(file, blockStyle.textIndentDefined);
  serialization::writePod(file, blockStyle.fontId);
  serialization::writePod(file, blockStyle.drawSeparatorBelow);
  serialization::writePod(file, blockStyle.isListItem);

  // Vertical layout data
  serialization::writePod(file, isVertical);
  if (isVertical) {
    for (auto y : wordYpos) serialization::writePod(file, y);
  }

  // Ruby text data
  for (size_t i = 0; i < words.size(); i++) {
    serialization::writeString(file, (i < rubyTexts.size()) ? rubyTexts[i] : std::string());
  }

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(FsFile& file) {
  uint16_t wc;
  std::vector<std::string> words;
  std::vector<int16_t> wordXpos;
  std::vector<EpdFontFamily::Style> wordStyles;
  BlockStyle blockStyle;

  // Word count
  serialization::readPod(file, wc);

  // Sanity check: prevent allocation of unreasonably large vectors (max 10000 words per block)
  if (wc > 10000) {
    LOG_ERR("TXB", "Deserialization failed: word count %u exceeds maximum", wc);
    return nullptr;
  }

  // Word data
  words.resize(wc);
  wordXpos.resize(wc);
  wordStyles.resize(wc);
  for (auto& w : words) serialization::readString(file, w);
  for (auto& x : wordXpos) serialization::readPod(file, x);
  for (auto& s : wordStyles) serialization::readPod(file, s);

  // Style (alignment + margins/padding/indent)
  serialization::readPod(file, blockStyle.alignment);
  serialization::readPod(file, blockStyle.textAlignDefined);
  serialization::readPod(file, blockStyle.marginTop);
  serialization::readPod(file, blockStyle.marginBottom);
  serialization::readPod(file, blockStyle.marginLeft);
  serialization::readPod(file, blockStyle.marginRight);
  serialization::readPod(file, blockStyle.paddingTop);
  serialization::readPod(file, blockStyle.paddingBottom);
  serialization::readPod(file, blockStyle.paddingLeft);
  serialization::readPod(file, blockStyle.paddingRight);
  serialization::readPod(file, blockStyle.textIndent);
  serialization::readPod(file, blockStyle.textIndentDefined);
  serialization::readPod(file, blockStyle.fontId);
  serialization::readPod(file, blockStyle.drawSeparatorBelow);
  serialization::readPod(file, blockStyle.isListItem);

  // Vertical layout data
  bool vertical = false;
  serialization::readPod(file, vertical);
  std::vector<int16_t> wordYpos;
  if (vertical) {
    wordYpos.resize(wc);
    for (auto& y : wordYpos) serialization::readPod(file, y);
  }

  // Ruby text data
  std::vector<std::string> rubyTexts(wc);
  for (auto& rt : rubyTexts) serialization::readString(file, rt);

  return std::unique_ptr<TextBlock>(new TextBlock(std::move(words), std::move(wordXpos), std::move(wordStyles),
                                                  blockStyle, std::move(wordYpos), vertical, std::move(rubyTexts)));
}
