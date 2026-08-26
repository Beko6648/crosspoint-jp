#pragma once

#include "ReaderProfile.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class ReaderProfilesActivity final : public Activity {
 public:
  explicit ReaderProfilesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReaderProfiles", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
 void render(RenderLock&&) override;

 private:
  static constexpr int kProfileItemCount = ReaderProfile::SLOT_COUNT * 2;
  static constexpr int kResetItemIndex = kProfileItemCount;
  static constexpr int kItemCount = kProfileItemCount + 1;
  ButtonNavigator navigator;
  int selectedIndex = 0;
  std::string resultText;

  void selectCurrent();
};
