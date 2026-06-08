#pragma once

#include <atomic>
#include <cstddef>
#include <optional>

// need size of cache line to align read and write ptr
// on separate cache lines (prevent false sharing)

#ifdef __cpp_lib_hardware_interference_size
static constexpr size_t CACHE_LINE_SIZE =
    std::hardware_destructive_interference_size;
#else
static constexpr size_t CACHE_LINE_SIZE = 64;
#endif

template <typename T, size_t Capacity> class SpscQueue {
private:
  static constexpr size_t m_capacity = Capacity + 1;
  alignas(CACHE_LINE_SIZE) T m_buffer[Capacity + 1];

  alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_readPtr;
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_writePtr;

  size_t getNextPos(size_t pos) { return (pos + 1) % m_capacity; }

public:
  SpscQueue() : m_readPtr(0), m_writePtr(0) {}

  bool push(const T &item) {
    size_t currWritePtr = m_writePtr.load(std::memory_order_relaxed);
    size_t nextWritePtr = getNextPos(currWritePtr);
    if (nextWritePtr == m_readPtr.load(std::memory_order_acquire)) {
      return false;
    }

    m_buffer[currWritePtr] = item;
    m_writePtr.store(nextWritePtr, std::memory_order_release);
  }

  std::optional<T> pop() {
    size_t currReadPtr = m_readPtr.load(std::memory_order_relaxed);
    if (currReadPtr == m_writePtr.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    T item = m_buffer[currReadPtr];
    m_readPtr.store(getNextPos(currReadPtr), std::memory_order_release);
    return item;
  }
};
