// Gauge model adapted from Free-Ink/freeink-sdk PR #132 (MIT; Copyright 2026 FreeInk).
#include <Arduino.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#define hostMillis testMillis
int checksRun = 0;
int checksFailed = 0;

#define CHECK(condition)                                               \
  do {                                                                 \
    ++checksRun;                                                       \
    if (!(condition)) {                                                \
      ++checksFailed;                                                  \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition); \
    }                                                                  \
  } while (0)

constexpr uint16_t TARGET = 650;
constexpr uint16_t DM_BASE = 0x9280;
constexpr uint16_t DM_FCC = 0x929D;
constexpr uint16_t DM_DC = 0x929F;

// Just enough of a BQ27220 to hold the load to TRM 6.1: the keys, CONFIG UPDATE, and
// Data Memory blocks that only take a write in CONFIG UPDATE with FULL ACCESS and
// the right checksum. Like the X3's gauge, it ignores keys sent less than 1.5 s
// apart, drops a second key sent more than 4 s after the first, and moves MACData()
// to the next block when MACDataSum() is read. FullChargeCapacity() copies Learned
// Full Charge Capacity at each reinit (TRM 1.1.10).
struct FakeGauge {
  uint8_t sec = 3;  // SEC[1:0]: 3 sealed, 2 unsealed, 1 full access
  bool cfg = false;
  int refuse = -1;  // index of the transaction the bus refuses
  int reinits = 0;
  uint16_t fcc = 3000;  // FullChargeCapacity()
  std::vector<std::string> log;
  std::array<uint8_t, 0x100> dm{};  // from DM_BASE
  uint16_t mac = 0;                 // selected block
  uint16_t staged = 0;              // MACData() written, not yet committed
  uint16_t lastKey = 0;
  unsigned long lastKeyAt = 0;

  FakeGauge() {
    for (size_t i = 0; i < dm.size(); ++i) dm[i] = static_cast<uint8_t>(i * 37 + 11);
    set(DM_FCC, 3000);
    set(DM_DC, 3000);
  }
  void set(const uint16_t address, const uint16_t value) {
    dm.at(address - DM_BASE) = value >> 8;
    dm.at(address - DM_BASE + 1) = value & 0xFF;
  }
  uint16_t get(const uint16_t address) const { return dm.at(address - DM_BASE) << 8 | dm.at(address - DM_BASE + 1); }
  // MACDataSum() over the address and the block, with the first two bytes replaced.
  uint8_t sum(const uint8_t first, const uint8_t second) const {
    unsigned total = (mac & 0xFF) + (mac >> 8) + first + second;
    for (int i = 2; i < 32; ++i) total += dm.at(mac - DM_BASE + i);
    return static_cast<uint8_t>(255 - total);
  }
  bool keyFollows(const uint16_t first, const uint16_t second, const uint16_t word) const {
    const unsigned long gap = hostMillis - lastKeyAt;
    return word == second && lastKey == first && gap >= 1500 && gap < 4000;
  }

  bool write(const uint8_t reg, const uint16_t word) {
    if (!record("W" + hex(reg) + "=" + hex(word, 4))) return false;
    if (reg == 0x00) {
      if (sec == 3 && keyFollows(0x0414, 0x3672, word)) sec = 2;
      if (sec == 2 && keyFollows(0xFFFF, 0xFFFF, word)) sec = 1;
      if (word == 0x0090 && sec == 1) cfg = true;
      if (word == 0x0091) {
        ++reinits;
        fcc = get(DM_FCC);
      }
      if (word == 0x0091 || word == 0x0092) cfg = false;
      if (word == 0x0030) sec = 3;
      lastKey = word;
      lastKeyAt = hostMillis;
    }
    if (reg == 0x3E) mac = word;
    if (reg == 0x40) staged = word;
    if (reg == 0x60 && cfg && sec == 1 && word == (0x2400 | sum(staged & 0xFF, staged >> 8))) {
      dm.at(mac - DM_BASE) = staged & 0xFF;
      dm.at(mac - DM_BASE + 1) = staged >> 8;
    }
    return true;
  }

  bool read(const uint8_t reg, uint16_t& word) {
    if (!record("R" + hex(reg))) return false;
    if (reg == 0x3A) word = static_cast<uint16_t>((cfg ? 0x0400 : 0) | sec << 1);
    if (reg == 0x3C) word = get(DM_DC);
    if (reg == 0x12) word = fcc;
    if (reg == 0x40) word = static_cast<uint16_t>(dm.at(mac - DM_BASE) | dm.at(mac - DM_BASE + 1) << 8);
    if (reg == 0x60) {
      word = static_cast<uint16_t>(0x2400 | sum(dm.at(mac - DM_BASE), dm.at(mac - DM_BASE + 1)));
      mac += 32;
    }
    return true;
  }

