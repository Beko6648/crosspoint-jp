#include "MappedInputManager.h"

#include "CrossPointSettings.h"

namespace {
using ButtonIndex = uint8_t;

// GPIO labels reflect ADC channels, not physical position. Keep the physical
// order here so every activity uses the same X3/X4 mapping.
// X4: first=upper BTN_UP, second=lower BTN_DOWN (verified on Yomuka X4).
// X3: first=left BTN_UP, second=right BTN_DOWN.
struct SidePair {
  ButtonIndex first;
  ButtonIndex second;
};
constexpr SidePair kSideX4{HalGPIO::BTN_UP, HalGPIO::BTN_DOWN};
constexpr SidePair kSideX3{HalGPIO::BTN_UP, HalGPIO::BTN_DOWN};

// Mirror a front button hardware index (0<->3, 1<->2) for inverted orientation.
// Physical buttons reverse left-to-right when the device is held upside down.
constexpr ButtonIndex mirrorFront(ButtonIndex idx) { return 3 - idx; }
}  // namespace

bool MappedInputManager::mapButton(const Button button, bool (HalGPIO::*fn)(uint8_t) const) const {
  const auto sideLayout = static_cast<CrossPointSettings::SIDE_BUTTON_LAYOUT>(SETTINGS.sideButtonLayout);
  const bool isX3 = gpio.deviceIsX3();
  const SidePair& sideHw = isX3 ? kSideX3 : kSideX4;
  const bool inverted = effectiveOrientation == Orientation::PortraitInverted;
  const bool landscapeCW = effectiveOrientation == Orientation::LandscapeClockwise;
  const bool landscapeCCW = effectiveOrientation == Orientation::LandscapeCounterClockwise;

  const ButtonIndex sideFirst = inverted ? sideHw.second : sideHw.first;
  const ButtonIndex sideSecond = inverted ? sideHw.first : sideHw.second;
  const ButtonIndex sideIncrease = isX3 ? sideHw.second : sideHw.first;
  const ButtonIndex sideDecrease = isX3 ? sideHw.first : sideHw.second;
  const bool prevNext = sideLayout == CrossPointSettings::PREV_NEXT;
  const ButtonIndex pageBackHw = prevNext ? sideHw.first : sideHw.second;
  const ButtonIndex pageForwardHw = prevNext ? sideHw.second : sideHw.first;

  switch (button) {
    case Button::Back:
      // Logical Back maps to user-configured front button.
      // Inverted: mirror the hardware position.
      return (gpio.*fn)(inverted ? mirrorFront(SETTINGS.frontButtonBack) : SETTINGS.frontButtonBack);
    case Button::Confirm:
      // Logical Confirm maps to user-configured front button.
      return (gpio.*fn)(inverted ? mirrorFront(SETTINGS.frontButtonConfirm) : SETTINGS.frontButtonConfirm);
    case Button::Left:
      // CCW: front buttons rotate to right side, physical top-to-bottom is
      // GPIO 3,2,1,0. "Left" (previous) should be physical top = GPIO of Right.
      if (inverted) return (gpio.*fn)(mirrorFront(SETTINGS.frontButtonLeft));
      if (landscapeCCW) return (gpio.*fn)(SETTINGS.frontButtonRight);
      return (gpio.*fn)(SETTINGS.frontButtonLeft);
    case Button::Right:
      // CCW: "Right" (next) should be physical bottom-ish = GPIO of Left.
      if (inverted) return (gpio.*fn)(mirrorFront(SETTINGS.frontButtonRight));
      if (landscapeCCW) return (gpio.*fn)(SETTINGS.frontButtonLeft);
      return (gpio.*fn)(SETTINGS.frontButtonRight);
    case Button::Up:
      return (gpio.*fn)(sideFirst);
    case Button::Down:
      return (gpio.*fn)(sideSecond);
    case Button::ValueIncrease:
      return (gpio.*fn)(sideIncrease);
    case Button::ValueDecrease:
      return (gpio.*fn)(sideDecrease);
    case Button::Power:
      // Power button bypasses remapping.
      return (gpio.*fn)(HalGPIO::BTN_POWER);
    case Button::PageBack:
      // Reader page navigation uses side buttons and can be swapped via settings.
      // Inverted: side buttons swap physical position.
      // CW: side buttons move to bottom, Down(left)/Up(right), swap needed.
      if (inverted || landscapeCW) return (gpio.*fn)(pageForwardHw);
      return (gpio.*fn)(pageBackHw);
    case Button::PageForward:
      if (inverted || landscapeCW) return (gpio.*fn)(pageBackHw);
      return (gpio.*fn)(pageForwardHw);
  }

  return false;
}

bool MappedInputManager::wasPressed(const Button button) const { return mapButton(button, &HalGPIO::wasPressed); }

bool MappedInputManager::wasReleased(const Button button) const { return mapButton(button, &HalGPIO::wasReleased); }

bool MappedInputManager::isPressed(const Button button) const { return mapButton(button, &HalGPIO::isPressed); }

bool MappedInputManager::wasAnyPressed() const { return gpio.wasAnyPressed(); }

bool MappedInputManager::wasAnyReleased() const { return gpio.wasAnyReleased(); }

unsigned long MappedInputManager::getHeldTime() const { return gpio.getHeldTime(); }

MappedInputManager::Labels MappedInputManager::mapLabels(const char* back, const char* confirm, const char* previous,
                                                         const char* next) const {
  // Build the label order based on the configured hardware mapping.
  // LandscapeCCW: front buttons rotate to right side (vertical). Physical
  // top-to-bottom becomes GPIO 3,2,1,0. drawButtonHints reverses labels
  // (0<->3, 1<->2) so that visual top = labels[3]. To make physical top = previous
  // (user expectation: up = previous page), we swap previous<->next in the label
  // assignment so that after drawButtonHints' reversal the labels match.
  const bool swapPrevNext = effectiveOrientation == Orientation::LandscapeCounterClockwise;
  const char* prev = swapPrevNext ? next : previous;
  const char* nxt = swapPrevNext ? previous : next;

  auto labelForHardware = [&](uint8_t hw) -> const char* {
    // Compare against configured logical roles and return the matching label.
    if (hw == SETTINGS.frontButtonBack) {
      return back;
    }
    if (hw == SETTINGS.frontButtonConfirm) {
      return confirm;
    }
    if (hw == SETTINGS.frontButtonLeft) {
      return prev;
    }
    if (hw == SETTINGS.frontButtonRight) {
      return nxt;
    }
    return "";
  };

  return {labelForHardware(HalGPIO::BTN_BACK), labelForHardware(HalGPIO::BTN_CONFIRM),
          labelForHardware(HalGPIO::BTN_LEFT), labelForHardware(HalGPIO::BTN_RIGHT)};
}

int MappedInputManager::getPressedFrontButton() const {
  // Scan the raw front buttons in hardware order.
  // This bypasses remapping so the remap activity can capture physical presses.
  if (gpio.wasPressed(HalGPIO::BTN_BACK)) {
    return HalGPIO::BTN_BACK;
  }
  if (gpio.wasPressed(HalGPIO::BTN_CONFIRM)) {
    return HalGPIO::BTN_CONFIRM;
  }
  if (gpio.wasPressed(HalGPIO::BTN_LEFT)) {
    return HalGPIO::BTN_LEFT;
  }
  if (gpio.wasPressed(HalGPIO::BTN_RIGHT)) {
    return HalGPIO::BTN_RIGHT;
  }
  return -1;
}
