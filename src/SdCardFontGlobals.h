#pragma once

#include "SdCardFontSystem.h"

class GfxRenderer;

// Global SD card font system instance (defined in main.cpp).
extern SdCardFontSystem sdFontSystem;

// Ensure the correct SD card font family is loaded for the given writing direction.
// Defined in main.cpp; call before entering the reader or after settings change.
extern void ensureSdFontLoaded(bool isVertical = false);

// Resolve and assign the ruby font for the selected writing direction.
// Call after ensureSdFontLoaded() so the SD font resolver is ready.
extern void configureRubyFont(bool isVertical);
