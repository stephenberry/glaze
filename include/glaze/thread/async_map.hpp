#pragma once

// Provides a semi-safe flat map
// This async_map only provides thread safety when inserting/deletion
// It is intended to store thread safe types for more efficient access

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "glaze/core/common.hpp"

// This async_map is intended to hold thread safe value types (V)

namespace glz
{
   template <class K, class V>
   struct async_map
   {
     private:
      std::vector<std::unique_ptr<std::pair<K, V>>> items;
      mutable std::shared_mutex mutex;

      // Helper function to perform binary search.
      // Returns a pair of iterator and a boolean indicating if the key was found.
      auto binary_search_key(const K& key) const
      {
         auto it = std::lower_bound(
            items.cbegin(), items.cend(), key,
            [](const std::unique_ptr<std::pair<K, V>>& item_ptr, const K& key) { return item_ptr->first < key; });
         if (it != items.cend() && !(key < (*it)->first)) { // Equivalent to key == (*it)->first
            return std::make_pair(it, true);
         }
         return std::make_pair(it, false);
      }

      auto binary_search_key(const K& key)
      {
         auto it = std::lower_bound(
            items.begin(), items.end(), key,
            [](const std::unique_ptr<std::pair<K, V>>& item_ptr, const K& key) { return item_ptr->first < key; });
         if (it != items.end() && !(key < (*it)->first)) { // Equivalent to key == (*it)->first
            return std::make_pair(it, true);
         }
         return std::make_pair(it, false);
      }

     public:
      using key_type = K;
      using mapped_type = V;
      using value_type = std::pair<K, V>;
      using const_value_type = const std::pair<K, V>;

      // Forward declaration of iterator classes
      class iterator;
      class const_iterator;

      // Iterator Class Definition
      class iterator
      {
        public:
         using iterator_category = std::forward_iterator_tag;
         using value_type = async_map::value_type;
         using difference_type = std::ptrdiff_t;
         using pointer = value_type*;
         using reference = value_type&;

        private:
         typename std::vector<std::unique_ptr<std::pair<K, V>>>::iterator item_it;
         async_map* map;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr;

        public:
         iterator(typename std::vector<std::unique_ptr<std::pair<K, V>>>::iterator item_it, async_map* map,
                  std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock = nullptr,
                  std::shared_ptr<std::unique_lock<std::shared_mutex>> existing_unique_lock = nullptr)
            : item_it(item_it), map(map), shared_lock_ptr(existing_shared_lock), unique_lock_ptr(existing_unique_lock)
         {
            // Acquire a shared lock only if no lock is provided
            if (!shared_lock_ptr && !unique_lock_ptr) {
               shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(map->mutex);
            }
         }

         // Copy Constructor
         iterator(const iterator& other)
            : item_it(other.item_it),
              map(other.map),
              shared_lock_ptr(other.shared_lock_ptr),
              unique_lock_ptr(other.unique_lock_ptr)
         {}

         // Move Constructor
         iterator(iterator&& other) noexcept
            : item_it(std::move(other.item_it)),
              map(other.map),
              shared_lock_ptr(std::move(other.shared_lock_ptr)),
              unique_lock_ptr(std::move(other.unique_lock_ptr))
         {}

         // Copy Assignment
         iterator& operator=(const iterator& other)
         {
            if (this != &other) {
               item_it = other.item_it;
               map = other.map;
               shared_lock_ptr = other.shared_lock_ptr;
               unique_lock_ptr = other.unique_lock_ptr;
            }
            return *this;
         }

         // Pre-increment
         iterator& operator++()
         {
            ++item_it;
            return *this;
         }

         // Post-increment
         iterator operator++(int)
         {
            iterator tmp(*this);
            ++(*this);
            return tmp;
         }

         reference operator*() const { return *(*item_it); }

         pointer operator->() const { return (*item_it).get(); }

         // Equality Comparison
         bool operator==(const iterator& other) const { return item_it == other.item_it; }

         // Inequality Comparison
         bool operator!=(const iterator& other) const { return !(*this == other); }

         // Comparison operators with iterator
         bool operator<(const iterator& other) const { return item_it < other.item_it; }
         bool operator>(const iterator& other) const { return item_it > other.item_it; }
         bool operator<=(const iterator& other) const { return item_it <= other.item_it; }
         bool operator>=(const iterator& other) const { return item_it >= other.item_it; }

