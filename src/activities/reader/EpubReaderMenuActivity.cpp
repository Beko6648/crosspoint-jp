#include "EpubReaderMenuActivity.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

EpubReaderMenuActivity::EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const std::string& title, const int currentPage, const int totalPages,
                                               const int bookProgressPercent, const uint8_t currentOrientation,
                                               const bool hasFootnotes, const bool verticalMode)
    : Activity("EpubReaderMenu", renderer, mappedInput),
      menuItems(buildMenuItems(hasFootnotes)),
      title(title),
      pendingOrientation(currentOrientation),
      currentPage(currentPage),
      totalPages(totalPages),
      bookProgressPercent(bookProgressPercent),
      verticalMode(verticalMode) {}

std::vector<EpubReaderMenuActivity::MenuItem> EpubReaderMenuActivity::buildMenuItems(bool hasFootnotes) {
  std::vector<MenuItem> items;
  items.reserve(16);
  items.push_back({MenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER});
  if (hasFootnotes) {
    items.push_back({MenuAction::FOOTNOTES, StrId::STR_FOOTNOTES});
  }
  items.push_back({MenuAction::READER_SETTINGS, StrId::STR_SETTINGS_TITLE});
  items.push_back({MenuAction::STYLE_FIRST_LINE_INDENT, StrId::STR_FIRST_LINE_INDENT});
  items.push_back({MenuAction::STYLE_LINE_SPACING, StrId::STR_LINE_SPACING});
  items.push_back({MenuAction::STYLE_INVERT_IMAGES, StrId::STR_INVERT_IMAGES});
  items.push_back({MenuAction::STYLE_STATUS_BAR, StrId::STR_CUSTOMISE_STATUS_BAR});
  items.push_back({MenuAction::ROTATE_SCREEN, StrId::STR_ORIENTATION});
  items.push_back({MenuAction::RUBY_OFFSET, StrId::STR_RUBY_OFFSET});
  items.push_back({MenuAction::AUTO_PAGE_TURN, StrId::STR_AUTO_TURN_PAGES_PER_MIN});
  items.push_back({MenuAction::GO_TO_PERCENT, StrId::STR_GO_TO_PERCENT});
  items.push_back({MenuAction::SCREENSHOT, StrId::STR_SCREENSHOT_BUTTON});
  items.push_back({MenuAction::DISPLAY_QR, StrId::STR_DISPLAY_QR});
  items.push_back({MenuAction::GO_HOME, StrId::STR_GO_HOME_BUTTON});
  items.push_back({MenuAction::DELETE_CACHE, StrId::STR_DELETE_CACHE});
  if (gpio.deviceIsX3()) {
    items.push_back({MenuAction::TILT_PAGE_TURN, StrId::STR_TILT_PAGE_TURN});
  }
  return items;
}

void EpubReaderMenuActivity::onEnter() {
  Activity::onEnter();
  skipNextButtonCheck = true;
  requestUpdate();
}

void EpubReaderMenuActivity::onExit() { Activity::onExit(); }

