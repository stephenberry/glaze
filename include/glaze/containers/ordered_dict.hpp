// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#endif
#endif

// A generic ordered dictionary using robin hood hashing with open addressing.
// Preserves insertion order via a contiguous std::vector of key-value pairs.
// Provides O(1) average lookup, insert, and unordered erase.
// Ordered erase is O(n) because it shifts elements to maintain order.
//
// Matches the tsl::ordered_map API: insert, insert_or_assign, try_emplace,
// emplace, erase, unordered_erase, find, at, operator[], count, contains,
// equal_range, front, back, nth, rehash, reserve, load_factor, etc.

namespace glz
{
   template <class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
             class Allocator = std::allocator<std::pair<Key, T>>>
   struct ordered_dict
   {
      using key_type = Key;
      using mapped_type = T;
      using value_type = std::pair<Key, T>;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using hasher = Hash;
      using key_equal = KeyEqual;
      using allocator_type = Allocator;
      using reference = value_type&;
      using const_reference = const value_type&;
      using pointer = value_type*;
      using const_pointer = const value_type*;

      using values_container_type = std::vector<value_type, Allocator>;
      using iterator = typename values_container_type::iterator;
      using const_iterator = typename values_container_type::const_iterator;
      using reverse_iterator = typename values_container_type::reverse_iterator;
      using const_reverse_iterator = typename values_container_type::const_reverse_iterator;

     private:
      struct bucket_entry
      {
         uint32_t index;
         uint32_t stored_hash;
      };

      static constexpr uint32_t empty_marker = std::numeric_limits<uint32_t>::max();
      static constexpr uint32_t min_bucket_count = 8;
      static constexpr float default_max_load_factor = 0.75f;

      values_container_type values_;
      bucket_entry* buckets_ = nullptr;
      uint32_t bucket_count_ = 0;
      uint32_t bucket_mask_ = 0;
      uint32_t load_threshold_ = 0;
      float max_load_factor_ = default_max_load_factor;

      [[no_unique_address]] Hash hash_;
      [[no_unique_address]] KeyEqual equal_;

      // --- Hash helpers ---

      static uint32_t to_stored_hash(size_t h) noexcept { return static_cast<uint32_t>(h); }

      uint32_t bucket_for_hash(uint32_t stored) const noexcept { return stored & bucket_mask_; }

      uint32_t distance_from_ideal(uint32_t actual, uint32_t stored) const noexcept
      {
         return (actual - bucket_for_hash(stored)) & bucket_mask_;
      }

      static uint32_t round_up_pow2(uint32_t v) noexcept
      {
         if (v == 0) return 0;
         --v;
         v |= v >> 1;
         v |= v >> 2;
         v |= v >> 4;
         v |= v >> 8;
         v |= v >> 16;
         return v + 1;
      }

      // --- Bucket memory ---

      void allocate_buckets()
      {
         if (bucket_count_ == 0) return;
         buckets_ = static_cast<bucket_entry*>(std::malloc(bucket_count_ * sizeof(bucket_entry)));
         if (!buckets_) GLZ_THROW_OR_ABORT(std::bad_alloc{});
         clear_buckets();
      }

      void deallocate_buckets() noexcept
      {
         std::free(buckets_);
         buckets_ = nullptr;
      }

      // Sets all bucket indices to empty_marker (0xFF bytes = 0xFFFFFFFF for each uint32_t)
      void clear_buckets() noexcept { std::memset(buckets_, 0xFF, bucket_count_ * sizeof(bucket_entry)); }

      // --- Core robin hood operations ---

      // Insert a bucket entry without checking for duplicates (used during rehash)
      void insert_into_buckets(bucket_entry entry) noexcept
      {
         uint32_t idx = bucket_for_hash(entry.stored_hash);
         uint32_t dist = 0;

         while (true) {
            auto& b = buckets_[idx];
            if (b.index == empty_marker) {
               b = entry;
               return;
            }
            uint32_t existing_dist = distance_from_ideal(idx, b.stored_hash);
            if (existing_dist < dist) {
               std::swap(b, entry);
               dist = existing_dist;
            }
            idx = (idx + 1) & bucket_mask_;
            ++dist;
         }
      }

