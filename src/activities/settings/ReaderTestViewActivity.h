#pragma once

#include "activities/Activity.h"

class ReaderTestViewActivity final : public Activity {
  bool vertical = false;
  bool rubyAdjustActive = false;
  bool rubyAdjustChanged = false;
  bool ignoreOpeningConfirmRelease = true;

  void adjustRubyOffset(bool xAxis, int delta);

 public:
  explicit ReaderTestViewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReaderTestView", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
