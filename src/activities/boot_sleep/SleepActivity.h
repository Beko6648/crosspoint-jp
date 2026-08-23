#pragma once
#include "../Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sleep", renderer, mappedInput) {}
  void onEnter() override;

 private:
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  bool renderTransparentSleepOverlay() const;
  bool renderTransparentSleepOverlayFile(const char* path) const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap, bool preserveBackground = false) const;
  void renderBlankSleepScreen() const;
  void renderCalendarOverlay() const;
  void drawCalendarIfPending() const;
  static bool isTimeValid();

  // カレンダーをBW描画パスに挿入するためのフラグ
  mutable bool calendarPending = false;
};