      // Find the bucket index for a given key. Returns bucket_count_ if not found.
      template <class K>
      uint32_t find_bucket(const K& key) const noexcept
      {
         if (bucket_count_ == 0) return bucket_count_;

         const size_t h = hash_(key);
         const uint32_t stored = to_stored_hash(h);
         uint32_t idx = bucket_for_hash(stored);
         uint32_t dist = 0;

         while (true) {
            const auto& b = buckets_[idx];
            if (b.index == empty_marker) return bucket_count_;
            uint32_t existing_dist = distance_from_ideal(idx, b.stored_hash);
            if (existing_dist < dist) return bucket_count_;
            if (b.stored_hash == stored && equal_(values_[b.index].first, key)) {
               return idx;
            }
            idx = (idx + 1) & bucket_mask_;
            ++dist;
         }
      }

      // Find the bucket that stores a particular value index
      uint32_t find_bucket_by_value_index(uint32_t value_index) const noexcept
      {
         const auto& key = values_[value_index].first;
         const size_t h = hash_(key);
         const uint32_t stored = to_stored_hash(h);
         uint32_t idx = bucket_for_hash(stored);

         while (true) {
            if (buckets_[idx].index == value_index) return idx;
            idx = (idx + 1) & bucket_mask_;
         }
      }

      // Backward shift deletion: remove the bucket at bucket_idx and shift subsequent entries back
      void erase_from_buckets(uint32_t bucket_idx) noexcept
      {
         uint32_t prev = bucket_idx;
         uint32_t curr = (bucket_idx + 1) & bucket_mask_;

         while (true) {
            auto& cb = buckets_[curr];
            if (cb.index == empty_marker || distance_from_ideal(curr, cb.stored_hash) == 0) {
               buckets_[prev] = {empty_marker, 0};
               return;
            }
            buckets_[prev] = cb;
            prev = curr;
            curr = (curr + 1) & bucket_mask_;
         }
      }

      void grow_and_rehash()
      {
         uint32_t new_count = bucket_count_ == 0 ? min_bucket_count : bucket_count_ * 2;
         rehash_impl(new_count);
      }

      void rehash_impl(uint32_t new_count)
      {
         deallocate_buckets();
         bucket_count_ = new_count;
         bucket_mask_ = bucket_count_ - 1;
         load_threshold_ = static_cast<uint32_t>(static_cast<float>(bucket_count_) * max_load_factor_);
         allocate_buckets();

         for (uint32_t i = 0; i < static_cast<uint32_t>(values_.size()); ++i) {
            const size_t h = hash_(values_[i].first);
            insert_into_buckets({i, to_stored_hash(h)});
         }
      }

      // Insert implementation that handles grow, duplicate check, and robin hood placement.
      // Returns {bucket_idx, stored_hash, found} where found=true means duplicate was found.
      struct insert_result
      {
         uint32_t bucket_idx;
         uint32_t stored_hash;
         bool found;
      };

      template <class K>
      insert_result insert_to_buckets(const K& key)
      {
         if (values_.size() >= load_threshold_) {
            grow_and_rehash();
         }

         const size_t h = hash_(key);
         const uint32_t stored = to_stored_hash(h);
         const uint32_t new_index = static_cast<uint32_t>(values_.size());
         uint32_t idx = bucket_for_hash(stored);
         uint32_t dist = 0;

         bucket_entry entry_to_place = {new_index, stored};
         bool checking_dup = true;

         while (true) {
            auto& b = buckets_[idx];
            if (b.index == empty_marker) {
               b = entry_to_place;
               return {idx, stored, false};
            }

            if (checking_dup && b.stored_hash == stored && equal_(values_[b.index].first, key)) {
               return {idx, stored, true}; // duplicate
            }

            uint32_t existing_dist = distance_from_ideal(idx, b.stored_hash);
            if (existing_dist < dist) {
               std::swap(b, entry_to_place);
               dist = existing_dist;
               checking_dup = false; // displaced entries are already in the table
            }

            idx = (idx + 1) & bucket_mask_;
            ++dist;
         }
      }

