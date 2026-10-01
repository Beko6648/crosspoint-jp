#pragma once
#include <ArduinoJson.h>

#include <string>
#include <vector>
namespace yomuka::sync {
// Inputs have passed snapshot validation. No storage access or mutation.
std::vector<std::string> previewUnit(unsigned unit, JsonVariantConst current, JsonVariantConst incoming,
                                     uint32_t spines, bool importing);
std::string previewDate(JsonVariantConst value);
}  // namespace yomuka::sync