void EpubReaderMenuActivity::loop() {
  if (skipNextButtonCheck) {
    const bool confirmCleared = !mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
                                !mappedInput.wasReleased(MappedInputManager::Button::Confirm);
    const bool backCleared = !mappedInput.isPressed(MappedInputManager::Button::Back) &&
                             !mappedInput.wasReleased(MappedInputManager::Button::Back);
    if (confirmCleared && backCleared) {
      skipNextButtonCheck = false;
    }
    return;
  }

  if (editingValue) {
    buttonNavigator.onPress({MappedInputManager::Button::Left}, [this] {
      if (changeCurrentValue(-1)) requestUpdate();
    });
    buttonNavigator.onPress({MappedInputManager::Button::Right}, [this] {
      if (changeCurrentValue(1)) requestUpdate();
    });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      editingValue = false;
      requestUpdate();
    }
    return;
  }

  // Handle navigation
  buttonNavigator.onPress({MappedInputManager::Button::Down}, [this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  buttonNavigator.onPress({MappedInputManager::Button::Up}, [this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto selectedAction = menuItems[selectedIndex].action;
    if (currentValueIsEditable()) {
      editingValue = true;
      requestUpdate();
      return;
    }
    if (selectedAction == MenuAction::ROTATE_SCREEN) {
      // Cycle orientation preview locally; actual rotation happens on menu exit.
      pendingOrientation = (pendingOrientation + 1) % orientationLabels.size();
      requestUpdate();
      return;
    }

    setResult(MenuResult{static_cast<int>(selectedAction), pendingOrientation, selectedPageTurnOption, layoutChanged});
    finish();
    return;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    result.data = MenuResult{-1, pendingOrientation, selectedPageTurnOption, layoutChanged};
    setResult(std::move(result));
    finish();
    return;
  }
}

void EpubReaderMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto orientation = renderer.getOrientation();
  // Landscape orientation: button hints are drawn along a vertical edge, so we
  // reserve a horizontal gutter to prevent overlap with menu content.
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  // Inverted portrait: button hints appear near the logical top, so we reserve
  // vertical space to keep the header and list clear.
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  constexpr int landscapeHintGutterWidth = 100;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? landscapeHintGutterWidth : 0;
  // Landscape CW places hints on the left edge; CCW keeps them on the right.
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int contentY = hintGutterHeight;

  // Title
  const std::string truncTitle =
      renderer.truncatedText(UI_12_FONT_ID, title.c_str(), contentWidth - 40, EpdFontFamily::BOLD);
  // Manual centering so we can respect the content gutter.
  const int titleX =
      contentX + (contentWidth - renderer.getTextWidth(UI_12_FONT_ID, truncTitle.c_str(), EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_12_FONT_ID, titleX, 15 + contentY, truncTitle.c_str(), true, EpdFontFamily::BOLD);

  // Progress summary
  std::string progressLine;
  if (totalPages > 0) {
    progressLine = std::string(tr(STR_CHAPTER_PREFIX)) + std::to_string(currentPage) + "/" +
                   std::to_string(totalPages) + std::string(tr(STR_PAGES_SEPARATOR));
  }
  progressLine += std::string(tr(STR_BOOK_PREFIX)) + std::to_string(bookProgressPercent) + "%";
  renderer.drawCenteredText(UI_10_FONT_ID, 45 + contentY, progressLine.c_str());

  // Menu Items
  const int startY = 75 + contentY;
  constexpr int lineHeight = 30;
  constexpr int valueRightMargin = 32;
  const int footerHeight = UITheme::getInstance().getMetrics().buttonHintsHeight;
  const int availableHeight = std::max(lineHeight, renderer.getScreenHeight() - startY - footerHeight - 5);
  const int visibleCount = std::max(1, availableHeight / lineHeight);
  const int maxFirstVisible = std::max(0, static_cast<int>(menuItems.size()) - visibleCount);
  const int firstVisible = std::clamp(selectedIndex - visibleCount + 1, 0, maxFirstVisible);
  const int lastVisible = std::min(static_cast<int>(menuItems.size()), firstVisible + visibleCount);

  for (int i = firstVisible; i < lastVisible; ++i) {
    const int displayY = startY + ((i - firstVisible) * lineHeight);
    const bool isSelected = i == selectedIndex;

    if (isSelected) {
      // Highlight only the content area so we don't paint over hint gutters.
      renderer.fillRect(contentX, displayY, contentWidth - 1, lineHeight, true);
    }

    renderer.drawText(UI_10_FONT_ID, contentX + 20, displayY, I18N.get(menuItems[i].labelId), !isSelected);

    const std::string value = getMenuItemValue(menuItems[i].action);
    if (!value.empty()) {
      const auto width = renderer.getTextWidth(UI_10_FONT_ID, value.c_str());
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - valueRightMargin - width, displayY, value.c_str(),
                        !isSelected);
    }

    if (menuItems[i].action == MenuAction::AUTO_PAGE_TURN) {
      // Render current page turn value on the right edge of the content area.
      const auto pageTurnValue = pageTurnLabels[selectedPageTurnOption];
      const auto pageTurnWidth = renderer.getTextWidth(UI_10_FONT_ID, pageTurnValue);
      renderer.drawText(UI_10_FONT_ID, contentX + contentWidth - valueRightMargin - pageTurnWidth, displayY,
                        pageTurnValue,
                        !isSelected);
    }
  }

  // Footer / Hints
  const auto selectedAction = menuItems[selectedIndex].action;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), editingValue ? tr(STR_SELECT)
                                                                        : (currentValueIsEditable() ? tr(STR_EDIT) : tr(STR_SELECT)),
                                            editingValue ? tr(STR_PREVIOUS) : "", editingValue ? tr(STR_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

std::string EpubReaderMenuActivity::getMenuItemValue(const MenuAction action) const {
  switch (action) {
    case MenuAction::ROTATE_SCREEN:
      return std::string(I18N.get(orientationLabels[pendingOrientation]));
    case MenuAction::STYLE_FIRST_LINE_INDENT:
      return SETTINGS.getDirectionSettings(verticalMode).firstLineIndent ? std::string(tr(STR_STATE_ON))
                                                                         : std::string(tr(STR_STATE_OFF));
    case MenuAction::STYLE_INVERT_IMAGES:
      return SETTINGS.invertImages ? std::string(tr(STR_STATE_ON)) : std::string(tr(STR_STATE_OFF));
    case MenuAction::STYLE_LINE_SPACING: {
      const uint8_t spacing = SETTINGS.getDirectionSettings(verticalMode).lineSpacing;
      constexpr uint8_t presets[] = {80, 120, 185, 220, 250};
      constexpr StrId labels[] = {StrId::STR_MINIMUM, StrId::STR_TIGHT, StrId::STR_STANDARD, StrId::STR_WIDE,
                                  StrId::STR_MAXIMUM};
      int closest = 0;
      for (int i = 1; i < 5; ++i) {
        if (std::abs(static_cast<int>(presets[i]) - spacing) < std::abs(static_cast<int>(presets[closest]) - spacing)) {
          closest = i;
        }
      }
      return std::string(I18N.get(labels[closest]));
    }
    case MenuAction::STYLE_STATUS_BAR:
      return "";
    case MenuAction::TILT_PAGE_TURN:
      return SETTINGS.tiltPageTurn ? std::string(tr(STR_STATE_ON)) : std::string(tr(STR_STATE_OFF));
    default:
      return "";
  }
}

bool EpubReaderMenuActivity::currentValueIsEditable() const {
  const auto action = menuItems[selectedIndex].action;
  return action == MenuAction::STYLE_FIRST_LINE_INDENT || action == MenuAction::STYLE_LINE_SPACING ||
         action == MenuAction::STYLE_INVERT_IMAGES || action == MenuAction::ROTATE_SCREEN ||
         action == MenuAction::AUTO_PAGE_TURN || action == MenuAction::TILT_PAGE_TURN;
}

bool EpubReaderMenuActivity::changeCurrentValue(const int delta, const bool toggleValue) {
  const auto action = menuItems[selectedIndex].action;
  switch (action) {
    case MenuAction::STYLE_FIRST_LINE_INDENT: {
      auto& value = SETTINGS.getDirectionSettings(verticalMode).firstLineIndent;
      value = toggleValue ? !value : (delta < 0 ? 0 : 1);
      SETTINGS.saveToFile();
      layoutChanged = true;
      return true;
    }
    case MenuAction::STYLE_LINE_SPACING: {
      auto& value = SETTINGS.getDirectionSettings(verticalMode).lineSpacing;
      constexpr uint8_t presets[] = {80, 120, 185, 220, 250};
      int closest = 0;
      for (int i = 1; i < 5; ++i) {
        if (std::abs(static_cast<int>(presets[i]) - value) < std::abs(static_cast<int>(presets[closest]) - value)) {
          closest = i;
        }
      }
      value = presets[std::clamp(closest + delta, 0, 4)];
      SETTINGS.saveToFile();
      layoutChanged = true;
      return true;
    }
    case MenuAction::STYLE_INVERT_IMAGES:
      SETTINGS.invertImages = toggleValue ? !SETTINGS.invertImages : (delta < 0 ? 0 : 1);
      SETTINGS.saveToFile();
      return true;
    case MenuAction::ROTATE_SCREEN:
      pendingOrientation = static_cast<uint8_t>(std::clamp(static_cast<int>(pendingOrientation) + delta, 0,
                                                            static_cast<int>(orientationLabels.size()) - 1));
      return true;
    case MenuAction::AUTO_PAGE_TURN:
      selectedPageTurnOption = static_cast<uint8_t>(std::clamp(static_cast<int>(selectedPageTurnOption) + delta, 0,
                                                                static_cast<int>(pageTurnLabels.size()) - 1));
      return true;
    case MenuAction::TILT_PAGE_TURN:
      SETTINGS.tiltPageTurn = toggleValue ? !SETTINGS.tiltPageTurn : (delta < 0 ? 0 : 1);
      SETTINGS.saveToFile();
      return true;
    default:
      return false;
  }
}