     public:
      // --- Constructors ---

      ordered_dict() = default;

      explicit ordered_dict(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(),
                            const Allocator& alloc = Allocator())
         : values_(alloc), max_load_factor_(default_max_load_factor), hash_(hash), equal_(equal)
      {
         if (bucket_count > 0) {
            auto bc = round_up_pow2(static_cast<uint32_t>(std::max(bucket_count, size_type(min_bucket_count))));
            bucket_count_ = bc;
            bucket_mask_ = bc - 1;
            load_threshold_ = static_cast<uint32_t>(static_cast<float>(bc) * max_load_factor_);
            allocate_buckets();
         }
      }

      template <class InputIt>
      ordered_dict(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(),
                   const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
         : ordered_dict(bucket_count, hash, equal, alloc)
      {
         insert(first, last);
      }

      ordered_dict(std::initializer_list<value_type> init, size_type bucket_count = 0, const Hash& hash = Hash(),
                   const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
         : ordered_dict(bucket_count, hash, equal, alloc)
      {
         insert(init.begin(), init.end());
      }

      ordered_dict(const ordered_dict& other) : values_(other.values_), hash_(other.hash_), equal_(other.equal_)
      {
         if (!values_.empty()) {
            auto bc = round_up_pow2(
               static_cast<uint32_t>(std::max(size_type(min_bucket_count),
                                              static_cast<size_type>(static_cast<float>(values_.size()) / max_load_factor_) + 1)));
            bucket_count_ = bc;
            bucket_mask_ = bc - 1;
            load_threshold_ = static_cast<uint32_t>(static_cast<float>(bc) * max_load_factor_);
            allocate_buckets();
            for (uint32_t i = 0; i < static_cast<uint32_t>(values_.size()); ++i) {
               insert_into_buckets({i, to_stored_hash(hash_(values_[i].first))});
            }
         }
      }

      ordered_dict(ordered_dict&& other) noexcept
         : values_(std::move(other.values_)),
           buckets_(other.buckets_),
           bucket_count_(other.bucket_count_),
           bucket_mask_(other.bucket_mask_),
           load_threshold_(other.load_threshold_),
           max_load_factor_(other.max_load_factor_),
           hash_(std::move(other.hash_)),
           equal_(std::move(other.equal_))
      {
         other.buckets_ = nullptr;
         other.bucket_count_ = 0;
         other.bucket_mask_ = 0;
         other.load_threshold_ = 0;
      }

      ~ordered_dict() { deallocate_buckets(); }

      ordered_dict& operator=(const ordered_dict& other)
      {
         if (this != &other) {
            ordered_dict tmp(other);
            swap(tmp);
         }
         return *this;
      }

      ordered_dict& operator=(ordered_dict&& other) noexcept
      {
         if (this != &other) {
            deallocate_buckets();
            values_ = std::move(other.values_);
            buckets_ = other.buckets_;
            bucket_count_ = other.bucket_count_;
            bucket_mask_ = other.bucket_mask_;
            load_threshold_ = other.load_threshold_;
            max_load_factor_ = other.max_load_factor_;
            hash_ = std::move(other.hash_);
            equal_ = std::move(other.equal_);
            other.buckets_ = nullptr;
            other.bucket_count_ = 0;
            other.bucket_mask_ = 0;
            other.load_threshold_ = 0;
         }
         return *this;
      }

      ordered_dict& operator=(std::initializer_list<value_type> ilist)
      {
         clear();
         insert(ilist.begin(), ilist.end());
         return *this;
      }

      // --- Iterators ---

      iterator begin() noexcept { return values_.begin(); }
      const_iterator begin() const noexcept { return values_.begin(); }
      const_iterator cbegin() const noexcept { return values_.cbegin(); }

      iterator end() noexcept { return values_.end(); }
      const_iterator end() const noexcept { return values_.end(); }
      const_iterator cend() const noexcept { return values_.cend(); }

      reverse_iterator rbegin() noexcept { return values_.rbegin(); }
      const_reverse_iterator rbegin() const noexcept { return values_.rbegin(); }
      const_reverse_iterator crbegin() const noexcept { return values_.crbegin(); }

      reverse_iterator rend() noexcept { return values_.rend(); }
      const_reverse_iterator rend() const noexcept { return values_.rend(); }
      const_reverse_iterator crend() const noexcept { return values_.crend(); }

      // --- Capacity ---

      [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
      size_type size() const noexcept { return values_.size(); }
      size_type max_size() const noexcept { return empty_marker - 1; }
      size_type capacity() const noexcept { return values_.capacity(); }

      void shrink_to_fit()
      {
         values_.shrink_to_fit();
         // Optionally rehash to minimal bucket count
         if (bucket_count_ > 0) {
            auto needed = round_up_pow2(
               static_cast<uint32_t>(std::max(size_type(min_bucket_count),
                                              static_cast<size_type>(static_cast<float>(values_.size()) / max_load_factor_) + 1)));
            if (needed < bucket_count_) {
               rehash_impl(needed);
            }
         }
      }

      // --- Modifiers ---

      void clear() noexcept
      {
         values_.clear();
         if (buckets_) clear_buckets();
      }

      std::pair<iterator, bool> insert(const value_type& value)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(value.first);
         if (found) {
            return {values_.begin() + buckets_[bucket_idx].index, false};
         }
         values_.push_back(value);
         return {values_.end() - 1, true};
      }

      std::pair<iterator, bool> insert(value_type&& value)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(value.first);
         if (found) {
            return {values_.begin() + buckets_[bucket_idx].index, false};
         }
         values_.push_back(std::move(value));
         return {values_.end() - 1, true};
      }

      template <class InputIt>
      void insert(InputIt first, InputIt last)
      {
         for (; first != last; ++first) {
            insert(*first);
         }
      }

      void insert(std::initializer_list<value_type> ilist) { insert(ilist.begin(), ilist.end()); }

      template <class M>
      std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(key);
         if (found) {
            auto it = values_.begin() + buckets_[bucket_idx].index;
            it->second = std::forward<M>(obj);
            return {it, false};
         }
         values_.emplace_back(key, std::forward<M>(obj));
         return {values_.end() - 1, true};
      }

