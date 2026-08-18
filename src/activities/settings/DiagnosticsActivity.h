#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/Activity.h"

class DiagnosticsActivity final : public Activity {
 public:
  explicit DiagnosticsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Diagnostics", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Page : uint8_t { Overview, Logs };
  enum class SaveResult : uint8_t { None, Saved, Failed };

  Page page = Page::Overview;
  SaveResult saveResult = SaveResult::None;
  bool sdReady = false;
  uint32_t freeHeap = 0;
  uint32_t maxAllocHeap = 0;
  uint32_t minFreeHeap = 0;
  int cacheDirectoryCount = 0;
  std::string recentLogs;
  std::vector<std::string> recentLogLines;
  std::string savedReportPath;

  void collectSnapshot();
  bool saveReport();
  void renderOverview(int x, int y, int contentWidth, int lineHeight);
  void renderLogs(int x, int y, int contentWidth, int lineHeight);
};
