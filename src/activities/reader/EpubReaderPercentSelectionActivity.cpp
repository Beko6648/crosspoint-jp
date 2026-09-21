#include "EpubReaderPercentSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "OrientationHelper.h"
#include "components/UITheme.h"
#include "components/UiLayout.h"
#include "fontIds.h"

namespace {
// Fine/coarse slider step sizes for percent adjustments.
constexpr int kSmallStep = 1;
constexpr int kLargeStep = 10;
}  // namespace

void EpubReaderPercentSelectionActivity::onEnter() {
  Activity::onEnter();
  renderer.setOrientation(readerOrientation);
  mappedInput.setEffectiveOrientation(OrientationHelper::toInputOrientation(readerOrientation));
  // Set up rendering task and mark first frame dirty.
  requestUpdate();
}

void EpubReaderPercentSelectionActivity::onExit() { Activity::onExit(); }

void EpubReaderPercentSelectionActivity::adjustPercent(const int delta) {
  // Apply delta and clamp within 0-100.
  percent += delta;
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  requestUpdate();
}

void EpubReaderPercentSelectionActivity::loop() {
  // Back cancels, confirm selects, arrows adjust the percent.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(PercentResult{percent});
    finish();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustPercent(-kSmallStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustPercent(kSmallStep); });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::ValueIncrease},
                                       [this] { adjustPercent(kLargeStep); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::ValueDecrease},
                                       [this] { adjustPercent(-kLargeStep); });
}

void EpubReaderPercentSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = UiLayout::from(renderer);
  const bool portraitInverted = renderer.getOrientation() == GfxRenderer::Orientation::PortraitInverted;
  const int topHintGutter = portraitInverted ? metrics.buttonHintsHeight + metrics.verticalSpacing : 0;
  const int centerOffset = layout.content.x + layout.content.width / 2 - renderer.getScreenWidth() / 2;

  // Title and numeric percent value.
  renderer.drawCenteredTextOffset(UI_12_FONT_ID, layout.content.y + 15 + topHintGutter, tr(STR_GO_TO_PERCENT), true,
                                  centerOffset, EpdFontFamily::BOLD);

  const std::string percentText = std::to_string(percent) + "%";
  renderer.drawCenteredTextOffset(UI_12_FONT_ID, layout.content.y + 90 + topHintGutter, percentText.c_str(), true,
                                  centerOffset, EpdFontFamily::BOLD);

  // Draw slider track.
  const int barWidth = std::min(360, layout.content.width - 40);
  constexpr int barHeight = 16;
  const int barX = layout.content.x + (layout.content.width - barWidth) / 2;
  const int barY = layout.content.y + 140 + topHintGutter;

  renderer.drawRect(barX, barY, barWidth, barHeight);

  // Fill slider based on percent.
  const int fillWidth = (barWidth - 4) * percent / 100;
  if (fillWidth > 0) {
    renderer.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4);
  }

  // Draw a simple knob centered at the current percent.
  const int knobX = barX + 2 + fillWidth - 2;
  renderer.fillRect(knobX, barY - 4, 4, barHeight + 8, true);

  // Hint text for step sizes.
  renderer.drawCenteredTextOffset(SMALL_FONT_ID, barY + 30, tr(STR_PERCENT_STEP_HINT), true, centerOffset);

  // Button hints follow the current front button layout.
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
