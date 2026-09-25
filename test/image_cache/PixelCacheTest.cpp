#include <HalStorage.h>
#include <Logging.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
static bool allocationFails = false;
static void* testMalloc(size_t n) { return allocationFails ? nullptr : std::malloc(n); }
#define malloc testMalloc
#include "PixelCache.h"
#undef malloc
#include "DecoderFileScope.h"
#include "ImageCacheValidation.h"

int main() {
  struct Decoder {
    int closes = 0;
    void close() { ++closes; }
  } decoder;
  const auto failedHeader = [&] {
    DecoderFileScope<Decoder> scope(decoder);
    return false;
  };
  assert(!failedHeader() && decoder.closes == 1);
  assert(PixelCache::requiredBytes(480, 800, 1) == 240);
  assert(PixelCache::requiredBytes(528, 792, 1) == 264);
  assert(PixelCache::requiredBytes(8, 2, 18) == 6);
  assert(PixelCache::requiredBytes(0, 8, 1) == 0);
  assert(PixelCache::requiredBytes(65535, 800, 18) == 0);
  // Real writer output, including row gaps, must remain byte-for-byte PXC6.
  {
    PixelCache c;
    assert(c.begin("ok", 8, 4, 0, 0, 1));
    c.buffer[0] = 0x1b;
    c.buffer[1] = 0xe4;
    assert(c.advanceTo(2));
    c.buffer[0] = 0x55;
    c.buffer[1] = 0xaa;
    assert(c.finalize());
    assert(c.buffer == nullptr);
    c.abort();  // must not delete a committed cache
  }
  assert((files.at("ok") == std::vector<uint8_t>{8, 0, 4, 0, 0x1b, 0xe4, 0, 0, 0x55, 0xaa, 0, 0}));
  assert(ImageCacheValidation::validatePixelCacheFile("ok", 8, 4));
  assert(!ImageCacheValidation::validatePixelCacheFile("ok", 12, 4));
  files["invalid"] = files["ok"];
  files["invalid"].pop_back();
  assert(!ImageCacheValidation::validatePixelCacheFile("invalid", 8, 4));
  files["invalid"].resize(3);
  assert(!ImageCacheValidation::validatePixelCacheFile("invalid", 8, 4));
  assert(Storage.remove("invalid"));
  {
    PixelCache c;
    assert(c.begin("invalid", 8, 4, 0, 0, 1));
    assert(c.finalize());
  }
  assert(ImageCacheValidation::validatePixelCacheFile("invalid", 8, 4));
  // Every write boundary: both header writes, advanceTo, and finalize.
  for (int failAt = 0; failAt < 6; ++failAt) {
    writesUntilFailure = failAt;
    {
      PixelCache c;
      if (c.begin("partial", 8, 4, 0, 0, 1)) {
        c.advanceTo(2);
        assert(!c.finalize());
      }
    }
    assert(!files.count("partial"));
  }
  writesUntilFailure = -1;
  {
    PixelCache c;
    assert(c.begin("abandoned", 8, 4, 0, 0, 1));
  }
  assert(!files.count("abandoned"));
  {
    PixelCache c;
    assert(c.begin("close", 8, 4, 0, 0, 1));
    closeFails = true;
    assert(!c.finalize());
    closeFails = false;
  }
  assert(!files.count("close"));
  {
    PixelCache c;
    allocationFails = true;
    assert(!c.begin("oom", 8, 4, 0, 0, 1));
    allocationFails = false;
    openFails = true;
    assert(!c.begin("open", 8, 4, 0, 0, 1));
    openFails = false;
    assert(c.begin("retry", 8, 2, 0, 0, 18));
    assert(c.finalize());
  }
  assert(!files.count("oom") && !files.count("open"));
  assert(files.at("retry").size() == 8);
  puts("PixelCache: output, small image, OOM, open/write/close failures, cleanup and retry passed");
}
