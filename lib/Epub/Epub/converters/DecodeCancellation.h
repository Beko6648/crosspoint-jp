#pragma once
// A cooperative request. A cancelled decode must discard its partial cache.
struct DecodeCancellation {
  bool (*requested)(void*) = nullptr;
  void* context = nullptr;
  bool cancelled = false;
  bool poll() {
    if (!cancelled && requested) cancelled = requested(context);
    return cancelled;
  }
};
