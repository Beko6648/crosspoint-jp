#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace yomuka {
namespace sync {

enum class ReadStatus : uint8_t { Present, Absent, Corrupt, IoError, StorageUnavailable, LimitExceeded };

// Restore an interrupted single-file replacement before reading. A complete
// final file wins over its backup; a temporary file is never published on read.
bool recoverFile(const std::string& path);
ReadStatus readBytes(const std::string& path, uint8_t* bytes, size_t capacity, size_t& length);
ReadStatus readJson(const std::string& path, JsonDocument& document, size_t maximumBytes = 262144);
bool writeBytes(const std::string& path, const uint8_t* bytes, size_t length);
bool writeJson(const std::string& path, const JsonDocument& document);
// Bounded-memory backup/restore with the same verified publication protocol.
bool copyStoredFile(const std::string& source, const std::string& destination, size_t maximumBytes = 262144);
uint32_t localUpdateTime();
bool readUpdateTime(JsonObjectConst object, uint32_t& timestamp);

}  // namespace sync
}  // namespace yomuka
