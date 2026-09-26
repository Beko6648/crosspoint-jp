#include <JPEGDEC.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "DecoderFileScope.h"
#include "LoanedDecoder.h"

struct Pixels {
  uint64_t hash = 14695981039346656037ULL;
  size_t count = 0;
  int result = 0;
  int error = 0;
};
int draw(JPEGDRAW* block) {
  auto& out = *static_cast<Pixels*>(block->pUser);
  const auto* pixels = reinterpret_cast<const uint8_t*>(block->pPixels);
  for (int y = 0; y < block->iHeight; ++y) {
    for (int x = 0; x < block->iWidthUsed; ++x) {
      out.hash = (out.hash ^ pixels[y * block->iWidth + x]) * 1099511628211ULL;
      ++out.count;
    }
  }
  return 1;
}

Pixels decode(std::vector<uint8_t>& data, GfxRenderer& renderer, bool loan, int scale) {
  Pixels pixels;
  bool invalidated = false;
  {
    LoanedDecoder<JPEGDEC> jpeg;
    assert(loan ? jpeg.tryLoan(renderer, invalidated) : jpeg.tryHeap());
    assert(jpeg->openRAM(data.data(), static_cast<int>(data.size()), draw) == 1);
    DecoderFileScope<JPEGDEC> file(*jpeg);
    jpeg->setPixelType(EIGHT_BIT_GRAYSCALE);
    jpeg->setUserPointer(&pixels);
    pixels.result = jpeg->decode(0, 0, scale);
    pixels.error = jpeg->getLastError();
    if (loan) {
      // Scratch beyond the decoder must not be touched by the real library.
      for (size_t i = sizeof(JPEGDEC); i < renderer.length; ++i) assert(renderer.storage[i] == 0xa5);
    }
  }
  assert(renderer.hasFrameBuffer() && !buildscratch::isLent());
  assert(invalidated == loan);
  return pixels;
}

int main(int argc, char** argv) {
  assert(argc > 1);
  int passed = 0, rejected = 0;
  for (int i = 1; i < argc; ++i) {
    std::ifstream file(argv[i], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    assert(!data.empty());
    for (size_t capacity : {size_t(48000), size_t(52272)}) {
      GfxRenderer renderer;
      renderer.length = capacity;
      for (int scale : {0, JPEG_SCALE_HALF, JPEG_SCALE_QUARTER, JPEG_SCALE_EIGHTH}) {
        const Pixels heap = decode(data, renderer, false, scale);
        std::memset(renderer.storage, 0xa5, renderer.length);
        const Pixels loan = decode(data, renderer, true, scale);
        assert(heap.result == loan.result && heap.error == loan.error);
        assert(heap.count == loan.count && heap.hash == loan.hash);
        if (heap.result == 1) {
          assert(heap.count > 0);
          ++passed;
        } else {
          ++rejected;
          std::printf("Both reject fixture=%s scale=%d error=%d\n", argv[i], scale, heap.error);
        }
      }
    }
  }
  assert(passed >= 48);
  printf("Successful combinations=%d; matching baseline rejections=%d\n", passed, rejected);
  printf("Real JPEGDEC: %d JPEG fixtures, 4 scales, X3/X4 capacities: heap/loan pixels match; canaries intact\n",
         argc - 1);
}
