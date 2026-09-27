#include <cassert>
#include <cstring>

#include "ImageRenderDiagnostics.h"
using namespace imagerenderdiag;
int main() {
  mark("outside");
  assert(count == 0);
  {
    PageScope textPage(false, 0, 0);
    mark("text");
  }
  assert(logs == 0 && count == 0);
  {
    PageScope imagePage(true, 0, 7);
    assert(count == 1 && logs == 0);
    ESP.freeBytes = 30000;
    mark("allocated", 52000);
    ESP.freeBytes = 80000;
    assert(samples[1].freeBytes == 30000 && samples[1].detail == 52000);
    task = &secondTask;
    mark("other-task");
    assert(count == 2);
    task = &firstTask;
    {
      PageScope nested(true, 0, 8);
      mark("nested");
    }
    assert(owner == task && logs == 0);
  }
  assert(owner == nullptr && logs == count + 1);
  assert(!std::strcmp(samples[count - 1].stage, "page-end"));
  {
    PageScope full(true, 0, 9);
    for (unsigned i = 0; i < 100; ++i) mark("overflow", i);
    assert(count == 64 && dropped == 37);
  }
  assert(count == 64 && dropped == 38 && owner == nullptr);
}
