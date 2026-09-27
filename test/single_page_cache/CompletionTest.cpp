#include <Epub/SinglePageCacheCompletion.h>

#include <cassert>
#include <initializer_list>
int main() {
  using namespace singlepagecache;
  assert(eligible(1, 0, 1, true, false));
  for (int spines : {0, 2, 10}) assert(!eligible(spines, 0, 1, true, false));
  assert(!eligible(1, 1, 1, true, false));
  assert(!eligible(1, 0, 0, true, false));
  assert(!eligible(1, 0, 2, true, false));
  assert(!eligible(1, 0, 1, false, false));
  assert(!eligible(1, 0, 1, true, true));
  for (const char* tag : {"img", "svg", "image", "svg:image", "x:img", "object", "embed", "picture", "audio", "video"})
    assert(isMediaElement(tag));
  for (const char* tag : {"p", "span", "ruby", "table", "body"}) assert(!isMediaElement(tag));
}
