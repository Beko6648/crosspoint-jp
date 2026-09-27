#pragma once
#include <algorithm>
#include <cstdint>
extern unsigned long now;
inline unsigned long millis() { return now; }
class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, ValueIncrease, ValueDecrease };
  bool pressed[8]{}, released[8]{}, held[8]{};
  unsigned long duration = 0;
  bool wasPressed(Button b) const { return pressed[static_cast<int>(b)]; }
  bool wasReleased(Button b) const { return released[static_cast<int>(b)]; }
  bool isPressed(Button b) const { return held[static_cast<int>(b)]; }
  unsigned long getHeldTime() const { return duration; }
};
