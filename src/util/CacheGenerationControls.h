#pragma once

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "ScreenshotUtil.h"

// Cache generation is synchronous, so the normal main-loop shortcut handler
// is not reached while it runs. Poll mapped Back for cancellation and
// raw inputs for the global screenshot shortcut.
class CacheGenerationControls {
 public:
  explicit CacheGenerationControls(MappedInputManager& input) : input(input) {}

  // Consume the cancellation hold and its release before normal navigation resumes.
  static bool consumeCancellationRelease(MappedInputManager& input) {
    if (!releasePending) return false;
    if (!input.isPressed(MappedInputManager::Button::Back)) releasePending = false;
    return true;
  }

  bool shouldCancel(GfxRenderer& renderer) {
    if (cancelled) return true;
    gpio.update();

#ifndef SIMULATOR
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
      holding = false;
      armed = false;
      return false;
    }

    screenshotHeld = false;
#endif
    const bool back = input.isPressed(MappedInputManager::Button::Back);
    if (!back) {
      armed = true;
      holding = false;
      return false;
    }
    if (!armed) return false;
    if (!holding) {
      holding = true;
      holdStarted = millis();
      return false;
    }
    if (static_cast<uint32_t>(millis() - holdStarted) < 1000) return false;
    cancelled = true;
    releasePending = true;
    LOG_INF("CACHE", "Cancellation accepted: Back held");
    return true;
  }

 private:
  MappedInputManager& input;
  inline static bool releasePending = false;
  bool armed = false;
  bool holding = false;
  bool cancelled = false;
  uint32_t holdStarted = 0;
  bool screenshotHeld = false;
};
