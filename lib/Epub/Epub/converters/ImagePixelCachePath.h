#pragma once

#include <FsHelpers.h>

#include <string>

// PNG tone revision only; the packed PXC6 representation is unchanged.
inline std::string getImagePixelCachePath(const std::string& imagePath) {
  const auto dot = imagePath.rfind('.');
  const auto stem = dot == std::string::npos ? imagePath : imagePath.substr(0, dot);
  return stem + (FsHelpers::hasPngExtension(imagePath) ? ".png-neutral.pxc6" : ".pxc6");
}