         // Comparison operators with const_iterator
         bool operator<(const const_iterator& other) const { return item_it < other.item_it; }
         bool operator>(const const_iterator& other) const { return item_it > other.item_it; }
         bool operator<=(const const_iterator& other) const { return item_it <= other.item_it; }
         bool operator>=(const const_iterator& other) const { return item_it >= other.item_it; }
      };

      class const_iterator
      {
        public:
         using iterator_category = std::forward_iterator_tag;
         using value_type = async_map::const_value_type;
         using difference_type = std::ptrdiff_t;
         using pointer = const value_type*;
         using reference = const value_type&;

        private:
         typename std::vector<std::unique_ptr<std::pair<K, V>>>::const_iterator item_it;
         const async_map* map;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;

        public:
         const_iterator(typename std::vector<std::unique_ptr<std::pair<K, V>>>::const_iterator item_it,
                        const async_map* map,
                        std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock = nullptr)
            : item_it(item_it), map(map), shared_lock_ptr(existing_shared_lock)
         {
            // Acquire a shared lock only if no lock is provided
            if (!shared_lock_ptr) {
               shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(map->mutex);
            }
         }

         // Copy Constructor
         const_iterator(const const_iterator& other)
            : item_it(other.item_it), map(other.map), shared_lock_ptr(other.shared_lock_ptr)
         {}

         // Move Constructor
         const_iterator(const_iterator&& other) noexcept
            : item_it(std::move(other.item_it)), map(other.map), shared_lock_ptr(std::move(other.shared_lock_ptr))
         {}

         // Copy Assignment
         const_iterator& operator=(const const_iterator& other)
         {
            if (this != &other) {
               item_it = other.item_it;
               map = other.map;
               shared_lock_ptr = other.shared_lock_ptr;
            }
            return *this;
         }

         // Pre-increment
         const_iterator& operator++()
         {
            ++item_it;
            return *this;
         }

         // Post-increment
         const_iterator operator++(int)
         {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
         }

         reference operator*() const { return *(*item_it); }

         pointer operator->() const { return (*item_it).get(); }

         // Equality Comparison
         bool operator==(const const_iterator& other) const { return item_it == other.item_it; }

         // Inequality Comparison
         bool operator!=(const const_iterator& other) const { return !(*this == other); }

         // Comparison operators with const_iterator
         bool operator<(const const_iterator& other) const { return item_it < other.item_it; }
         bool operator>(const const_iterator& other) const { return item_it > other.item_it; }
         bool operator<=(const const_iterator& other) const { return item_it <= other.item_it; }
         bool operator>=(const const_iterator& other) const { return item_it >= other.item_it; }

         // Comparison operators with iterator
         bool operator<(const iterator& other) const { return item_it < other.item_it; }
         bool operator>(const iterator& other) const { return item_it > other.item_it; }
         bool operator<=(const iterator& other) const { return item_it <= other.item_it; }
         bool operator>=(const iterator& other) const { return item_it >= other.item_it; }
      };

      // Value Proxy Class Definition
      class value_proxy
      {
        private:
         value_type& value_ref;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr;

        public:
         value_proxy(value_type& value_ref, std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock,
                     std::shared_ptr<std::unique_lock<std::shared_mutex>> existing_unique_lock = nullptr)
            : value_ref(value_ref), shared_lock_ptr(existing_shared_lock), unique_lock_ptr(existing_unique_lock)
         {
            // Ensure that a lock is provided
            assert(shared_lock_ptr || unique_lock_ptr);
         }

         // Disable Copy and Move
         value_proxy(const value_proxy&) = delete;
         value_proxy& operator=(const value_proxy&) = delete;
         value_proxy(value_proxy&&) = delete;
         value_proxy& operator=(value_proxy&&) = delete;

         // Access the value
         V& value() { return value_ref.second; }

         const V& value() const { return value_ref.second; }

         // Arrow Operator
         value_type* operator->() { return &value_ref; }

         // Implicit Conversion to V&
         operator V&() { return value_ref.second; }

         bool operator==(const V& other) const { return value() == other; }
      };

      // Const Value Proxy Class Definition
      class const_value_proxy
      {
        private:
         const_value_type& value_ref;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;

        public:
         const_value_proxy(const_value_type& value_ref,
                           std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock)
            : value_ref(value_ref), shared_lock_ptr(existing_shared_lock)
         {
            // Ensure that a lock is provided
            assert(shared_lock_ptr);
         }

