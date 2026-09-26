#include "ImageRenderDiagnostics.h"
int main() {
  imagerenderdiag::PageScope page(true,0,7);
  imagerenderdiag::CacheScope cache(0);
  imagerenderdiag::mark("disabled",42);
}
