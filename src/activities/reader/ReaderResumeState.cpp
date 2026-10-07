#include "ReaderResumeState.h"

#include "CrossPointState.h"

void ReaderResumeState::clearRememberedBook() {
  if (APP_STATE.openEpubPath.empty()) return;
  APP_STATE.openEpubPath.clear();
  APP_STATE.saveToFile();
}
