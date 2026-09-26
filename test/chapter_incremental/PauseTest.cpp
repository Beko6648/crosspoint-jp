#include "util/IdleChapterTestPause.h"
#include <cassert>
int main() {
  IdleChapterTestPause p;
  assert(!p.waiting(0,10000));
  assert(!p.arm(100,0)); // Must have real partial output.
  assert(p.arm(200,1));
  assert(p.waiting(200,10000));assert(p.waiting(10199,10000));
  assert(!p.waiting(10200,10000));
  assert(!p.arm(10201,2)); // Never pauses again during this build.
  p.reset();assert(!p.waiting(20000,10000));
  assert(p.arm(UINT32_MAX-100,1));
  assert(p.waiting(50,10000));assert(!p.waiting(9900,10000));
}
