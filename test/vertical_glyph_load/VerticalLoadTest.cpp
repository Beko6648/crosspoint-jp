#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "EpdFontData.h"
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
struct {
  size_t free = 90000, largest = 14324;
  size_t getFreeHeap() { return free; }
  size_t getMaxAllocHeap() { return largest; }
} ESP;
namespace Issue18Diagnostics {
void logMemory(const char*, const char*) {}
}  // namespace Issue18Diagnostics
static constexpr size_t MIN_FREE_HEAP_FOR_VERT_DATA = 32768;
static int allocations = 0, failAt = 0, live = 0, opens = 0;
void* operator new[](size_t bytes, const std::nothrow_t&) noexcept {
  if (++allocations == failAt || bytes > ESP.largest || bytes > ESP.free) return nullptr;
  void* p = std::malloc(bytes);
  if (p) ++live;
  return p;
}
void operator delete[](void* p) noexcept {
  if (p) {
    --live;
    std::free(p);
  }
}
void operator delete[](void* p, size_t) noexcept { operator delete[](p); }
static uint8_t fileData[2048];
static size_t fileSize;
struct FsFile {
  size_t pos = 0;
  bool seekSet(size_t value) {
    pos = value;
    return value < fileSize;
  }
  int read(void* out, size_t count) {
    if (count > fileSize - pos) return 0;
    std::memcpy(out, fileData + pos, count);
    pos += count;
    return static_cast<int>(count);
  }
  void close() {}
};
struct {
  bool openFileForRead(const char*, const char*, FsFile&) {
    ++opens;
    return true;
  }
} Storage;
static uint16_t readU16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
static uint32_t readU32(const uint8_t* p) {
  return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
class SdCardFont {
 public:
  static constexpr int MAX_STYLES = 4;
  struct Style {
    bool present = true, vertLoaded = false;
    uint32_t vertSectionOffset = 1;
    uint16_t vertCount = 0;
    uint32_t* vertCodepoints = nullptr;
    EpdGlyph* vertGlyphs = nullptr;
    uint8_t* vertBitmap = nullptr;
  } styles_[4];
  const char* filePath_ = "fixture.cpfont";
  bool loadVertData(uint8_t style);
  void release() {
    for (auto& s : styles_) {
      delete[] s.vertCodepoints;
      delete[] s.vertGlyphs;
      delete[] s.vertBitmap;
      s.vertCodepoints = nullptr;
      s.vertGlyphs = nullptr;
      s.vertBitmap = nullptr;
      s.vertLoaded = false;
    }
  }
  ~SdCardFont() { release(); }
};
#include "VerticalLoad.inc"
void fixture() {
  assert(live == 0);
  allocations = opens = 0;
  failAt = 0;
  ESP.free = 90000;
  ESP.largest = 14324;
  std::memset(fileData, 0, sizeof(fileData));
  fileData[1] = 2;
  EpdGlyph glyph{};
  glyph.width = 8;
  glyph.height = 8;
  glyph.dataLength = 32;
  const uint32_t points[] = {0x3001, 0x3002};
  for (int i = 0; i < 2; ++i) {
    auto* p = fileData + 3 + i * (4 + sizeof(EpdGlyph));
    std::memcpy(p, &points[i], 4);
    std::memcpy(p + 4, &glyph, sizeof(glyph));
  }
  fileSize = 3 + 2 * (4 + sizeof(EpdGlyph)) + 64;
}
int main() {
  fixture();
  {
    SdCardFont f;
    assert(f.loadVertData(0));
    assert(f.styles_[0].vertCodepoints[0] == 0x3001);
    assert(f.styles_[0].vertGlyphs[0].width == 8);
    int before = allocations;
    assert(f.loadVertData(0));
    assert(allocations == before);
    f.release();
    assert(live == 0);
    assert(f.loadVertData(0));
  }
  assert(live == 0);
  fixture();
  {
    SdCardFont f;
    ESP.free = 32767;
    assert(!f.loadVertData(0));
    assert(opens == 0 && allocations == 0);
    ESP.free = 32768;
    assert(f.loadVertData(0));
  }
  assert(live == 0);
  for (int n = 1; n <= 4; ++n) {
    fixture();
    {
      SdCardFont f;
      failAt = n;
      assert(!f.loadVertData(0));
      assert(live == 0 && !f.styles_[0].vertLoaded);
      failAt = 0;
      assert(f.loadVertData(0));
    }
    assert(live == 0);
  }
  fixture();
  {
    SdCardFont f;
    ESP.largest = 16;
    assert(!f.loadVertData(0));
    assert(live == 0);
    ESP.largest = 14324;
    assert(f.loadVertData(0));
  }
  assert(live == 0);
  for (size_t cut : {size_t(2), size_t(10), size_t(50)}) {
    fixture();
    {
      SdCardFont f;
      auto full = fileSize;
      fileSize = cut;
      assert(!f.loadVertData(0));
      assert(live == 0);
      fileSize = full;
      assert(f.loadVertData(0));
    }
    assert(live == 0);
  }
  fixture();
  {
    SdCardFont f;
    assert(!f.loadVertData(4));
    f.styles_[1].present = false;
    assert(!f.loadVertData(1));
    f.styles_[2].vertSectionOffset = 0;
    assert(!f.loadVertData(2));
  }
  assert(live == 0);
}
