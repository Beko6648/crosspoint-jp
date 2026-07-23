#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

class GenerateAllCacheActivity final : public Activity {
 public:
  explicit GenerateAllCacheActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GenerateAllCache", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum State { CONFIRMING, GENERATING, SUCCESS, INTERRUPTED, FAILED };

  State state = CONFIRMING;
  int totalCount = 0;
  int completeCount = 0;
  int resumableCount = 0;
  int notGeneratedCount = 0;

  void goBack() { finish(); }
  void generateAllCaches();
  void summarizeCacheStatuses(const std::vector<std::string>& epubFiles);
  std::string cacheGenerationResultText() const;
};
