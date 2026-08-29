#pragma once

#include <cstdint>
#include <I18n.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class BookReaderSettingsActivity final : public Activity {
 public:
  enum class Item : uint8_t {
    TestView,
    Font,
    Size,
    Spacing,
    Margin,
    Ruby,
    WritingMode,
    BookStyle,
    SaveAll,
    ClearAll,
    Count,
  };

 private:
  uint64_t fingerprint;
  bool verticalMode;
  int selectedIndex = 0;
  const char* resultText = nullptr;
  bool skipNextButtonCheck = true;
  ButtonNavigator buttonNavigator;

  void selectCurrent();
  bool isOverridden(Item item) const;
  static StrId itemLabel(Item item);

 public:
  BookReaderSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint64_t fingerprint,
                             bool verticalMode)
      : Activity("BookReaderSettings", renderer, mappedInput), fingerprint(fingerprint), verticalMode(verticalMode) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool supportsLandscape() const override { return true; }
};
