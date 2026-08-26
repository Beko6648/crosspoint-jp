#pragma once

#include <I18n.h>

#include <vector>

#include "../Activity.h"
#include "EpubReaderMenuActivity.h"
#include "util/ButtonNavigator.h"

class EpubReaderDetailsActivity final : public Activity {
 public:
  explicit EpubReaderDetailsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool supportsLandscape() const override { return true; }

 private:
  struct Item {
    EpubReaderMenuActivity::MenuAction action;
    StrId labelId;
  };

  const std::vector<Item> items = {
      {EpubReaderMenuActivity::MenuAction::OPEN_GLOBAL_READER_SETTINGS, StrId::STR_READER_SETTINGS},
      {EpubReaderMenuActivity::MenuAction::STYLE_STATUS_BAR, StrId::STR_CUSTOMISE_STATUS_BAR},
  };
  int selectedIndex = 0;
  bool skipNextButtonCheck = true;
  ButtonNavigator buttonNavigator;
};
