#include "BookReaderSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "BookReaderSettings.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "activities/settings/ReaderTestViewActivity.h"

namespace {

constexpr int kItemCount = static_cast<int>(BookReaderSettingsActivity::Item::Count);
constexpr uint16_t kFontFields = BookReaderSettings::DirectionFont;
constexpr uint16_t kSizeFields = BookReaderSettings::DirectionFontSize;
constexpr uint16_t kSpacingFields = BookReaderSettings::DirectionLineSpacing | BookReaderSettings::DirectionCharSpacing |
                                    BookReaderSettings::DirectionParagraphSpacing;
constexpr uint16_t kMarginFields = BookReaderSettings::DirectionMargin | BookReaderSettings::DirectionAlignment |
                                   BookReaderSettings::DirectionIndent;
constexpr uint16_t kRubyFields = BookReaderSettings::DirectionRubyEnabled | BookReaderSettings::DirectionRubyOffsetX |
                                 BookReaderSettings::DirectionRubyOffsetY;

}  // namespace

void BookReaderSettingsActivity::onEnter() {
  Activity::onEnter();
  skipNextButtonCheck = true;
  requestUpdate();
}

void BookReaderSettingsActivity::selectCurrent() {
  const Item item = static_cast<Item>(selectedIndex);
  if (item == Item::TestView) {
    startActivityForResult(std::make_unique<ReaderTestViewActivity>(renderer, mappedInput, fingerprint,
                                                                    verticalMode ? 1 : 0),
                           [this](const ActivityResult&) { requestUpdate(); });
    return;
  }
  BookReaderSettings::Override value;
  bool success = BookReaderSettings::load(fingerprint, value);
  if (success && item == Item::SaveAll) {
    value = BookReaderSettings::captureAll(SETTINGS);
  } else if (success && item == Item::ClearAll) {
    value = BookReaderSettings::Override{};
  } else if (success) {
    const auto current = BookReaderSettings::captureAll(SETTINGS);
    auto& direction = verticalMode ? value.vertical : value.horizontal;
    const auto& currentDirection = verticalMode ? current.vertical : current.horizontal;
    auto toggleDirection = [&current](BookReaderSettings::DirectionOverride& target,
                                      const BookReaderSettings::DirectionOverride& source, const uint16_t fields) {
      if ((target.fields & fields) != 0) {
        target.fields &= ~fields;
      } else {
        target.values = source.values;
        target.fields |= fields;
      }
    };
    switch (item) {
      case Item::TestView:
        break;
      case Item::Font: toggleDirection(direction, currentDirection, kFontFields); break;
      case Item::Size: toggleDirection(direction, currentDirection, kSizeFields); break;
      case Item::Spacing: toggleDirection(direction, currentDirection, kSpacingFields); break;
      case Item::Margin: toggleDirection(direction, currentDirection, kMarginFields); break;
      case Item::Ruby: toggleDirection(direction, currentDirection, kRubyFields); break;
      case Item::WritingMode:
        if (value.fields & BookReaderSettings::WritingMode)
          value.fields &= ~BookReaderSettings::WritingMode;
        else {
          value.writingMode = current.writingMode;
          value.fields |= BookReaderSettings::WritingMode;
        }
        break;
      case Item::BookStyle:
        if (value.fields & BookReaderSettings::BookStyle)
          value.fields &= ~BookReaderSettings::BookStyle;
        else {
          value.bookStyle = current.bookStyle;
          value.fields |= BookReaderSettings::BookStyle;
        }
        break;
      case Item::SaveAll:
      case Item::ClearAll:
      case Item::Count:
        break;
    }
  }
  if (success) success = BookReaderSettings::save(fingerprint, value);
  const StrId labelId = success ? (item == Item::ClearAll ? StrId::STR_BOOK_SETTINGS_CLEARED
                                                           : StrId::STR_BOOK_SETTINGS_SAVED)
                                : StrId::STR_BOOK_SETTINGS_FAILED;
  resultText = I18N.get(labelId);
  if (success) setResult(MenuResult{selectedIndex});
  requestUpdate();
}

bool BookReaderSettingsActivity::isOverridden(const Item item) const {
  BookReaderSettings::Override value;
  if (!BookReaderSettings::load(fingerprint, value)) return false;
  const auto hasDirection = [](const BookReaderSettings::DirectionOverride& direction, const uint16_t fields) {
    return (direction.fields & fields) != 0;
  };
  const auto& direction = verticalMode ? value.vertical : value.horizontal;
  switch (item) {
    case Item::TestView: return false;
    case Item::Font: return hasDirection(direction, kFontFields);
    case Item::Size: return hasDirection(direction, kSizeFields);
    case Item::Spacing: return hasDirection(direction, kSpacingFields);
    case Item::Margin: return hasDirection(direction, kMarginFields);
    case Item::Ruby: return hasDirection(direction, kRubyFields);
    case Item::WritingMode: return value.fields & BookReaderSettings::WritingMode;
    case Item::BookStyle: return value.fields & BookReaderSettings::BookStyle;
    case Item::SaveAll: return BookReaderSettings::hasAnyField(value);
    case Item::ClearAll:
    case Item::Count:
      return false;
  }
  return false;
}

StrId BookReaderSettingsActivity::itemLabel(const Item item) {
  static constexpr StrId kLabels[] = {
      StrId::STR_READER_TEST_VIEW, StrId::STR_FONT_FAMILY, StrId::STR_FONT_SIZE, StrId::STR_LINE_SPACING,
      StrId::STR_SCREEN_MARGIN,
      StrId::STR_BOOK_SETTINGS_RUBY,
      StrId::STR_BOOK_SETTINGS_WRITING_MODE, StrId::STR_BOOK_STYLE, StrId::STR_BOOK_SETTINGS_SAVE_CURRENT,
      StrId::STR_BOOK_SETTINGS_CLEAR,
  };
  return kLabels[static_cast<int>(item)];
}

void BookReaderSettingsActivity::loop() {
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
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kItemCount);
    requestUpdate();
  });
  buttonNavigator.onPress({MappedInputManager::Button::Left}, [this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kItemCount);
    requestUpdate();
  });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    selectCurrent();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void BookReaderSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int width = renderer.getScreenWidth();
  const int top = 15;
  renderer.drawCenteredText(UI_12_FONT_ID, top, tr(STR_BOOK_READER_SETTINGS), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 30, tr(STR_BOOK_SETTINGS_NOTE));
  for (int index = 0; index < kItemCount; ++index) {
    const int y = top + 58 + index * 34;
    const bool selected = index == selectedIndex;
    if (selected) renderer.fillRect(0, y, width - 1, 34, true);
    const Item item = static_cast<Item>(index);
    renderer.drawText(UI_10_FONT_ID, 20, y + 3, I18N.get(itemLabel(item)), !selected);
    const char* state = item == Item::TestView
                            ? tr(STR_BOOK_SETTINGS_PREVIEW)
                            : (isOverridden(item) ? tr(STR_BOOK_SETTINGS_THIS_BOOK) : tr(STR_BOOK_SETTINGS_GLOBAL));
    renderer.drawText(UI_10_FONT_ID, width - 95, y + 3, state, !selected);
  }
  if (resultText) renderer.drawCenteredText(UI_10_FONT_ID, top + 58 + kItemCount * 34, resultText);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_PREVIOUS), tr(STR_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
