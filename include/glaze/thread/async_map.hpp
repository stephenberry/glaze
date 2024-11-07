// Glaze Library
// For the license information refer to glaze.hpp

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
      std::vector<K> keys;
      std::vector<V> values;
      mutable std::shared_mutex mutex;

      // Helper function to perform binary search.
      // Returns a pair of iterator and a boolean indicating if the key was found.
      std::pair<typename std::vector<K>::const_iterator, bool> binary_search_key(const K& key) const
      {
         auto it = std::lower_bound(keys.cbegin(), keys.cend(), key);
         if (it != keys.cend() && !(key < *it)) { // Equivalent to key == *it
            return {it, true};
         }
         return {it, false};
      }

     public:
      using key_type = K;
      using mapped_type = V;
      using value_type = std::pair<const K&, V&>;
      using const_value_type = std::pair<const K&, const V&>;

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
         typename std::vector<K>::const_iterator key_it;
         typename std::vector<V>::iterator value_it;
         async_map* map;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr;

         struct proxy
         {
            std::pair<const K&, V&> p;

            std::pair<const K&, V&>* operator->() { return &p; }
         };

        public:
         iterator(typename std::vector<K>::const_iterator key_it, typename std::vector<V>::iterator value_it,
                  async_map* map, std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock = nullptr,
                  std::shared_ptr<std::unique_lock<std::shared_mutex>> existing_unique_lock = nullptr)
            : key_it(key_it),
              value_it(value_it),
              map(map),
              shared_lock_ptr(existing_shared_lock),
              unique_lock_ptr(existing_unique_lock)
         {
            // Acquire a shared lock only if no lock is provided
            if (!shared_lock_ptr && !unique_lock_ptr) {
               shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(map->mutex);
            }
         }

         // Copy Constructor
         iterator(const iterator& other)
            : key_it(other.key_it),
              value_it(other.value_it),
              map(other.map),
              shared_lock_ptr(other.shared_lock_ptr),
              unique_lock_ptr(other.unique_lock_ptr)
         {}

         // Move Constructor
         iterator(iterator&& other) noexcept
            : key_it(std::move(other.key_it)),
              value_it(std::move(other.value_it)),
              map(other.map),
              shared_lock_ptr(std::move(other.shared_lock_ptr)),
              unique_lock_ptr(std::move(other.unique_lock_ptr))
         {}

         // Copy Assignment
         iterator& operator=(const iterator& other)
         {
            if (this != &other) {
               key_it = other.key_it;
               value_it = other.value_it;
               map = other.map;
               shared_lock_ptr = other.shared_lock_ptr;
               unique_lock_ptr = other.unique_lock_ptr;
            }
            return *this;
         }

         // Pre-increment
         iterator& operator++()
         {
            ++key_it;
            ++value_it;
            return *this;
         }

         // Post-increment
         iterator operator++(int)
         {
            iterator tmp(*this);
            ++(*this);
            return tmp;
         }

         value_type operator*() const { return {*key_it, *value_it}; }

         proxy operator->() const { return proxy(*key_it, *value_it); }

         // Equality Comparison
         bool operator==(const iterator& other) const { return key_it == other.key_it; }

         // Inequality Comparison
         bool operator!=(const iterator& other) const { return !(*this == other); }
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
         typename std::vector<K>::const_iterator key_it;
         typename std::vector<V>::const_iterator value_it;
         const async_map* map;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;

         struct proxy
         {
            std::pair<const K&, const V&> p;

            const std::pair<const K&, const V&>* operator->() const { return &p; }
         };

        public:
         const_iterator(typename std::vector<K>::const_iterator key_it,
                        typename std::vector<V>::const_iterator value_it, const async_map* map,
                        std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock = nullptr)
            : key_it(key_it), value_it(value_it), map(map), shared_lock_ptr(existing_shared_lock)
         {
            // Acquire a shared lock only if no lock is provided
            if (!shared_lock_ptr) {
               shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(map->mutex);
            }
         }

         // Copy Constructor
         const_iterator(const const_iterator& other)
            : key_it(other.key_it), value_it(other.value_it), map(other.map), shared_lock_ptr(other.shared_lock_ptr)
         {}

         // Move Constructor
         const_iterator(const_iterator&& other) noexcept
            : key_it(std::move(other.key_it)),
              value_it(std::move(other.value_it)),
              map(other.map),
              shared_lock_ptr(std::move(other.shared_lock_ptr))
         {}

         // Copy Assignment
         const_iterator& operator=(const const_iterator& other)
         {
            if (this != &other) {
               key_it = other.key_it;
               value_it = other.value_it;
               map = other.map;
               shared_lock_ptr = other.shared_lock_ptr;
            }
            return *this;
         }

         // Pre-increment
         const_iterator& operator++()
         {
            ++key_it;
            ++value_it;
            return *this;
         }

         // Post-increment
         const_iterator operator++(int)
         {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
         }

         value_type operator*() const { return {*key_it, *value_it}; }

         proxy operator->() const { return proxy(*key_it, *value_it); }

         // Equality Comparison
         bool operator==(const const_iterator& other) const { return key_it == other.key_it; }

         // Inequality Comparison
         bool operator!=(const const_iterator& other) const { return !(*this == other); }
      };

      // Value Proxy Class Definition
      class value_proxy
      {
        private:
         V& value_ref;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr;

        public:
         value_proxy(V& value_ref, std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock,
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
         V& value() { return value_ref; }

         const V& value() const { return value_ref; }

         // Arrow Operator
         V* operator->() { return &value_ref; }

         // Implicit Conversion to V&
         operator V&() { return value_ref; }

         bool operator==(const V& other) const { return value() == other; }
      };

      // Const Value Proxy Class Definition
      class const_value_proxy
      {
        private:
         const V& value_ref;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;

        public:
         const_value_proxy(const V& value_ref,
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
         const V& value() const { return value_ref; }

         // Arrow Operator
         const V* operator->() const { return &value_ref; }

         // Implicit Conversion to const V&
         operator const V&() const { return value_ref; }

         bool operator==(const V& other) const { return value() == other; }
      };

      // Insert method behaves like std::map::insert
      std::pair<iterator, bool> insert(const std::pair<K, V>& pair)
      {
         auto unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(pair.first);

         if (found) {
            auto index = std::distance(keys.cbegin(), it);
            return {iterator(keys.cbegin() + index, values.begin() + index, this, nullptr, unique_lock_ptr), false};
         }
         else {
            // Insert while maintaining sorted order
            auto index = std::distance(keys.cbegin(), it);
            keys.insert(it, pair.first);
            values.insert(values.begin() + index, pair.second);
            return {iterator(keys.cbegin() + index, values.begin() + index, this, nullptr, unique_lock_ptr), true};
         }
      }

      template <class Key, class Value>
      std::pair<iterator, bool> emplace(Key&& key, Value&& value)
      {
         auto unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            auto index = std::distance(keys.cbegin(), it);
            return {iterator(keys.cbegin() + index, values.begin() + index, this, nullptr, unique_lock_ptr), false};
         }
         else {
            // Insert while maintaining sorted order
            auto index = std::distance(keys.cbegin(), it);
            keys.insert(it, key);
            values.insert(values.begin() + index, std::forward<Value>(value));
            return {iterator(keys.cbegin() + index, values.begin() + index, this, nullptr, unique_lock_ptr), true};
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
            auto index = std::distance(keys.cbegin(), it);
            return {iterator(keys.cbegin() + index, values.begin() + index, this, nullptr, unique_lock_ptr), false};
         }
         else {
            // Insert while maintaining sorted order
            auto index = std::distance(keys.cbegin(), it);
            keys.insert(it, key);
            values.emplace(values.begin() + index, std::forward<Args>(args)...);
            return {iterator(keys.cbegin() + index, values.begin() + index, this, nullptr, unique_lock_ptr), true};
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
         keys.clear();
         values.clear();
      }

      // Erase a key
      void erase(const K& key)
      {
         std::unique_lock lock(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            auto index = std::distance(keys.cbegin(), it);
            keys.erase(it);
            values.erase(values.begin() + index);
         }
      }

      // Find an iterator to the key
      iterator find(const K& key)
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);

         // Perform binary search to find the key
         auto [it, found] = binary_search_key(key);

         if (found) {
            auto index = std::distance(keys.cbegin(), it);
            return iterator(keys.cbegin() + index, values.begin() + index, this, shared_lock_ptr);
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
            auto index = std::distance(keys.cbegin(), it);
            return const_iterator(keys.cbegin() + index, values.cbegin() + index, this, shared_lock_ptr);
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
            auto index = std::distance(keys.cbegin(), it);
            return value_proxy(values[index], shared_lock_ptr);
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
            auto index = std::distance(keys.cbegin(), it);
            return const_value_proxy(values[index], shared_lock_ptr);
         }
         else {
            throw std::out_of_range("Key not found");
         }
      }

      // Begin iterator (non-const)
      iterator begin()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return iterator(keys.cbegin(), values.begin(), this, shared_lock_ptr);
      }

      // End iterator (non-const)
      iterator end()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return iterator(keys.cend(), values.end(), this, shared_lock_ptr);
      }

      // Begin iterator (const)
      const_iterator begin() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(keys.cbegin(), values.cbegin(), this, shared_lock_ptr);
      }

      // End iterator (const)
      const_iterator end() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(keys.cend(), values.cend(), this, shared_lock_ptr);
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
         return keys.size();
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
