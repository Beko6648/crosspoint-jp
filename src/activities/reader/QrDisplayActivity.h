#pragma once
#include <I18n.h>

#include <string>

#include "activities/Activity.h"

class QrDisplayActivity final : public Activity {
 public:
  explicit QrDisplayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& textPayload)
      : Activity("QrDisplay", renderer, mappedInput),
        textPayload(textPayload),
        readerOrientation(renderer.getOrientation()) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool supportsLandscape() const override { return true; }

 private:
  std::string textPayload;
  GfxRenderer::Orientation readerOrientation;
};
