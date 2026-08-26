#pragma once

#include <cstdint>

namespace ReaderProfile {

constexpr uint8_t SLOT_COUNT = 3;

enum class LoadResult : uint8_t {
  Loaded,
  Missing,
  Invalid,
  BackupFailed,
  SaveFailed,
  LoadedWithMissingFont,
};

bool save(uint8_t slot);
LoadResult load(uint8_t slot);
// Restore reader-only settings. Device, network, navigation, reading state,
// caches, and installed SD fonts are intentionally left untouched.
bool resetToDefaults();

}  // namespace ReaderProfile
