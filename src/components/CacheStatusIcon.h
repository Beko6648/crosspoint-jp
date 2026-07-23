#pragma once

#include <Epub.h>
#include <GfxRenderer.h>

namespace CacheStatusIcon {

inline void drawCircle(GfxRenderer& renderer, const int radius, const int centerX, const int centerY, const bool ink) {
  renderer.drawArc(radius, centerX, centerY, -1, -1, 1, ink);
  renderer.drawArc(radius, centerX, centerY, 1, -1, 1, ink);
  renderer.drawArc(radius, centerX, centerY, -1, 1, 1, ink);
  renderer.drawArc(radius, centerX, centerY, 1, 1, 1, ink);
}

inline void draw(GfxRenderer& renderer, const Epub::CacheGenerationStatus status, const int radius, const int centerX,
                 const int centerY, const bool ink = true) {
  drawCircle(renderer, radius, centerX, centerY, ink);
  switch (status) {
    case Epub::CacheGenerationStatus::NotGenerated:
      return;
    case Epub::CacheGenerationStatus::Resumable:
      renderer.fillRect(centerX - radius + 2, centerY, radius * 2 - 3, radius - 1, ink);
      return;
    case Epub::CacheGenerationStatus::Complete:
      renderer.drawLine(centerX - radius + 3, centerY, centerX - 1, centerY + radius / 2, 2, ink);
      renderer.drawLine(centerX - 1, centerY + radius / 2, centerX + radius - 2, centerY - radius + 3, 2, ink);
      return;
  }
}

}  // namespace CacheStatusIcon
