#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "SyncProgress.h"
#include "SyncTimestamp.h"

using namespace yomuka::sync;

namespace {
std::uint64_t checks = 0;
unsigned cases = 0;

#define CHECK(expression)                                                \
  do {                                                                   \
    ++checks;                                                            \
    if (!(expression)) {                                                 \
      std::fprintf(stderr, "FAIL line=%d: %s\n", __LINE__, #expression); \
      std::exit(1);                                                      \
    }                                                                    \
  } while (false)

template <typename F>
void runCase(const char* name, F test) {
  test();
  ++cases;
  std::printf("PASS %s\n", name);
}

bool same(const Progress& a, const Progress& b) {
  return a.spineIndex == b.spineIndex && a.chapterPage == b.chapterPage && a.chapterPageCount == b.chapterPageCount &&
         a.finished == b.finished && a.percent == b.percent;
}

bool same(const ProjectedProgress& a, const ProjectedProgress& b) {
  return a.spineIndex == b.spineIndex && a.chapterPage == b.chapterPage && a.chapterPageCount == b.chapterPageCount &&
         a.terminal == b.terminal && a.approximate == b.approximate;
}

Progress known(std::uint16_t index = 2, std::uint16_t page = 3, std::uint16_t count = 10, bool finished = false,
               std::uint8_t percent = 35) {
  return {index, page, count, finished, percent};
}

void expectDecodeFailure(const std::uint8_t* data, std::size_t size, ProgressError error) {
  Progress out = known();
  const auto before = out;
  CHECK(decodeLegacyProgress(data, size, out) == error);
  CHECK(same(out, before));
}

void expectEncodeFailure(const Progress& value, std::size_t capacity, ProgressError error) {
  std::array<std::uint8_t, 12> out;
  out.fill(0xA5U);
  const auto before = out;
  std::size_t written = 123;
  CHECK(encodeLegacyProgress(value, out.data(), capacity, written) == error);
  CHECK(out == before);
  CHECK(written == 123U);
}

void expectProjectionFailure(const Progress& value, std::uint32_t count, ResolvedChapterLayout layout,
                             ProgressError error) {
  ProjectedProgress out{17, 8, 20, true, false};
  const auto before = out;
  CHECK(projectProgressForBook(value, count, layout, out) == error);
  CHECK(same(out, before));
}

std::uint32_t randomState = 0x53F1A821U;
std::uint32_t nextRandom() {
  randomState ^= randomState << 13U;
  randomState ^= randomState >> 17U;
  randomState ^= randomState << 5U;
  return randomState;
}

void put16(std::uint8_t* out, std::uint16_t value) {
  out[0] = static_cast<std::uint8_t>(value & 255U);
  out[1] = static_cast<std::uint8_t>(value >> 8U);
}
}  // namespace

int main() {
  runCase("legacy-4-preserves-unknown-fields", [] {
    const std::uint8_t bytes[] = {0x34, 0x12, 0x78, 0x56};
    Progress out = known();
    CHECK(decodeLegacyProgress(bytes, sizeof(bytes), out) == ProgressError::None);
    CHECK(out.spineIndex == 0x1234U && out.chapterPage == 0x5678U);
    CHECK(!out.chapterPageCount && !out.finished && !out.percent);
  });
  runCase("legacy-6-page-count-is-not-finished", [] {
    const std::uint8_t bytes[] = {2, 0, 3, 0, 10, 0};
    Progress out;
    CHECK(decodeLegacyProgress(bytes, sizeof(bytes), out) == ProgressError::None);
    CHECK(out.chapterPageCount == 10U && !out.finished && !out.percent);
  });
  runCase("legacy-7-false-is-known-not-null", [] {
    const std::uint8_t bytes[] = {2, 0, 3, 0, 10, 0, 0};
    Progress out;
    CHECK(decodeLegacyProgress(bytes, sizeof(bytes), out) == ProgressError::None);
    CHECK(out.finished.has_value() && !*out.finished && !out.percent);
  });
  runCase("legacy-8-known-percent-and-little-endian", [] {
    const std::uint8_t bytes[] = {0x34, 0x12, 0x78, 0x56, 0, 0x60, 1, 100};
    Progress out;
    CHECK(decodeLegacyProgress(bytes, sizeof(bytes), out) == ProgressError::None);
    CHECK(out.spineIndex == 0x1234U && out.chapterPage == 0x5678U);
    CHECK(out.chapterPageCount == 0x6000U && out.finished == true && out.percent == 100U);
  });
  runCase("percent-255-is-unknown-and-canonicalizes-to-7", [] {
    const std::uint8_t bytes[] = {2, 0, 3, 0, 10, 0, 1, 255};
    Progress out;
    CHECK(decodeLegacyProgress(bytes, sizeof(bytes), out) == ProgressError::None);
    CHECK(!out.percent && out.finished == true);
    std::uint8_t encoded[8] = {};
    std::size_t size = 0;
    CHECK(encodeLegacyProgress(out, encoded, sizeof(encoded), size) == ProgressError::None);
    CHECK(size == 7U);
  });
  runCase("all-non-record-lengths-rejected-without-output-change", [] {
    const std::uint8_t bytes[40] = {};
    for (std::size_t size = 0; size <= sizeof(bytes); ++size) {
      if (size != 4U && size != 6U && size != 7U && size != 8U) {
        expectDecodeFailure(bytes, size, ProgressError::UnsupportedLength);
      }
    }
    expectDecodeFailure(nullptr, std::numeric_limits<std::size_t>::max(), ProgressError::UnsupportedLength);
  });
  runCase("null-input-is-not-empty-progress", [] {
    for (std::size_t size : {4U, 6U, 7U, 8U}) expectDecodeFailure(nullptr, size, ProgressError::NullBuffer);
    expectDecodeFailure(nullptr, 0, ProgressError::UnsupportedLength);
  });
  runCase("all-finished-byte-values", [] {
    std::uint8_t bytes[] = {0, 0, 0, 0, 1, 0, 0, 0};
    for (unsigned value = 0; value < 256U; ++value) {
      bytes[6] = static_cast<std::uint8_t>(value);
      if (value > 1U) {
        expectDecodeFailure(bytes, sizeof(bytes), ProgressError::InvalidFinishedFlag);
      } else {
        Progress out;
        CHECK(decodeLegacyProgress(bytes, sizeof(bytes), out) == ProgressError::None);
        CHECK(out.finished == (value != 0U));
      }
    }
  });
  runCase("all-percent-byte-values", [] {
    std::uint8_t bytes[] = {0, 0, 0, 0, 1, 0, 0, 0};
    for (unsigned value = 0; value < 256U; ++value) {
      bytes[7] = static_cast<std::uint8_t>(value);
      if (value > 100U && value != 255U) {
        expectDecodeFailure(bytes, sizeof(bytes), ProgressError::InvalidPercent);
      } else {
        Progress out;
        CHECK(decodeLegacyProgress(bytes, sizeof(bytes), out) == ProgressError::None);
        CHECK(value == 255U ? !out.percent : out.percent == value);
      }
    }
  });
  runCase("known-page-count-rejects-equal-and-greater-page", [] {
    const std::uint8_t equal[] = {0, 0, 10, 0, 10, 0};
    const std::uint8_t greater[] = {0, 0, 11, 0, 10, 0};
    expectDecodeFailure(equal, sizeof(equal), ProgressError::InvalidPageRange);
    expectDecodeFailure(greater, sizeof(greater), ProgressError::InvalidPageRange);
  });
  runCase("zero-count-requires-zero-page", [] {
    const std::uint8_t valid[] = {0, 0, 0, 0, 0, 0};
    const std::uint8_t invalid[] = {0, 0, 1, 0, 0, 0};
    Progress out;
    CHECK(decodeLegacyProgress(valid, sizeof(valid), out) == ProgressError::None);
    CHECK(out.chapterPageCount.has_value() && *out.chapterPageCount == 0U);
    expectDecodeFailure(invalid, sizeof(invalid), ProgressError::InvalidPageRange);
  });
  runCase("shape-validation-rejects-wire-percent-255", [] {
    auto value = known();
    value.percent = 255;
    CHECK(validateProgressShape(value) == ProgressError::InvalidPercent);
    expectEncodeFailure(value, 12, ProgressError::InvalidPercent);
  });
  runCase("normal-book-range-and-zero-spines", [] {
    CHECK(validateProgressForBook(known(), 3) == ProgressError::None);
    CHECK(validateProgressForBook(known(), 0) == ProgressError::InvalidSpineCount);
    CHECK(validateProgressForBook(known(), 1) == ProgressError::SpineOutOfRange);
  });
  runCase("finished-95-percent-is-not-terminal", [] {
    const auto value = known(2, 9, 10, true, 95);
    ProjectedProgress out;
    CHECK(projectProgressForBook(value, 3, {true, 20}, out) == ProgressError::None);
    CHECK(!out.terminal && out.spineIndex == 2U && out.chapterPage == 18U);
  });
  runCase("exact-terminal-needs-no-reflow", [] {
    const auto value = known(3, 0, 0, true, 100);
    CHECK(validateProgressForBook(value, 3) == ProgressError::None);
    ProjectedProgress out;
    CHECK(projectProgressForBook(value, 3, {false, 0}, out) == ProgressError::None);
    CHECK(out.terminal && !out.approximate && out.spineIndex == 3U);
    CHECK(out.chapterPage == 0U && out.chapterPageCount == 0U);
  });
  runCase("partial-terminal-tuples-rejected", [] {
    const auto terminal = known(3, 0, 0, true, 100);
    auto value = terminal;
    value.chapterPageCount.reset();
    CHECK(validateProgressForBook(value, 3) == ProgressError::InvalidTerminal);
    value = terminal;
    value.finished.reset();
    CHECK(validateProgressForBook(value, 3) == ProgressError::InvalidTerminal);
    value = terminal;
    value.finished = false;
    CHECK(validateProgressForBook(value, 3) == ProgressError::InvalidTerminal);
    value = terminal;
    value.percent.reset();
    CHECK(validateProgressForBook(value, 3) == ProgressError::InvalidTerminal);
    value = terminal;
    value.percent = 95;
    CHECK(validateProgressForBook(value, 3) == ProgressError::InvalidTerminal);
    value = terminal;
    value.chapterPageCount = 1;
    CHECK(validateProgressForBook(value, 3) == ProgressError::InvalidTerminal);
  });
  runCase("spine-field-max-and-book-count-no-narrowing", [] {
    const auto value = known(65535, 0, 1, false, 0);
    CHECK(validateProgressForBook(value, 65536U) == ProgressError::None);
    CHECK(validateProgressForBook(value, 65534U) == ProgressError::SpineOutOfRange);
    CHECK(validateProgressForBook(known(65535, 0, 0, true, 100), 65535U) == ProgressError::None);
  });
  runCase("projection-floor-and-different-layout", [] {
    ProjectedProgress out;
    CHECK(projectProgressForBook(known(0, 3, 10), 1, {true, 7}, out) == ProgressError::None);
    CHECK(out.chapterPage == 2U && out.chapterPageCount == 7U && out.approximate);
  });
  runCase("same-count-still-does-not-claim-text-anchor", [] {
    ProjectedProgress out;
    CHECK(projectProgressForBook(known(0, 3, 10), 1, {true, 10}, out) == ProgressError::None);
    CHECK(out.chapterPage == 3U && out.approximate);
  });
  runCase("projection-unknown-count-clamps", [] {
    auto value = known(0, 100, 101);
    value.chapterPageCount.reset();
    ProjectedProgress out;
    CHECK(projectProgressForBook(value, 1, {true, 7}, out) == ProgressError::None);
    CHECK(out.chapterPage == 6U);
  });
  runCase("empty-successful-target-is-not-parser-failure", [] {
    ProjectedProgress out;
    CHECK(projectProgressForBook(known(0, 3, 10), 1, {true, 0}, out) == ProgressError::None);
    CHECK(out.chapterPage == 0U && out.chapterPageCount == 0U && !out.terminal);
    expectProjectionFailure(known(0, 3, 10), 1, {false, 0}, ProgressError::LayoutNotReady);
  });
  runCase("projection-failures-never-modify-output", [] {
    expectProjectionFailure(known(), 1, {true, 10}, ProgressError::SpineOutOfRange);
    expectProjectionFailure(known(1, 0, 0, false, 0), 1, {true, 10}, ProgressError::InvalidTerminal);
    expectProjectionFailure(known(0, 10, 10), 1, {true, 10}, ProgressError::InvalidPageRange);
    expectProjectionFailure(known(), 4, {false, 10}, ProgressError::LayoutNotReady);
  });
  runCase("projection-max-multiplication-boundary", [] {
    ProjectedProgress out;
    CHECK(projectProgressForBook(known(0, 65534, 65535), 1, {true, 65535}, out) == ProgressError::None);
    CHECK(out.chapterPage == 65534U);
    CHECK(projectProgressForBook(known(0, 65534, 65535), 1, {true, 1}, out) == ProgressError::None);
    CHECK(out.chapterPage == 0U);
  });
  runCase("all-null-patterns-never-invent-known-values", [] {
    for (unsigned bits = 0; bits < 8U; ++bits) {
      Progress value;
      if ((bits & 1U) != 0U) value.chapterPageCount = 1;
      if ((bits & 2U) != 0U) value.finished = false;
      if ((bits & 4U) != 0U) value.percent = 0;
      const bool representable = bits == 0U || bits == 1U || bits == 3U || bits == 7U;
      if (!representable) {
        CHECK(validateProgressShape(value) == ProgressError::None);
        expectEncodeFailure(value, 12, ProgressError::UnrepresentableLegacyState);
      } else {
        std::uint8_t bytes[8] = {};
        std::size_t size = 999;
        CHECK(encodeLegacyProgress(value, bytes, sizeof(bytes), size) == ProgressError::None);
        CHECK(size == (bits == 0U ? 4U : bits == 1U ? 6U : bits == 3U ? 7U : 8U));
        Progress roundtrip;
        CHECK(decodeLegacyProgress(bytes, size, roundtrip) == ProgressError::None);
        CHECK(same(value, roundtrip));
      }
    }
  });
  runCase("encode-output-size-and-guard-bytes", [] {
    std::array<std::uint8_t, 12> buffer;
    buffer.fill(0xA5U);
    std::size_t written = 0;
    CHECK(encodeLegacyProgress(known(0x1234, 0x5678, 0x6000, true, 0), buffer.data() + 2, 8, written) ==
          ProgressError::None);
    const std::array<std::uint8_t, 8> expected = {0x34, 0x12, 0x78, 0x56, 0, 0x60, 1, 0};
    CHECK(written == 8U && std::memcmp(buffer.data() + 2, expected.data(), 8) == 0);
    CHECK(buffer[0] == 0xA5U && buffer[1] == 0xA5U && buffer[10] == 0xA5U && buffer[11] == 0xA5U);
  });
  runCase("encode-capacity-null-and-shape-failures", [] {
    for (std::size_t capacity = 0; capacity < 8U; ++capacity) {
      expectEncodeFailure(known(), capacity, ProgressError::BufferTooSmall);
    }
    expectEncodeFailure(known(0, 10, 10), 12, ProgressError::InvalidPageRange);
    std::size_t written = 77;
    CHECK(encodeLegacyProgress(known(), nullptr, 8, written) == ProgressError::NullBuffer);
    CHECK(written == 77U);
  });
  runCase("known-zero-percent-is-not-null", [] {
    std::uint8_t bytes[8] = {};
    std::size_t size = 0;
    CHECK(encodeLegacyProgress(known(0, 0, 1, false, 0), bytes, sizeof(bytes), size) == ProgressError::None);
    CHECK(size == 8U && bytes[7] == 0U);
    Progress out;
    CHECK(decodeLegacyProgress(bytes, size, out) == ProgressError::None);
    CHECK(out.percent.has_value() && *out.percent == 0U);
  });
  runCase("randomized-50000-valid-legacy-roundtrips", [] {
    constexpr std::array<std::size_t, 4> lengths = {4, 6, 7, 8};
    for (unsigned iteration = 0; iteration < 50000U; ++iteration) {
      std::uint8_t bytes[8] = {};
      const auto size = lengths[iteration % 4U];
      const auto count = static_cast<std::uint16_t>(nextRandom() & 65535U);
      const auto page = count == 0U ? static_cast<std::uint16_t>(0) : static_cast<std::uint16_t>(nextRandom() % count);
      put16(bytes, static_cast<std::uint16_t>(nextRandom() & 65535U));
      put16(bytes + 2, size == 4U ? static_cast<std::uint16_t>(nextRandom() & 65535U) : page);
      put16(bytes + 4, count);
      bytes[6] = static_cast<std::uint8_t>(nextRandom() & 1U);
      const auto percent = nextRandom() % 102U;
      bytes[7] = static_cast<std::uint8_t>(percent == 101U ? 255U : percent);
      Progress a;
      CHECK(decodeLegacyProgress(bytes, size, a) == ProgressError::None);
      std::uint8_t encoded[8] = {};
      std::size_t written = 0;
      CHECK(encodeLegacyProgress(a, encoded, sizeof(encoded), written) == ProgressError::None);
      Progress b;
      CHECK(decodeLegacyProgress(encoded, written, b) == ProgressError::None);
      CHECK(same(a, b));
    }
  });
  runCase("projection-grid-230272-combinations", [] {
    constexpr std::array<std::uint16_t, 7> targets = {0, 1, 2, 7, 16, 255, 65535};
    for (std::uint32_t count = 1; count <= 256U; ++count) {
      for (std::uint32_t page = 0; page < count; ++page) {
        for (const auto target : targets) {
          ProjectedProgress out;
          CHECK(projectProgressForBook(known(0, static_cast<std::uint16_t>(page), static_cast<std::uint16_t>(count)), 1,
                                       {true, target}, out) == ProgressError::None);
          const auto numerator = static_cast<std::uint64_t>(page) * target;
          CHECK(out.chapterPage == numerator / count);
          CHECK(target == 0U ? out.chapterPage == 0U : out.chapterPage < target);
        }
      }
    }
  });
  runCase("timestamps-do-not-change-on-failed-or-noop-save", [] {
    for (const auto outcome : {LocalSaveOutcome::Unchanged, LocalSaveOutcome::Failed}) {
      CHECK(timestampAfterLocalSave(123, outcome, {true, 999}) == 123U);
      CHECK(timestampAfterLocalSave(123, outcome, {false, 0}) == 123U);
      CHECK(timestampAfterLocalSave(0, outcome, {true, 999}) == 0U);
    }
  });
  runCase("unknown-clock-resets-stale-update-time", [] {
    CHECK(timestampAfterLocalSave(123, LocalSaveOutcome::Saved, {false, 999}) == 0U);
    CHECK(timestampAfterLocalSave(123, LocalSaveOutcome::Saved, {true, 0}) == 0U);
    CHECK(timestampAfterLocalSave(123, LocalSaveOutcome::Saved, {true, 999}) == 999U);
  });
  runCase("timestamp-range-no-wrap", [] {
    constexpr auto max = std::numeric_limits<std::uint32_t>::max();
    CHECK(timestampAfterLocalSave(123, LocalSaveOutcome::Saved, {true, max}) == max);
    CHECK(timestampAfterLocalSave(123, LocalSaveOutcome::Saved, {true, static_cast<std::uint64_t>(max) + 1U}) == 0U);
    CHECK(timestampAfterLocalSave(123, LocalSaveOutcome::Saved, {true, std::numeric_limits<std::uint64_t>::max()}) ==
          0U);
  });
  runCase("import-keeps-original-timestamp", [] {
    CHECK(timestampForImport(0) == 0U);
    CHECK(timestampForImport(123) == 123U);
    CHECK(timestampForImport(std::numeric_limits<std::uint32_t>::max()) == std::numeric_limits<std::uint32_t>::max());
  });
  runCase("timestamp-comparisons-are-hints-only", [] {
    CHECK(compareTimestamps(0, 0) == TimestampComparison::Unknown);
    CHECK(compareTimestamps(0, 10) == TimestampComparison::Unknown);
    CHECK(compareTimestamps(10, 0) == TimestampComparison::Unknown);
    CHECK(compareTimestamps(10, 10) == TimestampComparison::Equal);
    CHECK(compareTimestamps(11, 10) == TimestampComparison::LocalNewer);
    CHECK(compareTimestamps(10, 11) == TimestampComparison::IncomingNewer);
  });
  runCase("error-names-never-require-dynamic-storage", [] {
    for (unsigned value = 0; value <= static_cast<unsigned>(ProgressError::BufferTooSmall); ++value) {
      const auto* name = progressErrorName(static_cast<ProgressError>(value));
      CHECK(name != nullptr && *name != '\0');
      CHECK(std::strcmp(name, "unknown-error") != 0);
    }
    CHECK(std::strcmp(progressErrorName(static_cast<ProgressError>(255)), "unknown-error") == 0);
  });
  std::printf("SUMMARY cases=%u checks=%llu\n", cases, static_cast<unsigned long long>(checks));
  return 0;
}
