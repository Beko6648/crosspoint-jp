#include "EpubReaderDetailsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

EpubReaderDetailsActivity::EpubReaderDetailsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("EpubReaderDetails", renderer, mappedInput) {}

void EpubReaderDetailsActivity::onEnter() {
  Activity::onEnter();
  skipNextButtonCheck = true;
  requestUpdate();
}

void EpubReaderDetailsActivity::onExit() { Activity::onExit(); }

void EpubReaderDetailsActivity::loop() {
  if (skipNextButtonCheck) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
        !mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
        !mappedInput.isPressed(MappedInputManager::Button::Back) &&
        !mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      skipNextButtonCheck = false;
    }
    return;
  }

  buttonNavigator.onPress({MappedInputManager::Button::Right}, [this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(items.size()));
    requestUpdate();
  });
  buttonNavigator.onPress({MappedInputManager::Button::Left}, [this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(items.size()));
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(MenuResult{static_cast<int>(items[selectedIndex].action)});
    finish();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
  }
}

void EpubReaderDetailsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto orientation = renderer.getOrientation();
  const bool landscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool landscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  const int hintGutterWidth = (landscapeCw || landscapeCcw) ? 100 : 0;
  const int contentX = landscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int contentY = orientation == GfxRenderer::Orientation::PortraitInverted ? 50 : 0;

  renderer.drawCenteredText(UI_12_FONT_ID, 15 + contentY, tr(STR_DETAILED_SETTINGS), true, EpdFontFamily::BOLD);

  constexpr int startY = 65;
  constexpr int lineHeight = 34;
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    const int y = startY + contentY + i * lineHeight;
    const bool selected = i == selectedIndex;
    if (selected) renderer.fillRect(contentX, y, contentWidth - 1, lineHeight, true);
    renderer.drawText(UI_10_FONT_ID, contentX + 20, y, I18N.get(items[i].labelId), !selected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_PREVIOUS), tr(STR_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
