#pragma once
#include <Epub.h>

#include "../Activity.h"
#include "sync/SnapshotExchange.h"
class BookSyncActivity final : public Activity {
  enum class Screen { Menu, Files, Units, Confirm, Message };
  Screen screen = Screen::Menu;
  std::shared_ptr<Epub> epub;
  yomuka::sync::ExchangeBook book{};
  JsonDocument snapshot;
  uint8_t selected = 15, available = 15;
  int cursor = 0;
  bool importing = false, blocked = false, ignoreRelease = true;
  std::vector<std::string> files;
  std::vector<std::string> lines;
  // 0: comparison, 1..4: unit heading, -1: warning.
  std::vector<int> lineKinds;
  bool importComplete = false;
  void finishImport();
  std::string message;
  void loadImport(const std::string& path);
  void makePreview();
  void execute();
  void fail(const std::string& text);
  static bool fontAvailable(const char*, const char*, void*);
  static bool project(const yomuka::sync::Progress&, const BookReaderSettings::Override*, yomuka::sync::Progress&,
                      void*);

 public:
  BookSyncActivity(GfxRenderer& r, MappedInputManager& i, std::shared_ptr<Epub> book)
      : Activity("BookSync", r, i), epub(std::move(book)) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }
};
