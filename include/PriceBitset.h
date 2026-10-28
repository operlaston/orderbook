#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

// One bit per price. Bit p lives in word p >> 6 at bit position p & 63, so
// scanning 64 prices costs one word compare and the first/last set bit in a
// word is a single ctz/clz instruction.
//
// Price 0 is reserved: the scan functions return 0 to mean "no bit set", so
// the order book never stores anything at price 0.
class PriceBitset {
public:
  PriceBitset(uint64_t size) {
    uint64_t vecSize = size >> shift;
    if ((size & mask) != 0) {
      vecSize++;
    }
    m_bitset = std::vector<uint64_t>(vecSize, 0);
  }

  bool get(uint64_t pos) const {
    return m_bitset[getIndexOf(pos)] & getMaskWithPosSet(pos);
  }

  void set(uint64_t pos) {
    m_bitset[getIndexOf(pos)] |= getMaskWithPosSet(pos);
  }

  void unset(uint64_t pos) {
    m_bitset[getIndexOf(pos)] &= ~getMaskWithPosSet(pos);
  }

  // Clears pos and returns the lowest set bit above it (0 if none). Used to
  // find the new best ask, so pos must be the lowest set bit. That lets the
  // scan start at pos's word and use ctz on the first non-zero word.
  uint64_t unsetAndGetNext(uint64_t pos) {
    unset(pos);
    uint64_t currIndex = getIndexOf(pos);
    while (currIndex < m_bitset.size() && m_bitset[currIndex] == 0) {
      currIndex++;
    }
    if (currIndex == m_bitset.size()) return 0;
    uint8_t setBitLocal = __builtin_ctzll(m_bitset[currIndex]);
    return (currIndex << shift) | setBitLocal;
  }

  // Clears pos and returns the highest set bit below it (0 if none). Used to
  // find the new best bid, so pos must be the highest set bit. The scan walks
  // words downwards and takes 63 - clz of the first non-zero one.
  uint64_t unsetAndGetPrev(uint64_t pos) {
    unset(pos);
    uint64_t currIndex = getIndexOf(pos);
    while (currIndex > 0 && m_bitset[currIndex] == 0) {
      currIndex--;
    }
    if (currIndex == 0 && m_bitset[currIndex] == 0) return 0;
    uint8_t setBitLocal = mask - __builtin_clzll(m_bitset[currIndex]);
    return (currIndex << shift) | setBitLocal;
  }

private:
  // uint64_t m_size;
  std::vector<uint64_t> m_bitset;

  static constexpr uint8_t mask = 63;
  static constexpr uint8_t shift = 6;

  inline uint64_t getMaskWithPosSet(uint64_t pos) const {
    return (static_cast<uint64_t>(1) << (pos & mask));
  }

  inline uint64_t getIndexOf(uint64_t pos) const {
    return (pos >> shift);
  }
};
