#pragma once

#include <cstring>

#define FOOTNOTE_NUMBER_LEN 32
// Calibre can generate long, percent-encoded internal paths. Keep the full
// target so selecting a footnote can resolve it after cache serialization.
#define FOOTNOTE_HREF_LEN 256

struct FootnoteEntry {
  char number[FOOTNOTE_NUMBER_LEN];
  char href[FOOTNOTE_HREF_LEN];

  FootnoteEntry() {
    number[0] = '\0';
    href[0] = '\0';
  }
};
