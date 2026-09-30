#include "FontSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <SdCardFont.h>
#include <Utf8.h>

#include <algorithm>
#include <memory>
#include <new>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/UiLayout.h"
#include "fontIds.h"

FontSelectionActivity::FontSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             SdCardFontRegistry* registry, bool isVertical)
    : Activity("FontSelect", renderer, mappedInput), registry_(registry), isVertical_(isVertical) {}

void FontSelectionActivity::onEnter() {
  Activity::onEnter();

  // A ZIP copied to the SD card while the reader was powered on is not visible
  // in the boot-time registry until it is re-scanned.  This is metadata-only
  // discovery; font files are still opened lazily when selected.
  if (registry_) registry_->discover();

  // Build combined font list: built-in + SD card fonts
  fonts_.clear();
  fonts_.reserve(CrossPointSettings::BUILTIN_FONT_COUNT + (registry_ ? registry_->getFamilyCount() : 0));

  fonts_.push_back({I18N.get(StrId::STR_NOTO_SANS), true, CrossPointSettings::NOTOSANS});

  if (registry_) {
    const auto& families = registry_->getFamilies();
    for (int i = 0; i < static_cast<int>(families.size()); i++) {
      fonts_.push_back({families[i].name, false, static_cast<uint8_t>(CrossPointSettings::BUILTIN_FONT_COUNT + i)});
    }
  }

  // Find current selection
  selectedIndex_ = 0;
  if (SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName[0] != '\0' && registry_) {
    const auto& families = registry_->getFamilies();
    for (int i = 0; i < static_cast<int>(families.size()); i++) {
      if (families[i].name == SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName) {
        selectedIndex_ = CrossPointSettings::BUILTIN_FONT_COUNT + i;
        break;
      }
    }
  } else {
    selectedIndex_ = 0;
  }

  requestUpdate();
}

void FontSelectionActivity::onExit() { Activity::onExit(); }

void FontSelectionActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  buttonNavigator_.onNextRelease([this] {
    RenderLock lock(*this);
    selectedIndex_ = ButtonNavigator::nextIndex(selectedIndex_, static_cast<int>(fonts_.size()));
    requestUpdate();
  });

  buttonNavigator_.onPreviousRelease([this] {
    RenderLock lock(*this);
    selectedIndex_ = ButtonNavigator::previousIndex(selectedIndex_, static_cast<int>(fonts_.size()));
    requestUpdate();
  });
}

void FontSelectionActivity::handleSelection() {
  RenderLock lock(*this);
  const auto& font = fonts_[selectedIndex_];
  if (font.isBuiltin) {
    SETTINGS.getDirectionSettings(isVertical_).fontFamily = font.settingIndex;
    SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName[0] = '\0';
  } else if (registry_) {
    int sdIdx = font.settingIndex - CrossPointSettings::BUILTIN_FONT_COUNT;
    const auto& families = registry_->getFamilies();
    if (sdIdx < static_cast<int>(families.size())) {
      strncpy(SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName, families[sdIdx].name.c_str(),
              sizeof(SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName) - 1);
      SETTINGS.getDirectionSettings(isVertical_)
          .sdFontFamilyName[sizeof(SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName) - 1] = '\0';
    }
  }
  finish();
}

