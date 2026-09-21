#pragma once

#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class EpubReaderPercentSelectionActivity final : public Activity {
 public:
  // Slider-style percent selector for jumping within a book.
  explicit EpubReaderPercentSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                              const int initialPercent)
      : Activity("EpubReaderPercentSelection", renderer, mappedInput),
        percent(initialPercent),
        readerOrientation(renderer.getOrientation()) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool supportsLandscape() const override { return true; }

 private:
  // Current percent value (0-100) shown on the slider.
  int percent = 0;

  ButtonNavigator buttonNavigator;
  GfxRenderer::Orientation readerOrientation;

  // Change the current percent by a delta and clamp within bounds.
  void adjustPercent(int delta);
};
