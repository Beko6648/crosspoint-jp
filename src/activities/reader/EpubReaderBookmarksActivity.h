#pragma once

#include <Epub.h>

#include <vector>

#include "../../BookmarkEntry.h"
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class EpubReaderBookmarksActivity final : public Activity {
 public:
  EpubReaderBookmarksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::shared_ptr<Epub>& epub,
                              const std::string& epubPath);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool supportsLandscape() const override { return true; }

 private:
  static constexpr size_t MAX_BOOKMARKS = 24;
  std::shared_ptr<Epub> epub;
  std::string epubPath;
  std::vector<BookmarkEntry> bookmarks;
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  GfxRenderer::Orientation readerOrientation;
  enum class DeleteMode : uint8_t { NONE, ONE, ALL };
  DeleteMode deleteMode = DeleteMode::NONE;
  bool ignoreDeleteOpeningRelease = false;

  void save();
};
