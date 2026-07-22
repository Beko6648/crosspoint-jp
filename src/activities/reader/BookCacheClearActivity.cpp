#include "BookCacheClearActivity.h"

#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BookCacheClearActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void BookCacheClearActivity::onExit() { Activity::onExit(); }

void BookCacheClearActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DELETE_CACHE));

  if (state == WARNING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 60, tr(STR_DELETE_BOOK_CACHE_WARNING_1), true,
                              EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_DELETE_BOOK_CACHE_WARNING_2));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_DELETE_BOOK_CACHE_WARNING_3));
    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_DELETE), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == CLEARING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_CLEARING_CACHE));
  } else if (state == SUCCESS) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_CACHE_CLEARED), true, EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_CLEAR_CACHE_FAILED), true, EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}

void BookCacheClearActivity::loop() {
  if (state == WARNING) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      state = CLEARING;
      requestUpdateAndWait();
      state = epub && epub->clearCache() ? SUCCESS : FAILED;
      requestUpdate();
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      ActivityResult result;
      result.isCancelled = true;
      setResult(std::move(result));
      finish();
    }
    return;
  }

  if ((state == SUCCESS || state == FAILED) && mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = state != SUCCESS;
    setResult(std::move(result));
    finish();
  }
}
