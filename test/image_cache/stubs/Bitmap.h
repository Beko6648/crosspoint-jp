#pragma once
#include <HalStorage.h>
enum class BmpReaderError { Ok };
struct Bitmap {
  explicit Bitmap(FsFile&) {}
  BmpReaderError parseHeaders() { return BmpReaderError::Ok; }
};
