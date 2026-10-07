#include "SnapshotValidation.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "SyncProgress.h"

namespace yomuka::sync {
namespace {
bool validUtf8(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n;) {
    uint32_t c = p[i++];
    if (c < 128) {
      if (!c) return false;
      continue;
    }
    unsigned count;
    uint32_t minimum;
    if (c >= 0xc2 && c <= 0xdf) {
      count = 1;
      c &= 31;
      minimum = 128;
    } else if (c >= 0xe0 && c <= 0xef) {
      count = 2;
      c &= 15;
      minimum = 2048;
    } else if (c >= 0xf0 && c <= 0xf4) {
      count = 3;
      c &= 7;
      minimum = 65536;
    } else
      return false;
    if (count > n - i) return false;
    while (count--) {
      const uint8_t b = p[i++];
      if ((b & 0xc0) != 0x80) return false;
      c = (c << 6) | (b & 63);
    }
    if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
  }
  return true;
}
size_t unicodeLength(JsonString s) {
  size_t count = 0;
  for (size_t i = 0; i < s.size(); ++i)
    if ((static_cast<uint8_t>(s.c_str()[i]) & 0xc0) != 0x80) ++count;
  return count;
}
bool validBookId(JsonString s) {
  if (s.size() != 16) return false;
  for (size_t i = 0; i < 16; ++i)
    if (!((s.c_str()[i] >= '0' && s.c_str()[i] <= '9') || (s.c_str()[i] >= 'a' && s.c_str()[i] <= 'f'))) return false;
  return true;
}
// ArduinoJson intentionally accepts extensions and overwrites duplicate keys.
// First scan strict JSON syntax and compare decoded keys in each object.
class Scanner {
  const uint8_t* p;
  size_t n, i = 0;
  void ws() {
    while (i < n && (p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n')) ++i;
  }
  bool take(char c) {
    ws();
    if (i == n || p[i] != c) return false;
    ++i;
    return true;
  }
  bool hex(uint32_t& value) {
    value = 0;
    for (unsigned k = 0; k < 4; ++k) {
      if (i == n) return false;
      const char c = p[i++];
      unsigned d;
      if (c >= '0' && c <= '9')
        d = c - '0';
      else if (c >= 'a' && c <= 'f')
        d = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        d = c - 'A' + 10;
      else
        return false;
      value = value * 16 + d;
    }
    return true;
  }
  bool string(std::string* key = nullptr) {
    ws();
    const size_t start = i;
    if (!take('"')) return false;
    bool closed = false;
    while (i < n) {
      const uint8_t c = p[i++];
      if (c == '"') {
        closed = true;
        break;
      }
      if (c < 32) return false;
      if (c != '\\') continue;
      if (i == n) return false;
      const char escape = p[i++];
      if (escape == 'u') {
        uint32_t code;
        if (!hex(code) || code == 0 || (code >= 0xdc00 && code <= 0xdfff)) return false;
        if (code >= 0xd800 && code <= 0xdbff) {
          if (n - i < 2 || p[i++] != '\\' || p[i++] != 'u') return false;
          if (!hex(code) || code < 0xdc00 || code > 0xdfff) return false;
        }
      } else if (!std::strchr("\"\\/bfnrt", escape))
        return false;
    }
    if (!closed) return false;
    if (key) {
      JsonDocument decoded;
      if (deserializeJson(decoded, p + start, i - start) || !decoded.is<const char*>()) return false;
      *key = decoded.as<std::string>();
      if (key->size() > 64) return false;
    }
    return true;
  }
  bool number() {
    const size_t start = i;
    if (i < n && p[i] == '-') ++i;
    if (i == n) return false;
    if (p[i] == '0')
      ++i;
    else {
      if (p[i] < '1' || p[i] > '9') return false;
      while (i < n && p[i] >= '0' && p[i] <= '9') ++i;
    }
    if (i < n && p[i] == '.') {
      ++i;
      const size_t first = i;
      while (i < n && p[i] >= '0' && p[i] <= '9') ++i;
      if (i == first) return false;
    }
    if (i < n && (p[i] == 'e' || p[i] == 'E')) {
      ++i;
      if (i < n && (p[i] == '+' || p[i] == '-')) ++i;
      const size_t first = i;
      while (i < n && p[i] >= '0' && p[i] <= '9') ++i;
      if (i == first) return false;
    }
    return i > start;
  }
  bool value(unsigned depth) {
    ws();
    if (i == n || depth > 12) return false;
    if (p[i] == '"') return string();
    if (p[i] == '{') {
      ++i;
      ws();
      if (i < n && p[i] == '}') {
        ++i;
        return true;
      }
      std::vector<std::string> keys;
      do {
        std::string key;
        if (!string(&key) || keys.size() >= 32) return false;
        for (const auto& old : keys)
          if (old == key) return false;
        keys.push_back(std::move(key));
        if (!take(':') || !value(depth + 1)) return false;
        ws();
        if (i < n && p[i] == '}') {
          ++i;
          return true;
        }
      } while (take(','));
      return false;
    }
    if (p[i] == '[') {
      ++i;
      ws();
      if (i < n && p[i] == ']') {
        ++i;
        return true;
      }
      do {
        if (!value(depth + 1)) return false;
        ws();
        if (i < n && p[i] == ']') {
          ++i;
          return true;
        }
      } while (take(','));
      return false;
    }
    for (const char* literal : {"true", "false", "null"}) {
      const size_t length = std::strlen(literal);
      if (length <= n - i && std::memcmp(p + i, literal, length) == 0) {
        i += length;
        return true;
      }
    }
    return number();
  }

 public:
  Scanner(const uint8_t* bytes, size_t length) : p(bytes), n(length) {}
  bool scan() {
    if (!value(0)) return false;
    ws();
    return i == n;
  }
};
#include "SnapshotSchema.generated.h"
Progress progress(JsonVariantConst p) {
  Progress result;
  result.spineIndex = p["spineIndex"].as<uint16_t>();
  result.chapterPage = p["chapterPage"].as<uint16_t>();
  if (!p["chapterPageCount"].isNull()) result.chapterPageCount = p["chapterPageCount"].as<uint16_t>();
  if (!p["finished"].isNull()) result.finished = p["finished"].as<bool>();
  if (!p["percent"].isNull()) result.percent = p["percent"].as<uint8_t>();
  return result;
}
}  // namespace
SnapshotError parseSnapshot(const uint8_t* bytes, size_t length, JsonDocument& output) {
  if (!bytes || !length || length > 65536) return SnapshotError::Size;
  if ((length >= 3 && std::memcmp(bytes, "\xef\xbb\xbf", 3) == 0) || !validUtf8(bytes, length))
    return SnapshotError::Encoding;
  if (!Scanner(bytes, length).scan()) return SnapshotError::Json;
  JsonDocument candidate;
  if (deserializeJson(candidate, bytes, length, DeserializationOption::NestingLimit(12))) return SnapshotError::Json;
  const auto status = validateSnapshotDocument(candidate);
  if (status != SnapshotError::None) return status;
  output = std::move(candidate);
  return SnapshotError::None;
}
SnapshotError validateSnapshotDocument(const JsonDocument& candidate) {
  if (candidate.overflowed() || measureJson(candidate) > 65536) return SnapshotError::Size;
  if (!validateSchema(candidate.as<JsonVariantConst>())) return SnapshotError::Schema;
  const auto units = candidate["units"].as<JsonObjectConst>();
  if (!units["progress"].isUnbound() &&
      validateProgressShape(progress(units["progress"]["data"])) != ProgressError::None)
    return SnapshotError::Schema;
  const auto bookmarks = units["bookmarks"]["data"].as<JsonArrayConst>();
  for (size_t i = 0; i < bookmarks.size(); ++i) {
    const auto b = bookmarks[i];
    const auto text = b["summary"].as<JsonString>();
    if (!validUtf8(reinterpret_cast<const uint8_t*>(text.c_str()), text.size())) return SnapshotError::Encoding;
    if (b["chapterPage"].as<uint16_t>() >= b["chapterPageCount"].as<uint16_t>()) return SnapshotError::Schema;
    for (size_t j = 0; j < i; ++j)
      if (b["spineIndex"] == bookmarks[j]["spineIndex"] && b["chapterPage"] == bookmarks[j]["chapterPage"])
        return SnapshotError::Schema;
  }
  for (const char* direction : {"horizontal", "vertical"}) {
    const auto name = units["readerSettings"]["data"][direction]["font"]["sdFamilyName"];
    if (!name.isUnbound()) {
      const auto text = name.as<JsonString>();
      if (text.size() > 31 || !validUtf8(reinterpret_cast<const uint8_t*>(text.c_str()), text.size()))
        return SnapshotError::Schema;
    }
  }
  return SnapshotError::None;
}
SnapshotError validateSnapshotTarget(const JsonDocument& snapshot, const char* bookId, uint32_t spineCount) {
  if (!bookId || snapshot["bookId"] != bookId) return SnapshotError::BookMismatch;
  if (!spineCount || spineCount > 65535) return SnapshotError::SpineRange;
  const auto units = snapshot["units"].as<JsonObjectConst>();
  if (!units["progress"].isUnbound() &&
      validateProgressForBook(progress(units["progress"]["data"]), spineCount) != ProgressError::None)
    return SnapshotError::SpineRange;
  for (JsonVariantConst b : units["bookmarks"]["data"].as<JsonArrayConst>())
    if (b["spineIndex"].as<uint16_t>() >= spineCount) return SnapshotError::SpineRange;
  return SnapshotError::None;
}
bool snapshotSettingsAvailable(const JsonDocument& snapshot, FontAvailable available, void* context) {
  for (const char* direction : {"horizontal", "vertical"}) {
    const auto font = snapshot["units"]["readerSettings"]["data"][direction]["font"].as<JsonObjectConst>();
    if (!font.isNull() &&
        (!available || !available(font["family"].as<const char*>(), font["sdFamilyName"].as<const char*>(), context)))
      return false;
  }
  return true;
}
}  // namespace yomuka::sync
