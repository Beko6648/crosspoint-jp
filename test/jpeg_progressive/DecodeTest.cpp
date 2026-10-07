#include <JPEGDEC.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

struct Pixels {
  uint64_t hash = 14695981039346656037ULL;
  size_t count = 0;
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

int main(int argc, char** argv) {
  std::puts("fixture,scale,result,error,pixels,hash");
  for (int i = 1; i < argc; ++i) {
    std::ifstream file(argv[i], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    assert(!data.empty());
    for (int scale : {0, JPEG_SCALE_HALF, JPEG_SCALE_QUARTER, JPEG_SCALE_EIGHTH}) {
      auto jpeg = std::make_unique<JPEGDEC>();
      Pixels pixels;
      assert(jpeg->openRAM(data.data(), static_cast<int>(data.size()), draw) == 1);
      const int originalSampling = jpeg->getSubSample();
      jpeg->setPixelType(EIGHT_BIT_GRAYSCALE);
      jpeg->setUserPointer(&pixels);
      const int result = jpeg->decode(0, 0, scale);
      assert(jpeg->getSubSample() == originalSampling);
      std::printf("%s,%d,%d,%d,%zu,%llu\n", argv[i], scale, result, jpeg->getLastError(), pixels.count,
                  static_cast<unsigned long long>(pixels.hash));
      jpeg->close();
    }
  }
}
