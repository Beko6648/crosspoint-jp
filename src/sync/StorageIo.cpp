#include "StorageIo.h"

#include <HalStorage.h>

#include <ctime>

#include "SyncTimestamp.h"

namespace yomuka {
namespace sync {
namespace {
constexpr uint64_t kOffset = 14695981039346656037ULL;
constexpr uint64_t kPrime = 1099511628211ULL;
uint64_t hashBytes(uint64_t hash, const uint8_t* bytes, size_t size) {
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * kPrime;
  return hash;
}
bool removeIfExists(const std::string& path) { return !Storage.exists(path.c_str()) || Storage.remove(path.c_str()); }
bool verifyFile(const std::string& path, size_t size, uint64_t expectedHash) {
  HalFile file;
  if (!Storage.openFileForRead("SIO", path, file) || file.size() != size) return false;
  uint64_t hash = kOffset;
  uint8_t buffer[256];
  size_t remaining = size;
  while (remaining) {
    const size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    if (file.read(buffer, count) != static_cast<int>(count)) return false;
    hash = hashBytes(hash, buffer, count);
    remaining -= count;
  }
  return file.close() && hash == expectedHash;
}
bool publish(const std::string& path) {
  const std::string backup = path + ".bak";
  const bool hadOriginal = Storage.exists(path.c_str());
  if (hadOriginal && (!removeIfExists(backup) || !Storage.rename(path.c_str(), backup.c_str()))) return false;
  if (!Storage.rename((path + ".tmp").c_str(), path.c_str())) {
    if (hadOriginal) Storage.rename(backup.c_str(), path.c_str());
    return false;
  }
  // Keep the backup if cleanup fails. The new final file is already complete.
  if (hadOriginal) Storage.remove(backup.c_str());
  return true;
}
class JsonInput {
 public:
  HalFile& file;
  size_t expected;
  bool failed = false;
  int read() {
    const size_t position = file.position();
    const int result = file.read();
    if (result < 0 && position < expected) failed = true;
    return result;
  }
  size_t readBytes(char* buffer, size_t length) {
    size_t count = 0;
    while (count < length) {
      const int byte = read();
      if (byte < 0) break;
      buffer[count++] = static_cast<char>(byte);
    }
    return count;
  }
};
class JsonOutput {
 public:
  HalFile& file;
  uint64_t hash = kOffset;
  size_t count = 0;
  bool failed = false;
  size_t write(const uint8_t* bytes, size_t length) {
    const size_t written = file.write(bytes, length);
    hash = hashBytes(hash, bytes, written);
    count += written;
    if (written != length) failed = true;
    return written;
  }
  size_t write(uint8_t byte) { return write(&byte, 1); }
};
}  // namespace

bool recoverFile(const std::string& path) {
  HalStorage::StorageLock lock;
  if (!Storage.ready()) return false;
  const std::string backup = path + ".bak";
  return Storage.exists(path.c_str()) || !Storage.exists(backup.c_str()) ||
         Storage.rename(backup.c_str(), path.c_str());
}

ReadStatus readBytes(const std::string& path, uint8_t* bytes, size_t capacity, size_t& length) {
  HalStorage::StorageLock lock;
  if (!Storage.ready()) return ReadStatus::StorageUnavailable;
  if (!recoverFile(path)) return ReadStatus::IoError;
  if (!Storage.exists(path.c_str())) return ReadStatus::Absent;
  HalFile file;
  if (!Storage.openFileForRead("SIO", path, file)) return ReadStatus::IoError;
  const size_t size = file.size();
  if (size > capacity) return ReadStatus::LimitExceeded;
  if ((size && !bytes) || file.read(bytes, size) != static_cast<int>(size) || !file.close()) return ReadStatus::IoError;
  length = size;
  return ReadStatus::Present;
}

ReadStatus readJson(const std::string& path, JsonDocument& document, size_t maximumBytes) {
  HalStorage::StorageLock lock;
  if (!Storage.ready()) return ReadStatus::StorageUnavailable;
  if (!recoverFile(path)) return ReadStatus::IoError;
  if (!Storage.exists(path.c_str())) return ReadStatus::Absent;
  HalFile file;
  if (!Storage.openFileForRead("SIO", path, file)) return ReadStatus::IoError;
  const size_t size = file.size();
  if (size > maximumBytes) return ReadStatus::LimitExceeded;
  if (!size) return ReadStatus::Corrupt;
  JsonDocument candidate;
  JsonInput input{file, size};
  const auto error = deserializeJson(candidate, input);
  if (input.failed) return ReadStatus::IoError;
  if (error) return error == DeserializationError::NoMemory ? ReadStatus::LimitExceeded : ReadStatus::Corrupt;
  while (file.position() < size) {
    const int byte = input.read();
    if (input.failed) return ReadStatus::IoError;
    if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') return ReadStatus::Corrupt;
  }
  if (!file.close()) return ReadStatus::IoError;
  document = std::move(candidate);
  return ReadStatus::Present;
}

bool writeBytes(const std::string& path, const uint8_t* bytes, size_t length) {
  HalStorage::StorageLock lock;
  if (!Storage.ready() || (length && !bytes) || !recoverFile(path)) return false;
  const std::string temporary = path + ".tmp";
  if (!removeIfExists(temporary)) return false;
  HalFile file;
  if (!Storage.openFileForWrite("SIO", temporary, file) || file.write(bytes, length) != length) return false;
  file.flush();
  if (!file.close() || !verifyFile(temporary, length, hashBytes(kOffset, bytes, length))) return false;
  return publish(path);
}

bool writeJson(const std::string& path, const JsonDocument& document) {
  HalStorage::StorageLock lock;
  if (!Storage.ready() || document.overflowed() || !recoverFile(path)) return false;
  const std::string temporary = path + ".tmp";
  if (!removeIfExists(temporary)) return false;
  HalFile file;
  if (!Storage.openFileForWrite("SIO", temporary, file)) return false;
  JsonOutput output{file};
  const size_t expected = measureJson(document);
  serializeJson(document, output);
  file.flush();
  const bool closed = file.close();
  if (output.failed || output.count != expected || !closed || !verifyFile(temporary, expected, output.hash))
    return false;
  return publish(path);
}

uint32_t localUpdateTime() {
  const time_t now = time(nullptr);
  const bool known = now >= 1704067200;  // Same clock validity threshold as reading history.
  return timestampAfterLocalSave(0, LocalSaveOutcome::Saved, {known, known ? static_cast<uint64_t>(now) : 0});
}
bool copyStoredFile(const std::string& source, const std::string& destination, size_t maximumBytes) {
  HalStorage::StorageLock lock;
  if (source == destination || source == destination + ".tmp" || !Storage.ready() || !recoverFile(source) ||
      !recoverFile(destination))
    return false;
  HalFile input, output;
  if (!Storage.openFileForRead("SIO", source, input)) return false;
  const size_t length = input.size();
  if (length > maximumBytes || !removeIfExists(destination + ".tmp") ||
      !Storage.openFileForWrite("SIO", destination + ".tmp", output))
    return false;
  uint64_t hash = kOffset;
  uint8_t bytes[256];
  size_t remaining = length;
  while (remaining) {
    const size_t count = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
    if (input.read(bytes, count) != static_cast<int>(count) || output.write(bytes, count) != count) return false;
    hash = hashBytes(hash, bytes, count);
    remaining -= count;
  }
  output.flush();
  const bool inputClosed = input.close();
  const bool outputClosed = output.close();
  return inputClosed && outputClosed && verifyFile(destination + ".tmp", length, hash) && publish(destination);
}

bool readUpdateTime(JsonObjectConst object, uint32_t& timestamp) {
  const auto input = object["updatedAt"];
  if (input.isUnbound()) {
    timestamp = 0;
    return true;
  }
  if (!input.is<uint32_t>()) return false;
  timestamp = input.as<uint32_t>();
  return true;
}
}  // namespace sync
}  // namespace yomuka
