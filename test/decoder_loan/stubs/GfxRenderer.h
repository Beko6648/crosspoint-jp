#pragma once
#include <BuildScratch.h>

#include <cassert>
#include <cstring>

class GfxRenderer {
 public:
  alignas(16) uint8_t storage[52272]{};
  size_t length = sizeof(storage);
  bool hasBuffer = true;
  int lends = 0;
  int restores = 0;
  bool hasFrameBuffer() const { return hasBuffer; }
  class FrameBufferLoan {
   public:
    explicit FrameBufferLoan(GfxRenderer& r) : r_(r) {
      assert(r.hasBuffer && !buildscratch::isLent());
      r.hasBuffer = false;
      ++r.lends;
      buildscratch::lend(r.storage, r.length);
    }
    ~FrameBufferLoan() {
      // The decoder destructor must have released its exclusive claim first.
      assert(buildscratch::available() == r_.length);
      buildscratch::reclaim();
      std::memset(r_.storage, 0xff, r_.length);
      r_.hasBuffer = true;
      ++r_.restores;
    }

   private:
    GfxRenderer& r_;
  };
};
