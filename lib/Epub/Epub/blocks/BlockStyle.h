#pragma once

#include <algorithm>
#include <cstdint>

#include "Epub/css/CssStyle.h"

/**
 * BlockStyle - Block-level styling properties
 */
struct BlockStyle {
  // Upper bound (in em) for any single side's horizontal margin or padding.
  // Some EPUBs apply huge em-based insets to chapter-opener classes; without a
  // cap, effectiveWidth collapses to 1-2 words per line and justification dumps
  // the remaining space into a single gap.
  static constexpr float MAX_HORIZONTAL_INSET_EM = 2.0f;
  // Vertical writing maps h1/h2 after-block spacing to the next column. Keep
  // EPUB chapter-openers from reserving most of a page between a heading and
  // its body while retaining normal book-defined paragraph spacing.
  static constexpr float MAX_VERTICAL_BLOCK_SPACING_EM = 2.0f;

  CssTextAlign alignment = CssTextAlign::Justify;

  // Spacing (in pixels)
  int16_t marginTop = 0;
  int16_t marginBottom = 0;
  int16_t marginLeft = 0;
  int16_t marginRight = 0;
  int16_t paddingTop = 0;     // treated same as margin for rendering
  int16_t paddingBottom = 0;  // treated same as margin for rendering
  int16_t paddingLeft = 0;    // treated same as margin for rendering
  int16_t paddingRight = 0;   // treated same as margin for rendering
  int16_t textIndent = 0;
  bool textIndentDefined = false;  // true if text-indent was explicitly set in CSS
  bool textAlignDefined = false;   // true if text-align was explicitly set in CSS
  float lineHeightMultiplier = 1.0f;

  // Per-block font override (0 = use page-level fontId)
  int fontId = 0;
  // Draw a full-width horizontal separator line below this block (used for h1/h2)
  bool drawSeparatorBelow = false;
  // True for <li> elements — reduces extraParagraphSpacing
  bool isListItem = false;

  // Combined horizontal insets (margin + padding)
  [[nodiscard]] int16_t leftInset() const { return marginLeft + paddingLeft; }
  [[nodiscard]] int16_t rightInset() const { return marginRight + paddingRight; }
  [[nodiscard]] int16_t totalHorizontalInset() const { return leftInset() + rightInset(); }

  // Combine with another block style. Useful for parent -> child styles, where the child style should be
  // applied on top of the parent's style to get the combined style.
  BlockStyle getCombinedBlockStyle(const BlockStyle& child) const {
    BlockStyle combinedBlockStyle;

    combinedBlockStyle.marginTop = static_cast<int16_t>(child.marginTop + marginTop);
    combinedBlockStyle.marginBottom = static_cast<int16_t>(child.marginBottom + marginBottom);
    combinedBlockStyle.marginLeft = static_cast<int16_t>(child.marginLeft + marginLeft);
    combinedBlockStyle.marginRight = static_cast<int16_t>(child.marginRight + marginRight);

    combinedBlockStyle.paddingTop = static_cast<int16_t>(child.paddingTop + paddingTop);
    combinedBlockStyle.paddingBottom = static_cast<int16_t>(child.paddingBottom + paddingBottom);
    combinedBlockStyle.paddingLeft = static_cast<int16_t>(child.paddingLeft + paddingLeft);
    combinedBlockStyle.paddingRight = static_cast<int16_t>(child.paddingRight + paddingRight);
    // Text indent: use child's if defined
    if (child.textIndentDefined) {
      combinedBlockStyle.textIndent = child.textIndent;
      combinedBlockStyle.textIndentDefined = true;
    } else {
      combinedBlockStyle.textIndent = textIndent;
      combinedBlockStyle.textIndentDefined = textIndentDefined;
    }
    // Text align: use child's if defined
    if (child.textAlignDefined) {
      combinedBlockStyle.alignment = child.alignment;
      combinedBlockStyle.textAlignDefined = true;
    } else {
      combinedBlockStyle.alignment = alignment;
      combinedBlockStyle.textAlignDefined = textAlignDefined;
    }
    // Font override: child takes precedence
    combinedBlockStyle.fontId = (child.fontId != 0) ? child.fontId : fontId;
    combinedBlockStyle.lineHeightMultiplier = child.lineHeightMultiplier;
    combinedBlockStyle.drawSeparatorBelow = child.drawSeparatorBelow || drawSeparatorBelow;
    combinedBlockStyle.isListItem = child.isListItem;

    return combinedBlockStyle;
  }

