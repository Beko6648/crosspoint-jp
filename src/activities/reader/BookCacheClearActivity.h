#pragma once

#include <Epub.h>

#include <memory>

#include "../Activity.h"

class BookCacheClearActivity final : public Activity {
 public:
  BookCacheClearActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::shared_ptr<Epub> epub)
      : Activity("BookCacheClear", renderer, mappedInput),
        epub(std::move(epub)),
        readerOrientation(renderer.getOrientation()) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return true; }
  bool isReaderActivity() const override { return true; }
  bool supportsLandscape() const override { return true; }

 private:
  enum State { WARNING, CLEARING, SUCCESS, FAILED };

  std::shared_ptr<Epub> epub;
  State state = WARNING;
  GfxRenderer::Orientation readerOrientation;
};
