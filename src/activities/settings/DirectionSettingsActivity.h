#pragma once
#include <I18n.h>

#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class DirectionSettingsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  bool isVertical;
  int selectedIndex = 0;
  bool skipNextButtonCheck = true;
  bool editingValue = false;

  struct Item {
    StrId nameId;
    enum class Type { TOGGLE, ENUM, PRESET, FONT_FAMILY } type;
    uint8_t DirectionSettings::* valuePtr = nullptr;
    std::vector<StrId> enumValues;
    // Optional persisted values for ENUM entries.  When empty, the displayed
    // index is also the stored value.
    std::vector<uint8_t> enumStorageValues;
    struct ValueRange {
      uint8_t min;
      uint8_t max;
      uint8_t step;
    };
    ValueRange valueRange = {};
    std::vector<uint8_t> presetValues;
  };

  std::vector<Item> items;
  void buildItems();
  bool currentItemIsEditable() const;
  void changeCurrentItem(int delta, bool activateAction = false, bool toggleValue = false);
  DirectionSettings& ds() { return SETTINGS.getDirectionSettings(isVertical); }
  const DirectionSettings& ds() const { return SETTINGS.getDirectionSettings(isVertical); }

 public:
  explicit DirectionSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool isVertical);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
