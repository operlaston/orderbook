#pragma once

#include "Order.h"
#include "Utils.h"
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <map>

using PriceLevel = std::list<Order>;

template <typename Compare = std::less<Price>> class BookSide {

public:
  // map iterator is dependent on template parameter
  using Map = std::map<Price, PriceLevel, Compare>;
  using LevelIter = typename Map::iterator;
  using OrderIter = PriceLevel::iterator;

  BookSide() = default;

  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = Order;
    using difference_type = std::ptrdiff_t;
    using pointer = Order *;
    using reference = Order &;

    Iterator() = default;
    Iterator(Map *map, LevelIter levelIt, OrderIter orderIt)
        : m_map(map), m_levelIt(levelIt), m_orderIt(orderIt) {}

    reference operator*() const { return *m_orderIt; }
    pointer operator->() const { return &*m_orderIt; }

    // pre-inc
    Iterator &operator++() {
      m_orderIt++;
      while (m_orderIt == m_levelIt->second.end()) {
        m_levelIt++;
        if (m_levelIt == m_map->end()) {
          break;
        }
        m_orderIt = m_levelIt->second.begin();
      }

      if (m_levelIt == m_map->end()) {
        m_orderIt = OrderIter{};
      }

      return *this;
    }

    // post-inc
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const Iterator &other) const {
      return this->m_map == other.m_map && this->m_levelIt == other.m_levelIt &&
             this->m_orderIt == other.m_orderIt;
    }

    bool operator!=(const Iterator &other) const { return !(*this == other); }

  private:
    Map *m_map = nullptr;
    LevelIter m_levelIt{};
    OrderIter m_orderIt{};
  };

  Iterator begin() {
    if (m_orders.empty()) {
      return end();
    }
    LevelIter levelIt = m_orders.begin();
    while (levelIt != m_orders.end() && levelIt->second.empty()) {
      ++levelIt;
    }
    if (levelIt == m_orders.end()) {
      return end();
    }
    OrderIter orderIt = levelIt->second.begin();
    return Iterator(&m_orders, levelIt, orderIt);
  }

  Iterator end() { return Iterator(&m_orders, m_orders.end(), OrderIter{}); }

private:
  Map m_orders;
  static_assert(std::forward_iterator<Iterator>);
};
