// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "glaze/hash/sweethash.hpp"

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#endif
#endif

// An ordered_map optimized for JSON objects with string keys.
// - Preserves insertion order for iteration
// - Uses linear search for small maps (≤16 entries)
// - Lazily builds a sorted index for larger maps (O(log n) lookup, 8 bytes per entry)

namespace glz
{
   template <class T>
   struct ordered_map
   {
      using key_type = std::string;
      using mapped_type = T;
      using value_type = std::pair<std::string, T>;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using reference = value_type&;
      using const_reference = const value_type&;
      using iterator = typename std::vector<value_type>::iterator;
      using const_iterator = typename std::vector<value_type>::const_iterator;
      using reverse_iterator = typename std::vector<value_type>::reverse_iterator;
      using const_reverse_iterator = typename std::vector<value_type>::const_reverse_iterator;

     private:
      std::vector<value_type> data_;

      // Sorted index for fast lookup on large maps
      // Stores (hash, index) pairs sorted by hash - 8 bytes per entry
      // Colliding hashes are adjacent; lookup scans neighbors and compares actual keys
      // Maintained incrementally: new entries are insert-sorted into position.
      mutable std::vector<std::pair<uint32_t, uint32_t>> index_;
      mutable uint32_t index_size_ = 0; // number of data_ elements covered by the index (0 = fully invalid)

      static constexpr size_type linear_search_threshold = 8;

      static uint32_t hash_key(std::string_view key) noexcept { return sweethash::sweet32(key); }

      // Full invalidation - used by erase (indices shift) and clear
      void invalidate_index() noexcept { index_size_ = 0; }

      void rebuild_index() const
      {
         index_.clear();
         index_.reserve(data_.size());
         for (uint32_t i = 0; i < static_cast<uint32_t>(data_.size()); ++i) {
            index_.emplace_back(hash_key(data_[i].first), i);
         }

         std::sort(index_.begin(), index_.end(),
                   [](const auto& a, const auto& b) { return a.first < b.first; });

         index_size_ = static_cast<uint32_t>(data_.size());
      }

      // Bring the index up to date. If the index covers some elements,
      // incrementally insert-sort only the new ones. If fully invalid, rebuild.
      void ensure_index() const
      {
         if (data_.size() <= linear_search_threshold) return;
         const auto n = static_cast<uint32_t>(data_.size());
         if (index_size_ == n) return; // fully current

         if (index_size_ == 0) {
            rebuild_index(); // full rebuild after erase or first time
            return;
         }

         // Incrementally insert the new entries
         for (uint32_t i = index_size_; i < n; ++i) {
            const auto entry = std::pair{hash_key(data_[i].first), i};
            const auto* base = index_.data();
            const auto* p = branchless_lower_bound(base, index_.size(), entry.first);
            auto pos = static_cast<size_t>(p - base);
            index_.insert(index_.begin() + pos, entry);
         }
         index_size_ = n;
      }

      // Insert into the sorted index at a known position and update bookkeeping
      void index_insert_at(size_t pos, uint32_t hash, uint32_t data_idx) const
      {
         index_.insert(index_.begin() + pos, {hash, data_idx});
         index_size_ = static_cast<uint32_t>(data_.size());
      }

      // Result of a combined find + insertion-point search
      struct index_find_result
      {
         iterator it; // found iterator, or end() if not found
         size_t insert_pos; // index position for insertion (valid only when it == end())
         uint32_t hash; // precomputed hash of the key
      };

      // Single binary search that returns both the lookup result and
      // the index insertion position, avoiding a redundant second search.
      template <class K>
      index_find_result index_find_or_pos(const K& key)
      {
         ensure_index();

         const uint32_t h = hash_key(key);
         const auto* base = index_.data();
         const auto* idx_end = base + index_.size();
         const auto* p = branchless_lower_bound(base, index_.size(), h);
         const size_t pos = static_cast<size_t>(p - base);

         // Scan adjacent entries with matching hash
         for (auto* scan = p; scan != idx_end && scan->first == h; ++scan) {
            if (data_[scan->second].first == key) {
               return {data_.begin() + scan->second, pos, h};
            }
         }

         return {data_.end(), pos, h};
      }

      // Linear search - used for small maps
      template <class K>
      iterator linear_find(const K& key)
      {
         for (auto it = data_.begin(); it != data_.end(); ++it) {
            if (it->first == key) return it;
         }
         return data_.end();
      }

      template <class K>
      const_iterator linear_find(const K& key) const
      {
         for (auto it = data_.begin(); it != data_.end(); ++it) {
            if (it->first == key) return it;
         }
         return data_.end();
      }