  int writes() const {
    int count = 0;
    for (const auto& entry : log) count += entry[0] == 'W';
    return count;
  }

 private:
  static std::string hex(const unsigned value, const int digits = 2) {
    char text[8];
    std::snprintf(text, sizeof(text), "%0*X", digits, value);
    return text;
  }
  bool record(const std::string& entry) {
    const bool ok = static_cast<int>(log.size()) != refuse;
    log.push_back(entry + (ok ? "" : "!"));
    return ok;
  }
};

FakeGauge* gauge;
bool busUnavailable = false;
bool readWord(uint8_t r, uint16_t& v) { return !busUnavailable && gauge->read(r, v); }
bool writeWord(uint8_t r, uint16_t v) { return !busUnavailable && gauge->write(r, v); }
#include "Bq27220Capacity.h"
void run(FakeGauge& g) {
  gauge = &g;
  bq27220Load = {};
  for (int i = 0; i < 3000; ++i) {
    testMillis += 10;
    if (!bq27220LoadStep(0x55, 650, testMillis)) return;
  }
  assert(false);
}
int main() {
  FakeGauge good;
  run(good);
  assert(good.get(DM_DC) == 650 && good.fcc == 650 && !good.cfg && good.sec == 3 && bq27220Load.verified);
  auto n = good.log.size();
  for (size_t i = 0; i < n; ++i) {
    FakeGauge g;
    g.refuse = i;
    run(g);
    assert(!g.cfg && g.sec == 3 && bq27220Load.verified);
  }
  for (auto dc : {650, 3000}) {
    FakeGauge g;
    g.set(DM_DC, dc);
    g.set(DM_FCC, 2744);
    g.fcc = 2744;
    run(g);
    assert(g.fcc == 650 && bq27220Load.verified);
  }
  FakeGauge aged;
  aged.set(DM_DC, 650);
  aged.fcc = 560;
  run(aged);
  assert(aged.writes() == 0 && aged.fcc == 560);
  FakeGauge other;
  other.set(DM_DC, 1000);
  run(other);
  assert(other.writes() == 0);
  FakeGauge unsealed;
  unsealed.sec = 2;
  run(unsealed);
  assert(unsealed.sec == 3 && unsealed.fcc == 650);
  FakeGauge lost;
  gauge = &lost;
  bq27220Load = {};
  for (int i = 0; i < 3000; ++i) {
    testMillis += 10;
    bq27220LoadStep(0x55, 650, testMillis);
    if (bq27220Load.step == Bq27220Load::Step::WaitExit) break;
  }
  lost.refuse = lost.log.size();
  testMillis += 1000;
  assert(bq27220LoadStep(0x55, 650, testMillis));
  assert(!bq27220Load.verified);
  lost.refuse = -1;
  for (int i = 0; i < 3000; ++i) {
    testMillis += 10;
    if (!bq27220LoadStep(0x55, 650, testMillis)) break;
  }
  assert(!lost.cfg && lost.sec == 3);
  FakeGauge outage;
  gauge = &outage;
  bq27220Load = {};
  for (int i = 0; i < 3000; ++i) {
    testMillis += 10;
    bq27220LoadStep(0x55, 650, testMillis);
    if (bq27220Load.step == Bq27220Load::Step::WaitExit) break;
  }
  busUnavailable = true;
  const auto before = outage.log.size();
  for (int i = 0; i < 100; ++i) {
    testMillis += 100;
    assert(bq27220LoadStep(0x55, 650, testMillis));
  }
  assert(!bq27220Load.verified && bq27220Load.failed && outage.log.size() == before);
  busUnavailable = false;
  for (int i = 0; i < 3000; ++i) {
    testMillis += 10;
    if (!bq27220LoadStep(0x55, 650, testMillis)) break;
  }
  assert(bq27220Load.verified && !outage.cfg && outage.sec == 3);
  // Reboot at each loader step: closure on the next boot, correction on a subsequent boot if necessary.
  for (int stop = 0; stop < 1000; stop += 10) {
    FakeGauge g;
    gauge = &g;
    bq27220Load = {};
    for (int t = 0; t < stop; ++t) {
      testMillis += 10;
      if (!bq27220LoadStep(0x55, 650, testMillis)) break;
    }
    run(g);
    if (g.cfg || g.sec != 3) {
      printf("restart stop=%d cfg=%d sec=%d dc=%u fcc=%u\n", stop, g.cfg, g.sec, g.get(DM_DC), g.fcc);
      fflush(stdout);
    }
    assert(!g.cfg && g.sec == 3);
    run(g);
    assert(g.get(DM_DC) == 650 && g.fcc == 650 && !g.cfg && g.sec == 3);
  }
  printf("Capacity correction tests passed: %zu injected transaction failures plus guards and recovery\n", n);
}