  // Create a BlockStyle from CSS style properties, resolving CssLength values to pixels
  // emSize is the current font line height, used for em/rem unit conversion
  // paragraphAlignment is the CrossPoint fallback when EPUB does not provide
  // an alignment. EPUB alignment is selected explicitly per book style.
  static BlockStyle fromCssStyle(const CssStyle& cssStyle, const float emSize, const CssTextAlign paragraphAlignment,
                                 const bool preferCssAlignment,
                                 const uint16_t viewportWidth = 0) {
    BlockStyle blockStyle;
    const float vw = viewportWidth;
    const auto maxHorizontalInsetPx = static_cast<int16_t>(emSize * MAX_HORIZONTAL_INSET_EM);
    const auto maxVerticalSpacingPx = static_cast<int16_t>(emSize * MAX_VERTICAL_BLOCK_SPACING_EM);
    // Resolve all CssLength values to pixels using the current font's em size and viewport width
    blockStyle.marginTop = std::min(cssStyle.marginTop.toPixelsInt16(emSize, vw), maxVerticalSpacingPx);
    blockStyle.marginBottom = std::min(cssStyle.marginBottom.toPixelsInt16(emSize, vw), maxVerticalSpacingPx);
    blockStyle.marginLeft = std::min(cssStyle.marginLeft.toPixelsInt16(emSize, vw), maxHorizontalInsetPx);
    blockStyle.marginRight = std::min(cssStyle.marginRight.toPixelsInt16(emSize, vw), maxHorizontalInsetPx);

    blockStyle.paddingTop = std::min(cssStyle.paddingTop.toPixelsInt16(emSize, vw), maxVerticalSpacingPx);
    blockStyle.paddingBottom = std::min(cssStyle.paddingBottom.toPixelsInt16(emSize, vw), maxVerticalSpacingPx);
    blockStyle.paddingLeft = std::min(cssStyle.paddingLeft.toPixelsInt16(emSize, vw), maxHorizontalInsetPx);
    blockStyle.paddingRight = std::min(cssStyle.paddingRight.toPixelsInt16(emSize, vw), maxHorizontalInsetPx);

    // For textIndent: if it's a percentage we can't resolve (no viewport width),
    // leave textIndentDefined=false so the EmSpace fallback in applyParagraphIndent() is used
    if (cssStyle.hasTextIndent() && cssStyle.textIndent.isResolvable(vw)) {
      blockStyle.textIndent = cssStyle.textIndent.toPixelsInt16(emSize, vw);
      blockStyle.textIndentDefined = true;
    }
    blockStyle.textAlignDefined = cssStyle.hasTextAlign();
    const CssTextAlign fallbackAlignment =
        paragraphAlignment == CssTextAlign::None ? CssTextAlign::Justify : paragraphAlignment;
    blockStyle.alignment = (preferCssAlignment && blockStyle.textAlignDefined) ? cssStyle.textAlign : fallbackAlignment;
    if (cssStyle.hasLineHeight()) {
      const float cssLineHeight = cssStyle.lineHeightIsMultiplier
                                      ? cssStyle.lineHeight
                                      : cssStyle.lineHeightLength.toPixels(emSize) / std::max(1.0f, emSize);
      blockStyle.lineHeightMultiplier = std::clamp(cssLineHeight, 0.8f, 2.0f);
    }
    return blockStyle;
  }
};