      template <class M>
      std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(key);
         if (found) {
            auto it = values_.begin() + buckets_[bucket_idx].index;
            it->second = std::forward<M>(obj);
            return {it, false};
         }
         values_.emplace_back(std::move(key), std::forward<M>(obj));
         return {values_.end() - 1, true};
      }

      template <class... Args>
      std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(key);
         if (found) {
            return {values_.begin() + buckets_[bucket_idx].index, false};
         }
         values_.emplace_back(std::piecewise_construct, std::forward_as_tuple(key),
                              std::forward_as_tuple(std::forward<Args>(args)...));
         return {values_.end() - 1, true};
      }

      template <class... Args>
      std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(key);
         if (found) {
            return {values_.begin() + buckets_[bucket_idx].index, false};
         }
         // key may have been used for hashing but insert_to_buckets doesn't move it
         values_.emplace_back(std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                              std::forward_as_tuple(std::forward<Args>(args)...));
         return {values_.end() - 1, true};
      }

      template <class... Args>
      std::pair<iterator, bool> emplace(Args&&... args)
      {
         value_type value(std::forward<Args>(args)...);
         return insert(std::move(value));
      }

      // Ordered erase: preserves insertion order. O(n) because it shifts elements.
      iterator erase(const_iterator pos)
      {
         const auto erased_idx = static_cast<uint32_t>(pos - values_.cbegin());

         // Remove from bucket array
         uint32_t bi = find_bucket_by_value_index(erased_idx);
         erase_from_buckets(bi);

         // Erase from values vector
         values_.erase(values_.begin() + erased_idx);

         // Update all bucket indices > erased_idx (they shifted down by 1)
         for (uint32_t i = 0; i < bucket_count_; ++i) {
            if (buckets_[i].index != empty_marker && buckets_[i].index > erased_idx) {
               --buckets_[i].index;
            }
         }

         return values_.begin() + erased_idx;
      }

      iterator erase(const_iterator first, const_iterator last)
      {
         if (first == last) return values_.begin() + (first - values_.cbegin());

         // Erase from back to front to avoid invalidation issues
         auto start_idx = static_cast<size_t>(first - values_.cbegin());
         auto end_idx = static_cast<size_t>(last - values_.cbegin());
         auto count = end_idx - start_idx;

         // Remove all affected entries from buckets
         for (auto i = start_idx; i < end_idx; ++i) {
            uint32_t bi = find_bucket_by_value_index(static_cast<uint32_t>(i));
            erase_from_buckets(bi);
         }

         // Erase from values
         values_.erase(values_.begin() + start_idx, values_.begin() + end_idx);

         // Update all bucket indices that pointed past the erased range
         for (uint32_t i = 0; i < bucket_count_; ++i) {
            if (buckets_[i].index != empty_marker && buckets_[i].index >= end_idx) {
               buckets_[i].index -= static_cast<uint32_t>(count);
            }
         }

         return values_.begin() + start_idx;
      }

      size_type erase(const key_type& key)
      {
         auto it = find(key);
         if (it != end()) {
            erase(it);
            return 1;
         }
         return 0;
      }

      // Unordered erase: O(1) amortized. Swaps erased element with last, does NOT preserve insertion order.
      iterator unordered_erase(const_iterator pos)
      {
         const auto erased_idx = static_cast<uint32_t>(pos - values_.cbegin());
         const auto last_idx = static_cast<uint32_t>(values_.size() - 1);

         // Remove erased entry from buckets
         uint32_t bi = find_bucket_by_value_index(erased_idx);
         erase_from_buckets(bi);

         if (erased_idx != last_idx) {
            // Update bucket for the last element to point to erased_idx
            uint32_t last_bi = find_bucket_by_value_index(last_idx);
            buckets_[last_bi].index = erased_idx;

            // Move last element into erased position
            values_[erased_idx] = std::move(values_[last_idx]);
         }

         values_.pop_back();
         return values_.begin() + erased_idx;
      }

      size_type unordered_erase(const key_type& key)
      {
         auto it = find(key);
         if (it != end()) {
            unordered_erase(it);
            return 1;
         }
         return 0;
      }

      void swap(ordered_dict& other) noexcept
      {
         values_.swap(other.values_);
         std::swap(buckets_, other.buckets_);
         std::swap(bucket_count_, other.bucket_count_);
         std::swap(bucket_mask_, other.bucket_mask_);
         std::swap(load_threshold_, other.load_threshold_);
         std::swap(max_load_factor_, other.max_load_factor_);
         std::swap(hash_, other.hash_);
         std::swap(equal_, other.equal_);
      }

      // --- Lookup ---

      template <class K>
      iterator find(const K& key)
      {
         uint32_t bi = find_bucket(key);
         if (bi == bucket_count_) return values_.end();
         return values_.begin() + buckets_[bi].index;
      }

      template <class K>
      const_iterator find(const K& key) const
      {
         uint32_t bi = find_bucket(key);
         if (bi == bucket_count_) return values_.end();
         return values_.begin() + buckets_[bi].index;
      }

      template <class K>
      mapped_type& at(const K& key)
      {
         auto it = find(key);
         if (it == end()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("ordered_dict::at: key not found"));
         }
         return it->second;
      }

      template <class K>
      const mapped_type& at(const K& key) const
      {
         auto it = find(key);
         if (it == end()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("ordered_dict::at: key not found"));
         }
         return it->second;
      }

      mapped_type& operator[](const key_type& key)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(key);
         if (found) {
            return values_[buckets_[bucket_idx].index].second;
         }
         values_.emplace_back(key, mapped_type{});
         return values_.back().second;
      }

      mapped_type& operator[](key_type&& key)
      {
         auto [bucket_idx, stored, found] = insert_to_buckets(key);
         if (found) {
            return values_[buckets_[bucket_idx].index].second;
         }
         values_.emplace_back(std::move(key), mapped_type{});
         return values_.back().second;
      }

      template <class K>
      size_type count(const K& key) const
      {
         return find(key) != end() ? 1 : 0;
      }

      template <class K>
      bool contains(const K& key) const
      {
         return find(key) != end();
      }

      template <class K>
      std::pair<iterator, iterator> equal_range(const K& key)
      {
         auto it = find(key);
         if (it == end()) return {it, it};
         return {it, std::next(it)};
      }

      template <class K>
      std::pair<const_iterator, const_iterator> equal_range(const K& key) const
      {
         auto it = find(key);
         if (it == end()) return {it, it};
         return {it, std::next(it)};
      }

      // --- Ordered access ---

      reference front() { return values_.front(); }
      const_reference front() const { return values_.front(); }

      reference back() { return values_.back(); }
      const_reference back() const { return values_.back(); }

      iterator nth(size_type n) { return values_.begin() + n; }
      const_iterator nth(size_type n) const { return values_.begin() + n; }

      value_type* data() noexcept { return values_.data(); }
      const value_type* data() const noexcept { return values_.data(); }

      const values_container_type& values() const noexcept { return values_; }

      // --- Hash policy ---

      float load_factor() const noexcept
      {
         if (bucket_count_ == 0) return 0.0f;
         return static_cast<float>(values_.size()) / static_cast<float>(bucket_count_);
      }

      float max_load_factor() const noexcept { return max_load_factor_; }

      void max_load_factor(float ml)
      {
         max_load_factor_ = std::clamp(ml, 0.1f, 0.95f);
         load_threshold_ = static_cast<uint32_t>(static_cast<float>(bucket_count_) * max_load_factor_);
      }

      void rehash(size_type count)
      {
         auto needed = static_cast<size_type>(static_cast<float>(values_.size()) / max_load_factor_) + 1;
         count = std::max(count, needed);
         auto bc = round_up_pow2(static_cast<uint32_t>(std::max(count, size_type(min_bucket_count))));
         if (bc != bucket_count_) {
            rehash_impl(bc);
         }
      }

      void reserve(size_type count)
      {
         values_.reserve(count);
         auto needed = static_cast<size_type>(static_cast<float>(count) / max_load_factor_) + 1;
         auto bc = round_up_pow2(static_cast<uint32_t>(std::max(needed, size_type(min_bucket_count))));
         if (bc > bucket_count_) {
            rehash_impl(bc);
         }
      }

      size_type bucket_count() const noexcept { return bucket_count_; }

      // --- Observers ---

      hasher hash_function() const { return hash_; }
      key_equal key_eq() const { return equal_; }

      // --- Comparison ---

      bool operator==(const ordered_dict& other) const { return values_ == other.values_; }
      bool operator!=(const ordered_dict& other) const { return values_ != other.values_; }
   };

   template <class Key, class T, class Hash, class KeyEqual, class Allocator>
   void swap(ordered_dict<Key, T, Hash, KeyEqual, Allocator>& lhs,
             ordered_dict<Key, T, Hash, KeyEqual, Allocator>& rhs) noexcept
   {
      lhs.swap(rhs);
   }
}
