#pragma once

#include "Order.h"
#include "Utils.h"
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <type_traits>
#include <unordered_map>

using PriceLevel = std::list<Order>;

template <typename Compare = std::less<Price>> class BookSide {

public:
  // map iterator is dependent on template parameter
  using Map = std::map<Price, PriceLevel, Compare>;
  using LevelIter = typename Map::iterator;
  using OrderIter = PriceLevel::iterator;
  using LevelConstIter = typename Map::const_iterator;
  using OrderConstIter = PriceLevel::const_iterator;

  BookSide() = default;

  // template on constness to implement const_iterator
  template <bool isConst> struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = Order;
    using difference_type = std::ptrdiff_t;
    using pointer = std::conditional_t<isConst, const Order *, Order *>;
    using reference = std::conditional_t<isConst, const Order &, Order &>;

    template <bool> friend struct Iterator;
    friend class BookSide;

    using MapT = std::conditional_t<isConst, const Map *, Map *>;
    using LevelIterT = std::conditional_t<isConst, LevelConstIter, LevelIter>;
    using OrderIterT = std::conditional_t<isConst, OrderConstIter, OrderIter>;

    Iterator() = default;
    Iterator(MapT map, LevelIterT levelIt, OrderIterT orderIt)
        : m_map(map), m_levelIt(levelIt), m_orderIt(orderIt) {}

    // allow conversion from iterator -> const_iterator
    Iterator(const Iterator<!isConst> &other)
      requires isConst
        : m_map(other.m_map), m_levelIt(other.m_levelIt),
          m_orderIt(other.m_orderIt) {}

    reference operator*() const { return *m_orderIt; }
    pointer operator->() const { return &*m_orderIt; }

    // pre-inc
    Iterator &operator++() {
      m_orderIt++;

      // traverse empty levels
      while (m_orderIt == m_levelIt->second.end()) {
        m_levelIt++;
        if (m_levelIt == m_map->end()) {
          break;
        }
        m_orderIt = m_levelIt->second.begin();
      }

      if (m_levelIt == m_map->end()) {
        m_orderIt = OrderIterT{};
      }

      return *this;
    }

    // post-inc
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    // templated to allow const_iter==iter comparisons + vice-versa
    template <bool C> bool operator==(const Iterator<C> &other) const {
      return this->m_map == other.m_map && this->m_levelIt == other.m_levelIt &&
             this->m_orderIt == other.m_orderIt;
    }

    template <bool C> bool operator!=(const Iterator<C> &other) const {
      return !(*this == other);
    }

  private:
    MapT m_map = nullptr;
    LevelIterT m_levelIt{};
    OrderIterT m_orderIt{};
  };

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

  iterator begin() {
    LevelIter levelIt = m_orders.begin();
    while (levelIt != m_orders.end() && levelIt->second.empty()) {
      ++levelIt;
    }
    if (levelIt == m_orders.end()) {
      return end();
    }
    OrderIter orderIt = levelIt->second.begin();
    return iterator{&m_orders, levelIt, orderIt};
  }

  iterator end() { return iterator{&m_orders, m_orders.end(), OrderIter{}}; }

  // overloaded for const_iterator (const BookSide)
  const_iterator begin() const {
    LevelConstIter levelIt = m_orders.begin();
    while (levelIt != m_orders.end() && levelIt->second.empty()) {
      ++levelIt;
    }
    if (levelIt == m_orders.end()) {
      return end();
    }
    OrderConstIter orderIt = levelIt->second.begin();
    return const_iterator{&m_orders, levelIt, orderIt};
  }

  const_iterator end() const {
    return const_iterator{&m_orders, m_orders.end(), OrderConstIter{}};
  }

  const_iterator cbegin() const { return begin(); }

  const_iterator cend() const { return end(); }

  // update activeOrder automatically and insert the order into the book
  iterator insert(const Order &order,
                  std::unordered_map<OrderId, OrderIter> &activeOrders) {
    auto levelIt = m_orders.try_emplace(order.getPrice()).first;
    levelIt->second.push_back(order);
    auto orderIt = std::prev(levelIt->second.end());
    assert(!activeOrders.contains(order.getId()));
    activeOrders[order.getId()] = orderIt;
    return iterator{&m_orders, levelIt, orderIt};
  }

  // update activeOrders automatically and erase the underlying Order object
  iterator erase(iterator it,
                 std::unordered_map<OrderId, OrderIter> &activeOrders) {
    OrderIter orderIt = it.m_orderIt;
    activeOrders.erase(orderIt->getId());
    LevelIter levelIt = it.m_levelIt;
    assert(levelIt != m_orders.end());

    // get the iterator to the next order object and cleanup empty level if
    // there is one
    OrderIter nextOrderIt = levelIt->second.erase(orderIt);
    if (nextOrderIt == levelIt->second.end()) {
      LevelIter nextLevelIt = levelIt;
      nextLevelIt++;
      if (levelIt->second.empty()) {
        m_orders.erase(levelIt);
      }
      if (nextLevelIt == m_orders.end()) {
        return end();
      }
      nextOrderIt = nextLevelIt->second.begin();
      return iterator{&m_orders, nextLevelIt, nextOrderIt};
    }
    return iterator{&m_orders, levelIt, nextOrderIt};
  }

private:
  Map m_orders;
  static_assert(std::forward_iterator<iterator>);
  static_assert(std::forward_iterator<const_iterator>);
};
