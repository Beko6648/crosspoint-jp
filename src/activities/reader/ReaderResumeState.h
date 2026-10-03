#pragma once

#include <atomic>

// Rendering runs on another task; persist the first successful display from
// the main task (or onExit under RenderLock), never from the render task.
class ReaderResumeState {
  std::atomic<bool> rendered{false};
  bool remembered = false;

 public:
  static void clearRememberedBook();
  void markPageRendered() { rendered.store(true, std::memory_order_release); }
  bool takeRenderedBook() {
    if (remembered || !rendered.load(std::memory_order_acquire)) return false;
    remembered = true;
    return true;
  }
};