      // Branchless binary search - compiles to cmov instead of conditional branches
      static const std::pair<uint32_t, uint32_t>* branchless_lower_bound(
         const std::pair<uint32_t, uint32_t>* p, size_t len, uint32_t target) noexcept
      {
         while (len > 1) {
            size_t half = len / 2;
            p += (p[half - 1].first < target) * half;
            len -= half;
         }
         // Final element check: advance past it if it's less than target
         p += (len == 1 && p->first < target);
         return p;
      }

      // Binary search using hash index
      // On hash match, scans adjacent entries to handle collisions
      template <class K>
      iterator index_find(const K& key)
      {
         ensure_index();

         const uint32_t h = hash_key(key);
         const auto* base = index_.data();
         const auto* idx_end = base + index_.size();
         const auto* p = branchless_lower_bound(base, index_.size(), h);

         for (; p != idx_end && p->first == h; ++p) {
            if (data_[p->second].first == key) {
               return data_.begin() + p->second;
            }
         }
         return data_.end();
      }

      template <class K>
      const_iterator index_find(const K& key) const
      {
         ensure_index();

         const uint32_t h = hash_key(key);
         const auto* base = index_.data();
         const auto* idx_end = base + index_.size();
         const auto* p = branchless_lower_bound(base, index_.size(), h);

         for (; p != idx_end && p->first == h; ++p) {
            if (data_[p->second].first == key) {
               return data_.begin() + p->second;
            }
         }
         return data_.end();
      }

     public:
      // Constructors
      ordered_map() = default;

      ordered_map(std::initializer_list<value_type> init)
      {
         data_.reserve(init.size());
         for (const auto& pair : init) {
            if (linear_find(pair.first) == data_.end()) {
               data_.push_back(pair);
            }
         }
      }

      template <class InputIt>
      ordered_map(InputIt first, InputIt last)
      {
         for (; first != last; ++first) {
            insert(*first);
         }
      }

      ordered_map(const ordered_map& other) : data_(other.data_)
      {
         // Don't copy index - it will be rebuilt lazily with correct string_views
      }

      ordered_map(ordered_map&& other) noexcept : data_(std::move(other.data_))
      {
         // Don't move index - string_views would point to moved-from strings
         other.invalidate_index();
      }

      ordered_map& operator=(const ordered_map& other)
      {
         if (this != &other) {
            data_ = other.data_;
            invalidate_index();
         }
         return *this;
      }

      ordered_map& operator=(ordered_map&& other) noexcept
      {
         if (this != &other) {
            data_ = std::move(other.data_);
            invalidate_index();
            other.invalidate_index();
         }
         return *this;
      }

      // Iterators (follow insertion order)
      iterator begin() noexcept { return data_.begin(); }
      const_iterator begin() const noexcept { return data_.begin(); }
      const_iterator cbegin() const noexcept { return data_.cbegin(); }

      iterator end() noexcept { return data_.end(); }
      const_iterator end() const noexcept { return data_.end(); }
      const_iterator cend() const noexcept { return data_.cend(); }

      reverse_iterator rbegin() noexcept { return data_.rbegin(); }
      const_reverse_iterator rbegin() const noexcept { return data_.rbegin(); }
      const_reverse_iterator crbegin() const noexcept { return data_.crbegin(); }

      reverse_iterator rend() noexcept { return data_.rend(); }
      const_reverse_iterator rend() const noexcept { return data_.rend(); }
      const_reverse_iterator crend() const noexcept { return data_.crend(); }

