#pragma once
#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
namespace yomuka::sync {
enum class SnapshotError : uint8_t { None, Size, Encoding, Json, Schema, BookMismatch, SpineRange };
// No storage writes. Output is unchanged on failure. Font availability and
// layout projection remain explicit receiver responsibilities before apply.
SnapshotError parseSnapshot(const uint8_t* bytes, size_t length, JsonDocument& output);
// Revalidate an already parsed/internal document without duplicating it.
// Raw external input must still pass parseSnapshot (syntax/duplicate keys).
SnapshotError validateSnapshotDocument(const JsonDocument& document);
SnapshotError validateSnapshotTarget(const JsonDocument& snapshot, const char* bookId, uint32_t spineCount);
using FontAvailable = bool (*)(const char* family, const char* sdFamilyName, void* context);
// False holds the entire settings unit; never silently substitutes a font.
bool snapshotSettingsAvailable(const JsonDocument& snapshot, FontAvailable available, void* context);
}  // namespace yomuka::sync
