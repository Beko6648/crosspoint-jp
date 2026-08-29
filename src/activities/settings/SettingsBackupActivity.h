#pragma once

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class SettingsBackupActivity final : public Activity {
  ButtonNavigator navigator;
  int selectedIndex = 0;
  std::string resultText;

  bool saveBackup() const;
  bool restoreBackup() const;
  void selectCurrent();

 public:
  explicit SettingsBackupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SettingsBackup", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
