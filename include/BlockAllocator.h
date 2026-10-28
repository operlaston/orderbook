#pragma once

#include "Utils.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <stdexcept>

template<typename T>
class BlockAllocator;

// A fixed number of equal-sized blocks carved from one cache-line-aligned
// buffer. Free blocks form an intrusive singly linked list: a free block's
// first 8 bytes store the pointer to the next free block, so no extra memory
// is needed. That is why blockSize must be at least a pointer, and a multiple
// of 8 so every block stays pointer-aligned. allocate/deallocate just pop and
// push the list head.
class FixedPool {
public:
  FixedPool(uint64_t blockCount, uint64_t blockSize) : m_blockSize(blockSize){
    assert(blockSize % 8 == 0);
    if (blockCount == 0 || blockSize < sizeof(size_t)) {
      throw std::runtime_error("block size must be at least as big as a pointer and blockCount must be > 0");
    }
    uint64_t bufSize = blockCount * blockSize;
    m_buffer = static_cast<uint8_t *>(::operator new(bufSize, std::align_val_t(CACHE_LINE_SIZE)));
    m_lastAddress = m_buffer + (blockCount * blockSize) - 1;

    freeListHead = reinterpret_cast<FreeNode *>(m_buffer);
    uint64_t currIdx = 0;
    for (uint64_t i = 0; i < blockCount; i++) {
      FreeNode *currNode = reinterpret_cast<FreeNode *>(&m_buffer[currIdx]);
      if (i == blockCount - 1) {
        currNode->next = nullptr;
      }
      else {
        uint64_t nextIdx = currIdx + blockSize;
        currNode->next = reinterpret_cast<FreeNode *>(&m_buffer[nextIdx]);
        currIdx = nextIdx;
      }
    }
  }

  // defining any of these could cause trouble since FixedPool instances
  // are referred to be BlockAllocator objects
  FixedPool(const FixedPool&) = delete;
  const FixedPool& operator=(const FixedPool&) = delete;
  FixedPool(FixedPool&& other) = delete;
  const FixedPool& operator=(FixedPool&& other) = delete;

  ~FixedPool() {
    ::operator delete(m_buffer, std::align_val_t(CACHE_LINE_SIZE));
  }

  void *allocate() {
    if (freeListHead == nullptr) {
      return nullptr;
    }
    else {
      void *allocaPtr = freeListHead;
      freeListHead = freeListHead->next;
      return allocaPtr;
    }
  }

  void deallocate(void *mem) {
    FreeNode* prevHead = freeListHead;
    freeListHead = static_cast<FreeNode *>(mem);
    freeListHead->next = prevHead;
  }

private:
  template<typename T>
  friend class BlockAllocator;

  struct FreeNode {
    FreeNode *next;
  };

  uint8_t *m_buffer;
  const uint64_t m_blockSize;
  uint8_t *m_lastAddress;
  FreeNode *freeListHead;
};


// STL allocator backed by a FixedPool. std::list rebinds it to its node type
// and allocates one node at a time. Those come from the pool while it has
// blocks left. Anything else (n != 1, a type bigger than a block, or an empty
// pool) falls back to ::operator new.
template<typename T>
class BlockAllocator {
public:
  using value_type = T;

  template<typename U>
  friend class BlockAllocator;

  explicit BlockAllocator(FixedPool &pool) : m_pool(&pool) {}

  template<typename U>
  BlockAllocator(const BlockAllocator<U>& other) : m_pool(other.m_pool) {}

  T* allocate(std::size_t n) {
    if (n == 1 && sizeof(T) <= m_pool->m_blockSize) {
      if (void *p = m_pool->allocate()) {
        return static_cast<T*>(p);
      }
    }
    return static_cast<T*>(::operator new(n * sizeof(T)));
  }

  // Pool and heap memory are told apart by address: anything inside the
  // pool's buffer goes back to the free list, everything else was heap
  // allocated.
  void deallocate(T* ptr, std::size_t n) {
    (void)n;
    uint8_t *p = reinterpret_cast<uint8_t *>(ptr);
    if (p >= m_pool->m_buffer && p <= m_pool->m_lastAddress) {
      m_pool->deallocate(ptr);
    }
    else {
      ::operator delete(ptr);
    }
  }

  template<typename U>
  bool operator==(const BlockAllocator<U>& other) const {
    return m_pool == other.m_pool;
  }

  template<typename U>
  bool operator!=(const BlockAllocator<U>& other) const {
    return m_pool != other.m_pool;
  }

private:
  FixedPool *m_pool;
};
