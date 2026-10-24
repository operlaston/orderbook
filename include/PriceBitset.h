#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

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

  // assumes the passed in position is the first bit set
  // this function is used to update the best ask
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

  // assumes the passed in position is the last bit set
  // this function is used to update the best bid
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