         // Disable Copy and Move
         const_value_proxy(const const_value_proxy&) = delete;
         const_value_proxy& operator=(const const_value_proxy&) = delete;
         const_value_proxy(const_value_proxy&&) = delete;
         const_value_proxy& operator=(const_value_proxy&&) = delete;

         // Access the value
         const V& value() const { return value_ref.second; }

         // Arrow Operator
         const_value_type* operator->() const { return &value_ref; }

         // Implicit Conversion to const V&
         operator const V&() const { return value_ref.second; }

         bool operator==(const V& other) const { return value() == other; }
      };

      // Insert method behaves like std::map::insert
      std::pair<iterator, bool> insert(const std::pair<K, V>& pair)
      {
         auto unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(pair.first);

         if (found) {
            return {iterator(it, this, nullptr, unique_lock_ptr), false};
         }
         else {
            // Insert while maintaining sorted order
            it = items.insert(it, std::make_unique<std::pair<K, V>>(pair));
            return {iterator(it, this, nullptr, unique_lock_ptr), true};
         }
      }

      template <class Key, class Value>
      std::pair<iterator, bool> emplace(Key&& key, Value&& value)
      {
         auto unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            return {iterator(it, this, nullptr, unique_lock_ptr), false};
         }
         else {
            // Insert while maintaining sorted order
            it =
               items.insert(it, std::make_unique<std::pair<K, V>>(std::forward<Key>(key), std::forward<Value>(value)));
            return {iterator(it, this, nullptr, unique_lock_ptr), true};
         }
      }

      // Emplace method
      template <typename... Args>
      std::pair<iterator, bool> emplace(const K& key, Args&&... args)
      {
         auto unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            return {iterator(it, this, nullptr, unique_lock_ptr), false};
         }
         else {
            // Construct value in place while maintaining sorted order
            it =
               items.insert(it, std::make_unique<std::pair<K, V>>(std::piecewise_construct, std::forward_as_tuple(key),
                                                                  std::forward_as_tuple(std::forward<Args>(args)...)));
            return {iterator(it, this, nullptr, unique_lock_ptr), true};
         }
      }

      // Try_emplace method
      template <typename... Args>
      std::pair<iterator, bool> try_emplace(const K& key, Args&&... args)
      {
         return emplace(key, std::forward<Args>(args)...);
      }

      // Clear all elements
      void clear()
      {
         std::unique_lock lock(mutex);
         items.clear();
      }

      // Erase a key
      void erase(const K& key)
      {
         std::unique_lock lock(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            items.erase(it);
         }
      }

      // Find an iterator to the key
      iterator find(const K& key)
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            return iterator(it, this, shared_lock_ptr);
         }
         else {
            return end();
         }
      }

      // Const version of find
      const_iterator find(const K& key) const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            return const_iterator(it, this, shared_lock_ptr);
         }
         else {
            return end();
         }
      }

      // Access element with bounds checking (non-const)
      value_proxy at(const K& key)
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            return value_proxy(*(*it), shared_lock_ptr);
         }
         else {
            throw std::out_of_range("Key not found");
         }
      }

      // Access element with bounds checking (const)
      const_value_proxy at(const K& key) const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            return const_value_proxy(*(*it), shared_lock_ptr);
         }
         else {
            throw std::out_of_range("Key not found");
         }
      }

      // Begin iterator (non-const)
      iterator begin()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return iterator(items.begin(), this, shared_lock_ptr);
      }

      // End iterator (non-const)
      iterator end()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return iterator(items.end(), this, shared_lock_ptr);
      }

      // Begin iterator (const)
      const_iterator begin() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(items.cbegin(), this, shared_lock_ptr);
      }

      // End iterator (const)
      const_iterator end() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(items.cend(), this, shared_lock_ptr);
      }

      // Count the number of elements with the given key (0 or 1)
      size_t count(const K& key) const
      {
         std::shared_lock lock(mutex);
         auto [it, found] = binary_search_key(key);
         return found ? 1 : 0;
      }

      size_t size() const
      {
         std::shared_lock lock(mutex);
         return items.size();
      }

      // Check if the map contains the key
      bool contains(const K& key) const
      {
         std::shared_lock lock(mutex);
         auto [it, found] = binary_search_key(key);
         return found;
      }
   };
}
