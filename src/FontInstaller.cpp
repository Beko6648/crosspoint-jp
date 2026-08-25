#include "FontInstaller.h"

#include <HalStorage.h>
#include <Logging.h>
#include <mbedtls/sha256.h>

#include <cctype>
#include <cstring>

#include "CrossPointSettings.h"

FontInstaller::FontInstaller(SdCardFontRegistry& registry) : registry_(registry) {}

bool FontInstaller::isValidFamilyName(const char* name) {
  if (name == nullptr || name[0] == '\0') return false;

  // Reject path traversal
  if (strstr(name, "..") != nullptr) return false;
  if (strchr(name, '/') != nullptr) return false;
  if (strchr(name, '\\') != nullptr) return false;

  for (const char* p = name; *p != '\0'; ++p) {
    char c = *p;
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
      return false;
    }
  }
  return true;
}

bool FontInstaller::ensureFamilyDir(const char* familyName) {
  // Ensure base fonts directory exists
  if (!Storage.exists(SdCardFontRegistry::FONTS_DIR)) {
    if (!Storage.mkdir(SdCardFontRegistry::FONTS_DIR)) {
      LOG_ERR("FONT", "Failed to create fonts dir: %s", SdCardFontRegistry::FONTS_DIR);
      return false;
    }
  }

  char dirPath[128];
  snprintf(dirPath, sizeof(dirPath), "%s/%s", SdCardFontRegistry::FONTS_DIR, familyName);

  if (!Storage.exists(dirPath)) {
    if (!Storage.mkdir(dirPath)) {
      LOG_ERR("FONT", "Failed to create family dir: %s", dirPath);
      return false;
    }
  }
  return true;
}

bool FontInstaller::validateCpfontFile(const char* path) {
  FsFile file;
  if (!Storage.openFileForRead("FONT", path, file)) {
    LOG_ERR("FONT", "Cannot open for validation: %s", path);
    return false;
  }

  uint8_t magic[CPFONT_MAGIC_LEN];
  size_t bytesRead = file.read(magic, CPFONT_MAGIC_LEN);
  file.close();

  if (bytesRead < CPFONT_MAGIC_LEN) {
    LOG_ERR("FONT", "File too small: %s (%zu bytes)", path, bytesRead);
    return false;
  }

  if (memcmp(magic, "CPFONT\0\0", CPFONT_MAGIC_LEN) != 0) {
    LOG_ERR("FONT", "Bad magic in: %s", path);
    return false;
  }

  return true;
}

bool FontInstaller::verifySha256File(const char* path, const char* expectedHex) {
  if (expectedHex == nullptr || expectedHex[0] == '\0') return true;  // Legacy manifest without hashes.
  if (strlen(expectedHex) != 64) {
    LOG_ERR("FONT", "Invalid SHA-256 length for: %s", path);
    return false;
  }

  FsFile file;
  if (!Storage.openFileForRead("FONT", path, file)) {
    LOG_ERR("FONT", "Cannot open for SHA-256: %s", path);
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, /*is224=*/0);

  uint8_t buffer[512];
  size_t remaining = file.fileSize();
  bool readOk = true;
  while (remaining > 0) {
    const size_t wanted = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const int got = file.read(buffer, wanted);
    if (got <= 0 || static_cast<size_t>(got) != wanted) {
      readOk = false;
      break;
    }
    mbedtls_sha256_update(&sha, buffer, wanted);
    remaining -= wanted;
  }

  uint8_t digest[32];
  if (readOk) mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  file.close();
  if (!readOk) {
    LOG_ERR("FONT", "Failed to read for SHA-256: %s", path);
    return false;
  }

  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  char actualHex[65];
  for (size_t i = 0; i < sizeof(digest); ++i) {
    actualHex[i * 2] = HEX_DIGITS[digest[i] >> 4];
    actualHex[i * 2 + 1] = HEX_DIGITS[digest[i] & 0x0F];
  }
  actualHex[64] = '\0';

  bool matches = true;
  for (size_t i = 0; i < 64; ++i) {
    const char expected = static_cast<char>(std::tolower(static_cast<unsigned char>(expectedHex[i])));
    if (actualHex[i] != expected) {
      matches = false;
      break;
    }
  }
  if (!matches) LOG_ERR("FONT", "SHA-256 mismatch: %s", path);
  return matches;
}

void FontInstaller::buildFontPath(const char* family, const char* filename, char* outBuf, size_t outBufSize) {
  snprintf(outBuf, outBufSize, "%s/%s/%s", SdCardFontRegistry::FONTS_DIR, family, filename);
}

FontInstaller::Error FontInstaller::deleteFamily(const char* familyName) {
  if (!isValidFamilyName(familyName)) {
    return Error::INVALID_FAMILY_NAME;
  }

  const SdCardFontFamilyInfo* family = registry_.findFamily(familyName);
  const char* familyDir = family ? family->path.c_str() : SdCardFontRegistry::FONTS_DIR;

  char dirPath[128];
  if (family) {
    snprintf(dirPath, sizeof(dirPath), "%s", familyDir);
  } else {
    snprintf(dirPath, sizeof(dirPath), "%s/%s", familyDir, familyName);
  }

  if (!Storage.exists(dirPath)) {
    LOG_DBG("FONT", "Family dir does not exist: %s", dirPath);
    return Error::OK;  // Already gone
  }

  // Recursively remove the directory and all its contents
  if (!Storage.removeDir(dirPath)) {
    LOG_ERR("FONT", "Failed to remove family dir: %s", dirPath);
    return Error::SD_WRITE_ERROR;
  }

  // If this was the active font in either direction, clear the setting
  bool cleared = false;
  if (strcmp(SETTINGS.horizontal.sdFontFamilyName, familyName) == 0) {
    SETTINGS.horizontal.sdFontFamilyName[0] = '\0';
    cleared = true;
  }
  if (strcmp(SETTINGS.vertical.sdFontFamilyName, familyName) == 0) {
    SETTINGS.vertical.sdFontFamilyName[0] = '\0';
    cleared = true;
  }
  if (cleared) {
    SETTINGS.saveToFile();
    LOG_DBG("FONT", "Cleared active SD font (deleted family: %s)", familyName);
  }

  return Error::OK;
}

void FontInstaller::refreshRegistry() { registry_.discover(); }

bool FontInstaller::isFamilyInstalled(const char* familyName) const {
  return registry_.findFamily(familyName) != nullptr;
}
