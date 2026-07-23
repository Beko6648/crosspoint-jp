#pragma once

#include <GfxRenderer.h>
#include <HalGPIO.h>

#include "ScreenshotUtil.h"

// Cache generation is synchronous, so the normal main-loop shortcut handler
// is not reached while it runs.  Poll the raw input here to keep both
// cancellation and the global screenshot shortcut responsive.
class CacheGenerationControls {
 public:
  bool shouldCancel(GfxRenderer& renderer) {
    gpio.update();

    // Do not wait for InputManager's debounced state here: DOWN's ADC value is
    // immediately available to the cancellation path, while a just-pressed
    // POWER button may not have reached that state yet.
    const bool powerPressed = digitalRead(InputManager::POWER_BUTTON_PIN) == LOW;
    const bool downPressed = analogRead(InputManager::BUTTON_ADC_PIN_2) <= 1120;
    const bool screenshotPressed = powerPressed && downPressed;
    if (screenshotPressed) {
      if (!screenshotHeld) {
        screenshotHeld = true;
        ScreenshotUtil::takeScreenshot(renderer);
      }
      // DOWN is also a normal cancellation input.  The chord must win.
      return false;
    }

    screenshotHeld = false;
    constexpr int kAdcNoButton = 3800;
    return analogRead(1) < kAdcNoButton || analogRead(2) < kAdcNoButton;
  }

 private:
  bool screenshotHeld = false;
};
