#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "DecoderFileScope.h"
#include "LoanedDecoder.h"

struct alignas(16) Decoder {
  static inline bool failHeap = false;
  static inline int live = 0;
  static inline int closed = 0;
  bool opened = false;
  Decoder() { ++live; }
  ~Decoder() {
    assert(!opened);
    --live;
  }
  void close() {
    assert(opened);
    opened = false;
    ++closed;
  }
  static void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return failHeap ? nullptr : std::malloc(size);
  }
  static void* operator new(size_t, void* p) { return p; }
  static void operator delete(void* p) { std::free(p); }
};

int main() {
  GfxRenderer r;
  bool invalidated = false;
  {
    LoanedDecoder<Decoder> d;
    assert(d.tryHeap());
    assert(!d.usesScratch());
  }
  assert(!invalidated && r.lends == 0 && Decoder::live == 0);
  Decoder::failHeap = true;
  {
    LoanedDecoder<Decoder> d;
    assert(!d.tryHeap());
    assert(d.tryLoan(r, invalidated));
    assert(invalidated && d.usesScratch() && !r.hasFrameBuffer());
    assert(reinterpret_cast<uintptr_t>(&*d) % alignof(Decoder) == 0);
    d->opened = true;
    DecoderFileScope<Decoder> file(*d);  // closes before placement destruction
  }
  assert(r.hasFrameBuffer() && r.restores == 1 && !buildscratch::isLent());
  assert(Decoder::closed == 1 && Decoder::live == 0);

  // Existing external loan: use it, but do not acquire or return another loan.
  alignas(16) uint8_t external[128]{};
  buildscratch::lend(external + 1, sizeof(external) - 1);
  invalidated = false;
  {
    LoanedDecoder<Decoder> d;
    assert(d.tryLoan(r, invalidated));
    assert(!invalidated && r.lends == 1);
    assert(reinterpret_cast<uintptr_t>(&*d) % alignof(Decoder) == 0);
  }
  assert(buildscratch::available() == sizeof(external) - 1);
  auto* claimed = buildscratch::claim(1);
  {
    LoanedDecoder<Decoder> d;
    assert(!d.tryLoan(r, invalidated));
  }
  assert(buildscratch::available() == 0 && r.lends == 1);
  buildscratch::release(claimed);
  buildscratch::reclaim();

  // Too short after alignment: claim must be returned on failure.
  buildscratch::lend(external + 1, sizeof(Decoder));
  {
    LoanedDecoder<Decoder> d;
    assert(!d.tryLoan(r, invalidated));
  }
  assert(buildscratch::available() == sizeof(Decoder));
  buildscratch::reclaim();
  r.length = 1;
  invalidated = false;
  {
    LoanedDecoder<Decoder> d;
    assert(!d.tryLoan(r, invalidated));
  }
  assert(invalidated && r.hasFrameBuffer() && r.restores == 2);
  r.hasBuffer = false;
  invalidated = false;
  {
    LoanedDecoder<Decoder> d;
    assert(!d.tryLoan(r, invalidated));
  }
  assert(!invalidated && !buildscratch::isLent() && Decoder::live == 0);
  puts("Decoder loan: heap failure fallback, alignment, exclusive claim, close/destruction/restore ordering passed");
}