      // Capacity
      [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
      size_type size() const noexcept { return data_.size(); }
      size_type capacity() const noexcept { return data_.capacity(); }
      void reserve(size_type new_cap) { data_.reserve(new_cap); }
      void shrink_to_fit() { data_.shrink_to_fit(); }

      // Modifiers
      void clear() noexcept
      {
         data_.clear();
         index_.clear();
         index_size_ = 0;
      }

      std::pair<iterator, bool> insert(const value_type& value)
      {
         if (data_.size() <= linear_search_threshold) {
            auto it = linear_find(value.first);
            if (it != end()) return {it, false};
            data_.push_back(value);
            return {data_.end() - 1, true};
         }
         auto [it, pos, h] = index_find_or_pos(value.first);
         if (it != end()) return {it, false};
         data_.push_back(value);
         if (index_size_ > 0) {
            index_insert_at(pos, h, static_cast<uint32_t>(data_.size() - 1));
         }
         return {data_.end() - 1, true};
      }

      std::pair<iterator, bool> insert(value_type&& value)
      {
         if (data_.size() <= linear_search_threshold) {
            auto it = linear_find(value.first);
            if (it != end()) return {it, false};
            data_.push_back(std::move(value));
            return {data_.end() - 1, true};
         }
         auto [it, pos, h] = index_find_or_pos(value.first);
         if (it != end()) return {it, false};
         data_.push_back(std::move(value));
         if (index_size_ > 0) {
            index_insert_at(pos, h, static_cast<uint32_t>(data_.size() - 1));
         }
         return {data_.end() - 1, true};
      }

      template <class InputIt>
      void insert(InputIt first, InputIt last)
      {
         for (; first != last; ++first) {
            insert(*first);
         }
      }

      void insert(std::initializer_list<value_type> ilist) { insert(ilist.begin(), ilist.end()); }

      template <class K, class... Args>
      std::pair<iterator, bool> emplace(K&& key, Args&&... args)
      {
         if (data_.size() <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return {it, false};
            data_.emplace_back(std::forward<K>(key), T(std::forward<Args>(args)...));
            return {data_.end() - 1, true};
         }
         auto [it, pos, h] = index_find_or_pos(key);
         if (it != end()) return {it, false};
         data_.emplace_back(std::forward<K>(key), T(std::forward<Args>(args)...));
         if (index_size_ > 0) {
            index_insert_at(pos, h, static_cast<uint32_t>(data_.size() - 1));
         }
         return {data_.end() - 1, true};
      }

      iterator erase(const_iterator pos)
      {
         invalidate_index();
         return data_.erase(pos);
      }

      iterator erase(const_iterator first, const_iterator last)
      {
         invalidate_index();
         return data_.erase(first, last);
      }

      size_type erase(std::string_view key)
      {
         auto it = find(key);
         if (it != end()) {
            invalidate_index();
            data_.erase(it);
            return 1;
         }
         return 0;
      }

      // Lookup
      template <class K>
      iterator find(const K& key)
      {
         if (data_.size() <= linear_search_threshold) {
            return linear_find(key);
         }
         return index_find(key);
      }

      template <class K>
      const_iterator find(const K& key) const
      {
         if (data_.size() <= linear_search_threshold) {
            return linear_find(key);
         }
         return index_find(key);
      }

      template <class K>
      bool contains(const K& key) const
      {
         return find(key) != end();
      }

      template <class K>
      size_type count(const K& key) const
      {
         return contains(key) ? 1 : 0;
      }

      // Element access
      mapped_type& operator[](const key_type& key)
      {
         if (data_.size() <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return it->second;
            data_.emplace_back(key, mapped_type{});
            return data_.back().second;
         }
         auto [it, pos, h] = index_find_or_pos(key);
         if (it != end()) return it->second;
         data_.emplace_back(key, mapped_type{});
         if (index_size_ > 0) {
            index_insert_at(pos, h, static_cast<uint32_t>(data_.size() - 1));
         }
         return data_.back().second;
      }

      mapped_type& operator[](key_type&& key)
      {
         if (data_.size() <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return it->second;
            data_.emplace_back(std::move(key), mapped_type{});
            return data_.back().second;
         }
         auto [it, pos, h] = index_find_or_pos(key);
         if (it != end()) return it->second;
         data_.emplace_back(std::move(key), mapped_type{});
         if (index_size_ > 0) {
            index_insert_at(pos, h, static_cast<uint32_t>(data_.size() - 1));
         }
         return data_.back().second;
      }

      // Heterogeneous lookup version for operator[]
      // Only participates when K is convertible to string_view but not string
      template <class K>
         requires(std::convertible_to<K, std::string_view> && !std::same_as<std::decay_t<K>, std::string> &&
                  !std::same_as<std::decay_t<K>, key_type>)
      mapped_type& operator[](K&& key)
      {
         if (data_.size() <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return it->second;
            data_.emplace_back(std::string(std::forward<K>(key)), mapped_type{});
            return data_.back().second;
         }
         auto [it, pos, h] = index_find_or_pos(key);
         if (it != end()) return it->second;
         data_.emplace_back(std::string(std::forward<K>(key)), mapped_type{});
         if (index_size_ > 0) {
            index_insert_at(pos, h, static_cast<uint32_t>(data_.size() - 1));
         }
         return data_.back().second;
      }

      template <class K>
      mapped_type& at(const K& key)
      {
         auto it = find(key);
         if (it == end()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("ordered_map::at: key not found"));
         }
         return it->second;
      }

      template <class K>
      const mapped_type& at(const K& key) const
      {
         auto it = find(key);
         if (it == end()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("ordered_map::at: key not found"));
         }
         return it->second;
      }

      // Direct access to underlying data
      value_type* data() noexcept { return data_.data(); }
      const value_type* data() const noexcept { return data_.data(); }

      // Comparison
      bool operator==(const ordered_map& other) const { return data_ == other.data_; }
      bool operator!=(const ordered_map& other) const { return data_ != other.data_; }
   };
}
