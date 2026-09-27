#include <Arduino.h>
#include <Epub/css/CssParser.h>

#include <cassert>
#include <iostream>

#include "util/CacheProgressPolicy.h"
int main() {
  CacheProgressPolicy progress(100);
  assert(progress.shouldUpdate(0, 0, 100));
  progress.displayed(0, 0, 100);
  assert(!progress.shouldUpdate(0, 0, 5099));
  assert(progress.shouldUpdate(0, 0, 5100));
  assert(progress.shouldUpdate(1, 0, 101));
  assert(progress.shouldUpdate(0, 25, 101));
  progress.displayed(0, 0, UINT32_MAX - 2000);
  assert(!progress.shouldUpdate(0, 0, 2000));
  assert(progress.shouldUpdate(0, 0, 3000));
  CssParser parser("/book");
  FsFile input;
  const std::string css = "p { font-weight: bold; } .test { text-align: center; }";
  input.bytes = std::make_shared<std::vector<uint8_t>>(css.begin(), css.end());
  assert(parser.loadFromStream(input));
  assert(parser.ruleCount() == 2 && parser.saveToCache());
  parser.clear();
  ESP = {};
  assert(parser.loadFromCache(0, nullptr, true) && parser.ruleCount() == 2);
  // The initial reserve guard is probe 0; probe 2 fails after one rule.
  ESP = {};
  ESP.failAt = 2;
  assert(!parser.loadFromCache(0, nullptr, true) && parser.empty());
  ESP = {};
  ESP.failAt = 2;
  assert(parser.loadFromCache(0, nullptr, false) && parser.ruleCount() == 1);
  ESP = {};
  assert(parser.loadFromCache(0, nullptr, true) && parser.ruleCount() == 2);
  parser.clear();
  assert(parser.saveToCache());
  assert(parser.loadFromCache(0, nullptr, true) && parser.empty());
  parser.deleteCache();
  assert(!parser.loadFromCache(0, nullptr, true));
  std::cout << "PASS: timed progress, book transition, wraparound; strict partial CSS rejection, reader fallback, "
               "retry, empty and missing CSS\n";
}
