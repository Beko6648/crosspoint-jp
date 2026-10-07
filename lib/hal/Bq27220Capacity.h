/*
MIT License

Copyright (c) 2026 FreeInk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#pragma once
#include <Arduino.h>

#include <cstdint>
// Adapted from Free-Ink/freeink-sdk PR #132, commit 0086f9a (MIT).
// TI SLUUBD4A 6.1. Included inside a private namespace by HAL and host tests.
// The caller supplies readWord(reg,out) / writeWord(reg,value), and serializes I2C.
constexpr uint8_t BQ27220_CONTROL = 0x00;
constexpr uint8_t BQ27220_FULL_CHARGE_CAPACITY = 0x12;
constexpr uint8_t BQ27220_OPERATION_STATUS = 0x3A;
constexpr uint8_t BQ27220_DESIGN_CAPACITY = 0x3C;
constexpr uint8_t BQ27220_MAC_CONTROL = 0x3E;
constexpr uint8_t BQ27220_MAC_DATA = 0x40;
constexpr uint8_t BQ27220_MAC_DATA_SUM = 0x60;  // MACDataLen() in the high byte
constexpr uint16_t BQ27220_CFGUPDATE = 1 << 10;
constexpr uint8_t BQ27220_SEC_SEALED = 3;
constexpr uint8_t BQ27220_SEC_FULL_ACCESS = 1;
constexpr uint16_t BQ27220_KEYS[] = {0x0414, 0x3672, 0xFFFF, 0xFFFF};  // unseal, then full access
constexpr uint16_t BQ27220_SEALED = 0x0030;
constexpr uint16_t BQ27220_ENTER_CFG_UPDATE = 0x0090;
constexpr uint16_t BQ27220_EXIT_CFG_UPDATE_REINIT = 0x0091;
constexpr uint16_t BQ27220_EXIT_CFG_UPDATE = 0x0092;
constexpr uint16_t BQ27220_DM_LEARNED_FCC = 0x929D;
constexpr uint16_t BQ27220_DM_DESIGN_CAPACITY = 0x929F;
constexpr uint16_t BQ27220_TI_DEFAULT_MAH = 3000;
constexpr unsigned long BQ27220_KEY_GAP_MS = 1500;
constexpr unsigned long BQ27220_FLAG_POLL_MS = 500;
constexpr unsigned long BQ27220_FLAG_GIVE_UP_MS = 5000;

struct Bq27220Load {
  enum class Step : uint8_t { Check, Keys, WaitEnter, WaitExit, Done };
  Step step = Step::Check;
  uint8_t key = 0;
  uint8_t retries = 0;
  bool inConfig = false;
  bool failed = false;
  bool changed = false;  // A parameter update was attempted; verified records success.
  bool verified = false;
  bool wrote = false;
  unsigned long sentAt = 0;
  unsigned long nextAt = 0;
};
Bq27220Load bq27220Load;

uint16_t swapBytes(const uint16_t word) { return static_cast<uint16_t>((word << 8) | (word >> 8)); }

// A Learned Full Charge Capacity more than a quarter above the cell was learned
// against TI's 3000 mAh default: a learning cycle moves it at most 256 mAh down
// (TRM 1.1.3), so it stays far above a small cell, and FullChargeCapacity() copies
// it at every reinit (TRM 1.1.10). One learned on the cell itself is kept.
bool bq27220LearnedTooHigh(const uint16_t learned, const uint16_t mah) { return learned > mah + mah / 4; }

// Replaces TI's default Design Capacity, or a Learned Full Charge Capacity that is
// too high, in one big-endian Data Memory word, adjusting the block checksum by
// the bytes that change.
bool bq27220WriteParam(const uint8_t addr, const uint16_t address, const uint16_t mah, bool& wrote) {
  uint16_t old = 0;
  uint16_t sumAndLength = 0;
  if (!writeWord(BQ27220_MAC_CONTROL, address)) return false;
  delay(10);
  if (!readWord(BQ27220_MAC_DATA, old) || !readWord(BQ27220_MAC_DATA_SUM, sumAndLength)) return false;
  const uint16_t value = swapBytes(old);
  if (address == BQ27220_DM_LEARNED_FCC ? !bq27220LearnedTooHigh(value, mah) : value != BQ27220_TI_DEFAULT_MAH) {
    return true;
  }
  wrote = true;
  const uint8_t sum = static_cast<uint8_t>(sumAndLength + (old & 0xFF) + (old >> 8) - (mah & 0xFF) - (mah >> 8));
  // Reading MACDataSum() moves the X3's gauge to the next block: select this one again.
  if (!writeWord(BQ27220_MAC_CONTROL, address)) return false;
  delay(10);
  return writeWord(BQ27220_MAC_DATA, swapBytes(mah)) &&
         writeWord(BQ27220_MAC_DATA_SUM, static_cast<uint16_t>((sumAndLength & 0xFF00) | sum));
}

bool bq27220Wait(Bq27220Load& s, const Bq27220Load::Step step, const unsigned long now) {
  s.step = step;
  s.sentAt = now;
  s.nextAt = now + BQ27220_FLAG_POLL_MS;
  return true;
}

// Keep retrying closure until readback confirms CONFIG UPDATE off and sealed.
// Persistent bus loss is reported and retried slowly, never marked successful.
bool bq27220Close(Bq27220Load& s, const uint8_t addr, const unsigned long now) {
  uint16_t status = 0;
  if (!readWord(BQ27220_OPERATION_STATUS, status)) {
    s.failed = true;
    s.step = Bq27220Load::Step::WaitExit;
    s.nextAt = now + 1000;
    return true;
  }
  if (status & BQ27220_CFGUPDATE) {
    if (!writeWord(BQ27220_CONTROL, s.wrote ? BQ27220_EXIT_CFG_UPDATE_REINIT : BQ27220_EXIT_CFG_UPDATE))
      s.failed = true;
    s.step = Bq27220Load::Step::WaitExit;
    s.nextAt = now + 500;
    return true;
  }
  if (((status >> 1) & 3) != BQ27220_SEC_SEALED) {
    if (!writeWord(BQ27220_CONTROL, BQ27220_SEALED)) s.failed = true;
    s.step = Bq27220Load::Step::WaitExit;
    s.nextAt = now + 500;
    return true;
  }
  uint16_t dc = 0, fcc = 0;
  if (!readWord(BQ27220_DESIGN_CAPACITY, dc) || !readWord(BQ27220_FULL_CHARGE_CAPACITY, fcc)) {
    s.failed = true;
    s.step = Bq27220Load::Step::WaitExit;
    s.nextAt = now + 1000;
    return true;
  }
  s.verified = dc == 650 && fcc > 0 && !bq27220LearnedTooHigh(fcc, 650);
  s.failed |= !s.verified;
  if (!s.verified && (dc == 3000 || dc == 650) && s.retries++ < 2) {
    s.wrote = false;
    s.step = Bq27220Load::Step::Check;
    s.nextAt = now + 1000;
    return true;
  }
  s.step = Bq27220Load::Step::Done;
  return false;
}

bool bq27220LoadStep(const uint8_t addr, const uint16_t mah, const unsigned long now) {
  using Step = Bq27220Load::Step;
  Bq27220Load& s = bq27220Load;
  if (s.step == Step::Done) return false;
  if (static_cast<long>(now - s.nextAt) < 0) return true;
  uint16_t status = 0;
  switch (s.step) {
    case Step::Check: {
      uint16_t dc = 0;
      s.step = Step::Done;
      if (!readWord(BQ27220_DESIGN_CAPACITY, dc)) {
        s.step = Step::Check;
        s.nextAt = now + 1000;
        s.failed = true;
        return true;
      }
      if (!readWord(BQ27220_OPERATION_STATUS, status)) {
        s.step = Step::Check;
        s.nextAt = now + 1000;
        s.failed = true;
        return true;
      }
      s.inConfig = status & BQ27220_CFGUPDATE;
      const uint8_t security = (status >> 1) & 0b11;
      if (s.inConfig || (dc == mah && security != BQ27220_SEC_SEALED)) {
        s.wrote = true;  // an earlier load was cut short
        return bq27220Close(s, addr, now);
      }
      if (dc == mah) {
        uint16_t fcc = 0;
        if (!readWord(BQ27220_FULL_CHARGE_CAPACITY, fcc)) {
          s.step = Step::Check;
          s.nextAt = now + 1000;
          s.failed = true;
          return true;
        }
        if (!bq27220LearnedTooHigh(fcc, mah)) {
          s.verified = fcc > 0;
          s.failed |= !s.verified;
          return false;
        }
      } else if (dc != BQ27220_TI_DEFAULT_MAH) {
        return false;
      }
      s.key = security == BQ27220_SEC_SEALED ? 0 : security == BQ27220_SEC_FULL_ACCESS ? 4 : 2;
      s.step = Step::Keys;
      [[fallthrough]];
    }
    case Step::Keys:
      if (s.key < 4) {
        if (!writeWord(BQ27220_CONTROL, BQ27220_KEYS[s.key++])) return bq27220Close(s, addr, now);
        s.nextAt = now + BQ27220_KEY_GAP_MS;
        return true;
      }
      s.inConfig = true;
      if (!writeWord(BQ27220_CONTROL, BQ27220_ENTER_CFG_UPDATE)) return bq27220Close(s, addr, now);
      return bq27220Wait(s, Step::WaitEnter, now);
    case Step::WaitExit:
      return bq27220Close(s, addr, now);
    case Step::WaitEnter: {
      const bool entering = s.step == Step::WaitEnter;
      const bool reached =
          readWord(BQ27220_OPERATION_STATUS, status) && static_cast<bool>(status & BQ27220_CFGUPDATE) == entering;
      if (!reached && now - s.sentAt < BQ27220_FLAG_GIVE_UP_MS) {
        s.nextAt = now + BQ27220_FLAG_POLL_MS;
        return true;
      }
      // Design Capacity last: if anything fails it still reads 3000, or FullChargeCapacity()
      // still reads too high, so the next start retries.
      if (entering && reached && bq27220WriteParam(addr, BQ27220_DM_LEARNED_FCC, mah, s.wrote)) {
        s.changed = true;
        if (!bq27220WriteParam(addr, BQ27220_DM_DESIGN_CAPACITY, mah, s.wrote)) s.failed = true;
      }
      return bq27220Close(s, addr, now);
    }
    case Step::Done:
      break;
  }
  return false;
}