void FontSelectionActivity::drawSpecimen(const int x, const int y, const int width, const int height) {
  // render() holds RenderLock. Keep the candidate independent of the reader's
  // resident fonts and settings; remove every temporary registration before
  // destroying its glyph storage, including on failure.
  struct SpecimenFont {
    GfxRenderer& renderer;
    std::unique_ptr<SdCardFont> sd;
    int id = 0;
    bool registered = false;
    ~SpecimenFont() {
      if (registered) {
        renderer.removeFont(id);
        renderer.unregisterSdCardFont(id);
        renderer.unregisterSdCardFontScale(id);
      }
    }
  } candidate{renderer, nullptr};
  constexpr int pointSize = 14;
  const bool japanese = I18N.getLanguage() == Language::JAPANESE;
  const auto& entry = fonts_[selectedIndex_];
  const auto name = renderer.truncatedText(UI_10_FONT_ID, entry.name.c_str(), width);
  renderer.drawText(UI_10_FONT_ID, x, y, name.c_str());
  const int labelHeight = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, x, y + labelHeight,
                    japanese ? "字体見本 14pt（横書き）" : "Sample 14pt (horizontal)");

  const EpdFontFamily* family = nullptr;
  if (entry.isBuiltin) {
    const auto found = renderer.getFontMap().find(NOTOSANS_14_FONT_ID);
    if (found != renderer.getFontMap().end()) family = &found->second;
  } else if (registry_) {
    const auto* info = registry_->findFamily(entry.name);
    const SdCardFontFileInfo* file = nullptr;
    if (info) {
      for (const auto& option : info->files) {
        if (option.style != 0 || option.pointSize == 0) continue;
        const int distance = std::abs(static_cast<int>(option.pointSize) - pointSize);
        if (!file || distance < std::abs(static_cast<int>(file->pointSize) - pointSize) ||
            (distance == std::abs(static_cast<int>(file->pointSize) - pointSize) &&
             option.pointSize > file->pointSize)) {
          file = &option;
        }
      }
    }
    candidate.sd.reset(new (std::nothrow) SdCardFont());
    if (file && candidate.sd && candidate.sd->load(file->path.c_str()) && candidate.sd->getEpdFont()) {
      // A private positive ID avoids both reader fallback and generated IDs.
      candidate.id = 1;
      while (renderer.getFontMap().count(candidate.id) || renderer.getSdCardFonts().count(candidate.id)) ++candidate.id;
      renderer.insertFont(candidate.id, EpdFontFamily(candidate.sd->getEpdFont()));
      renderer.registerSdCardFont(candidate.id, candidate.sd.get());
      renderer.registerSdCardFontScale(candidate.id, pointSize * 256 / file->pointSize);
      candidate.registered = true;
      family = &renderer.getFontMap().at(candidate.id);
    }
  }
  if (entry.isBuiltin && family) {
    candidate.id = 1;
    while (renderer.getFontMap().count(candidate.id) || renderer.getSdCardFonts().count(candidate.id)) ++candidate.id;
    renderer.insertFont(candidate.id, *family);
    candidate.registered = true;
    family = &renderer.getFontMap().at(candidate.id);
  }
  const int sampleTop = y + 2 * labelHeight + 8;
  if (!family) {
    renderer.drawText(UI_10_FONT_ID, x, sampleTop, japanese ? "見本を読み込めません" : "Sample unavailable");
    return;
  }
  constexpr const char* samples[] = {"Aa Bb Gg Qq Rr 0123456789", "The quick brown fox jumps.",
                                     "山の向こうに、青い空。", "「ゆっくり読もう。」カフェで一冊。"};
  const int lineHeight = std::max(1, renderer.getLineHeight(candidate.id)) + 4;
  const int bottom = y + height - labelHeight - 4;
  int lineY = sampleTop;
  bool missing = false;
  bool clipped = false;
  for (const auto* sample : samples) {
    int cursorX = x;
    const char* cursor = sample;
    while (*cursor) {
      const auto cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&cursor));
      if (!cp) break;
      // Exact lookup prevents both replacement glyphs and Japanese fallback.
      const bool covered = candidate.sd ? candidate.sd->hasCodepoint(cp) : family->getGlyphExact(cp) != nullptr;
      const auto* glyph = covered ? family->getGlyphExact(cp) : nullptr;
      if (!glyph && cp != ' ') missing = true;
      const int advance =
          glyph ? std::max(1, (fp4::toPixel(glyph->advanceX) * renderer.getSdCardFontScale(candidate.id) + 128) / 256)
                : (cp < 128 ? 14 : 28);
      if (cursorX + advance > x + width) {
        cursorX = x;
        lineY += lineHeight;
      }
      if (lineY + lineHeight > bottom) {
        clipped = true;
        continue;
      }
      if (glyph) {
        const int scale = renderer.getSdCardFontScale(candidate.id);
        const int minX = (glyph->left * scale + 128) >> 8;
        const int maxX = minX + ((glyph->width * scale + 128) >> 8);
        if (cursorX + minX >= x && cursorX + maxX <= x + width) {
          renderer.drawGlyphExact(candidate.id, cursorX, lineY, cp);
        }
      }
      cursorX += advance;
    }
    lineY += lineHeight;
  }
  const char* caption = missing ? (japanese ? "未収録・読込不可の文字は空白" : "Unavailable glyphs are blank")
                                : (clipped ? (japanese ? "見本の一部を表示" : "Sample shortened") : "");
  const auto captionText = renderer.truncatedText(UI_10_FONT_ID, caption, width);
  renderer.drawText(UI_10_FONT_ID, x, y + height - labelHeight, captionText.c_str());
}

void FontSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = UiLayout::from(renderer);
  const bool portraitInverted = renderer.getOrientation() == GfxRenderer::Orientation::PortraitInverted;
  const int topHintGutter = portraitInverted ? metrics.buttonHintsHeight + metrics.verticalSpacing : 0;

  GUI.drawHeader(renderer,
                 Rect{layout.content.x, layout.content.y + metrics.topPadding + topHintGutter, layout.content.width,
                      metrics.headerHeight},
                 tr(STR_FONT_FAMILY));

  const int contentTop =
      layout.content.y + metrics.topPadding + topHintGutter + metrics.headerHeight + metrics.verticalSpacing;
  const int bottomHints = layout.landscape ? 0 : metrics.buttonHintsHeight + metrics.verticalSpacing;
  const int contentHeight = layout.content.y + layout.content.height - contentTop - bottomHints;

  // Determine which font index is currently active (to mark as "Selected")
  int currentFontIndex = 0;
  if (SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName[0] != '\0' && registry_) {
    const auto& families = registry_->getFamilies();
    for (int i = 0; i < static_cast<int>(families.size()); i++) {
      if (families[i].name == SETTINGS.getDirectionSettings(isVertical_).sdFontFamilyName) {
        currentFontIndex = CrossPointSettings::BUILTIN_FONT_COUNT + i;
        break;
      }
    }
  } else {
    currentFontIndex = 0;
  }

  const int gap = metrics.verticalSpacing;
  const int previewWidth = layout.landscape ? layout.content.width * 56 / 100 : layout.content.width;
  const int previewHeight = layout.landscape ? contentHeight : std::min(350, contentHeight - 144);
  drawSpecimen(layout.content.x + metrics.contentSidePadding, contentTop, previewWidth - 2 * metrics.contentSidePadding,
               previewHeight - gap);
  const Rect listRect = layout.landscape ? Rect{layout.content.x + previewWidth + gap, contentTop,
                                                layout.content.width - previewWidth - gap, contentHeight}
                                         : Rect{layout.content.x, contentTop + previewHeight + gap,
                                                layout.content.width, contentHeight - previewHeight - gap};
  if (layout.landscape) {
    renderer.drawLine(layout.content.x + previewWidth, contentTop, layout.content.x + previewWidth,
                      contentTop + contentHeight - 1);
  } else {
    renderer.drawLine(layout.content.x, contentTop + previewHeight, layout.content.x + layout.content.width - 1,
                      contentTop + previewHeight);
  }
  GUI.drawList(
      renderer, listRect, static_cast<int>(fonts_.size()), selectedIndex_,
      [this](int index) { return fonts_[index].name; }, nullptr, nullptr,
      [this, currentFontIndex](int index) -> std::string { return index == currentFontIndex ? tr(STR_SELECTED) : ""; },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
