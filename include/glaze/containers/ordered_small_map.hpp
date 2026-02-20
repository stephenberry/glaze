// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "glaze/hash/sweethash.hpp"

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#endif
#endif

// An ordered_small_map optimized for JSON objects with string keys.
// Designed for objects with few keys (typically <256), where preserving
// insertion order matters and memory efficiency is important.
// Uses 24 bytes on the stack and ~40 bytes per entry on the heap
// (32 for the key-value pair + 8 for the hash index) —
// significantly less than hash table alternatives.
//
// Design:
// - Preserves insertion order (backed by a contiguous array)
// - Linear search for small maps (≤8 entries) — no heap overhead
// - Lazily builds a sorted hash index for larger maps (O(log n) lookup)
// - Bloom filter accelerates inserts by skipping duplicate checks for new keys

namespace glz
{
   template <class T>
   struct ordered_small_map
   {
      using key_type = std::string;
      using mapped_type = T;
      using value_type = std::pair<std::string, T>;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using reference = value_type&;
      using const_reference = const value_type&;
      using iterator = value_type*;
      using const_iterator = const value_type*;
      using reverse_iterator = std::reverse_iterator<iterator>;
      using const_reverse_iterator = std::reverse_iterator<const_iterator>;

     private:
      // Compact index entry: maps a hash to a position in the data array
      struct hash_index_entry
      {
         uint32_t hash;
         uint32_t index;
      };

      // Bloom filter: 1024 bits (128 bytes) with 2 hash functions.
      // False positive rate at n entries: (1 - e^(-2n/1024))^2
      //   n=32: ~4%   n=64: ~13%   n=128: ~40%   n=256: ~74%
      static constexpr size_t bloom_bytes = 128;
      static constexpr uint32_t bloom_bits = bloom_bytes * 8; // 1024
      static constexpr uint32_t bloom_mask = bloom_bits - 1; // 0x3FF

      // Heap-allocated index block: [index_header][hash_index_entry × capacity]
      // The bloom filter is embedded in the header.
      struct index_header
      {
         uint32_t size; // number of data elements covered by the sorted index (0 = fully invalid)
         uint32_t capacity; // allocated index entry slots
         uint8_t bloom[bloom_bytes]; // bloom filter for fast insert rejection
      };

      // Compact data storage: pointer + uint32 size/capacity = 16 bytes
      // Combined with index_ pointer (8 bytes) = 24 bytes total
      value_type* data_ = nullptr; // 8 bytes
      uint32_t size_ = 0; // 4 bytes
      uint32_t capacity_ = 0; // 4 bytes
      mutable index_header* index_ = nullptr; // 8 bytes
      // Total: 24 bytes

      static constexpr size_type linear_search_threshold = 8;
      static constexpr size_type bloom_threshold = 128; // disable bloom filter above this size

      static uint32_t hash_key(std::string_view key) noexcept { return sweethash::sweet32(key); }

      // --- Data array management ---

      void grow_if_needed()
      {
         if (size_ == capacity_) {
            const uint32_t new_cap = capacity_ ? capacity_ * 2 : 4;
            auto* new_data = static_cast<value_type*>(::operator new(new_cap * sizeof(value_type)));
            // Move existing elements
            for (uint32_t i = 0; i < size_; ++i) {
               std::construct_at(new_data + i, std::move(data_[i]));
               std::destroy_at(data_ + i);
            }
            ::operator delete(data_);
            data_ = new_data;
            capacity_ = new_cap;
         }
      }

      void push_back_impl(const value_type& val)
      {
         grow_if_needed();
         std::construct_at(data_ + size_, val);
         ++size_;
      }

      void push_back_impl(value_type&& val)
      {
         grow_if_needed();
         std::construct_at(data_ + size_, std::move(val));
         ++size_;
      }

      template <class... Args>
      void emplace_back_impl(Args&&... args)
      {
         grow_if_needed();
         std::construct_at(data_ + size_, std::forward<Args>(args)...);
         ++size_;
      }

      void destroy_all() noexcept
      {
         for (uint32_t i = 0; i < size_; ++i) {
            std::destroy_at(data_ + i);
         }
      }

      void free_data() noexcept
      {
         destroy_all();
         ::operator delete(data_);
         data_ = nullptr;
         size_ = 0;
         capacity_ = 0;
      }

      // --- Bloom filter ---

      void bloom_set(uint32_t h) const noexcept
      {
         const uint32_t a = h & bloom_mask;
         const uint32_t b = (h >> 10) & bloom_mask;
         index_->bloom[a >> 3] |= uint8_t(1) << (a & 7);
         index_->bloom[b >> 3] |= uint8_t(1) << (b & 7);
      }

      bool bloom_maybe_contains(uint32_t h) const noexcept
      {
         const uint32_t a = h & bloom_mask;
         const uint32_t b = (h >> 10) & bloom_mask;
         return (index_->bloom[a >> 3] & (uint8_t(1) << (a & 7))) && (index_->bloom[b >> 3] & (uint8_t(1) << (b & 7)));
      }

      void bloom_clear() const noexcept { std::memset(index_->bloom, 0, bloom_bytes); }

      // --- Index memory management ---

      hash_index_entry* index_entries() const noexcept
      {
         return static_cast<hash_index_entry*>(static_cast<void*>(index_ + 1));
      }

      uint32_t index_size() const noexcept { return index_ ? index_->size : 0; }

      void invalidate_index() noexcept
      {
         if (index_) index_->size = 0;
      }

      void free_index() noexcept
      {
         std::free(index_);
         index_ = nullptr;
      }

      void ensure_index_capacity(uint32_t needed) const
      {
         if (index_ && index_->capacity >= needed) return;
         uint32_t cap = index_ ? index_->capacity : 0;
         while (cap < needed) cap = cap ? cap * 2 : 16;
         auto* block =
            static_cast<index_header*>(std::realloc(index_, sizeof(index_header) + cap * sizeof(hash_index_entry)));
         if (!block) GLZ_THROW_OR_ABORT(std::bad_alloc{});
         if (!index_) {
            block->size = 0;
            std::memset(block->bloom, 0, bloom_bytes);
         }
         block->capacity = cap;
         index_ = block;
      }

      // --- Index construction ---

      // Full rebuild: rehash all keys, sort, and repopulate bloom filter.
      void rebuild_index() const
      {
         ensure_index_capacity(size_);
         bloom_clear();
         auto* entries = index_entries();
         for (uint32_t i = 0; i < size_; ++i) {
            const uint32_t h = hash_key(data_[i].first);
            entries[i] = {h, i};
            bloom_set(h);
         }
         std::sort(entries, entries + size_, [](const auto& a, const auto& b) { return a.hash < b.hash; });
         index_->size = size_;
      }

      // Bring the index up to date.
      // If fully invalid (after erase) or many entries are stale (bloom deferred), do a full rebuild.
      // If only a few entries are stale (incremental inserts above bloom_threshold), insert-sort them.
      void ensure_index() const
      {
         if (size_ <= linear_search_threshold) return;
         const auto current = index_size();
         if (current == size_) return; // fully current

         if (current == 0 || (size_ - current) > linear_search_threshold) {
            rebuild_index(); // full rebuild
            return;
         }

         // Incrementally insert-sort the few new entries
         for (uint32_t i = current; i < size_; ++i) {
            const hash_index_entry entry{hash_key(data_[i].first), i};
            const auto* base = index_entries();
            const auto* p = branchless_lower_bound(base, index_->size, entry.hash);
            auto pos = static_cast<size_t>(p - base);

            const uint32_t m = index_->size;
            ensure_index_capacity(m + 1);
            auto* entries = index_entries();
            if (pos < m) {
               std::memmove(entries + pos + 1, entries + pos, (m - pos) * sizeof(hash_index_entry));
            }
            entries[pos] = entry;
            index_->size = i + 1;
         }
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
      index_find_result index_find_or_pos(const K& key, uint32_t h)
      {
         ensure_index();

         const auto* base = index_entries();
         const auto* idx_end = base + index_->size;
         const auto* p = branchless_lower_bound(base, index_->size, h);
         const size_t pos = static_cast<size_t>(p - base);

         // Scan adjacent entries with matching hash
         for (auto* scan = p; scan != idx_end && scan->hash == h; ++scan) {
            if (data_[scan->index].first == key) {
               return {data_ + scan->index, pos, h};
            }
         }

         return {data_ + size_, pos, h};
      }

      // Linear search - used for small maps
      template <class K>
      iterator linear_find(const K& key)
      {
         for (uint32_t i = 0; i < size_; ++i) {
            if (data_[i].first == key) return data_ + i;
         }
         return data_ + size_;
      }

      template <class K>
      const_iterator linear_find(const K& key) const
      {
         for (uint32_t i = 0; i < size_; ++i) {
            if (data_[i].first == key) return data_ + i;
         }
         return data_ + size_;
      }

      // Branchless binary search - compiles to cmov instead of conditional branches
      static const hash_index_entry* branchless_lower_bound(const hash_index_entry* p, size_t len,
                                                            uint32_t target) noexcept
      {
         while (len > 1) {
            size_t half = len / 2;
            p += (p[half - 1].hash < target) * half;
            len -= half;
         }
         // Final element check: advance past it if it's less than target
         p += (len == 1 && p->hash < target);
         return p;
      }

      // Binary search using hash index
      // On hash match, scans adjacent entries to handle collisions
      template <class K>
      iterator index_find(const K& key) const
      {
         ensure_index();

         const uint32_t h = hash_key(key);
         const auto* base = index_entries();
         const auto* idx_end = base + index_->size;
         const auto* p = branchless_lower_bound(base, index_->size, h);

         for (; p != idx_end && p->hash == h; ++p) {
            if (data_[p->index].first == key) {
               return data_ + p->index;
            }
         }
         return data_ + size_;
      }

      template <class K>
      const_iterator const_index_find(const K& key) const
      {
         ensure_index();

         const uint32_t h = hash_key(key);
         const auto* base = index_entries();
         const auto* idx_end = base + index_->size;
         const auto* p = branchless_lower_bound(base, index_->size, h);

         for (; p != idx_end && p->hash == h; ++p) {
            if (data_[p->index].first == key) {
               return data_ + p->index;
            }
         }
         return data_ + size_;
      }

      // --- Insert helpers ---

      // Try the bloom filter fast path for insert.
      // Returns true if the key is definitely new and was appended (caller is done).
      // Returns false if the bloom says "maybe present" (caller must do full search).
      template <class F>
      bool try_bloom_insert(uint32_t h, F&& append_fn)
      {
         if (index_ && size_ <= bloom_threshold && !bloom_maybe_contains(h)) {
            // Definitely not present — skip the search
            append_fn();
            bloom_set(h);
            // Index is now stale (index_->size < size_), will rebuild on next lookup or false positive
            return true;
         }
         return false;
      }

      // Full insert path: ensure index, search for duplicate, insert if new.
      // Returns iterator to existing or newly inserted element, and whether insertion happened.
      template <class F>
      std::pair<iterator, bool> indexed_insert(const key_type& key, uint32_t h, F&& append_fn)
      {
         auto [it, pos, _] = index_find_or_pos(key, h);
         if (it != end()) return {it, false};
         append_fn();
         bloom_set(h);
         // After ensure_index in index_find_or_pos, index is current for all entries before this one.
         // Insert the new entry into the sorted index to keep it current.
         if (index_size() > 0) {
            const uint32_t n = index_->size;
            ensure_index_capacity(n + 1);
            auto* entries = index_entries();
            if (pos < n) {
               std::memmove(entries + pos + 1, entries + pos, (n - pos) * sizeof(hash_index_entry));
            }
            entries[pos] = {h, static_cast<uint32_t>(size_ - 1)};
            index_->size = size_;
         }
         return {data_ + size_ - 1, true};
      }

     public:
      // Constructors
      ordered_small_map() = default;

      ~ordered_small_map()
      {
         free_data();
         free_index();
      }

      ordered_small_map(std::initializer_list<value_type> init)
      {
         reserve(init.size());
         for (const auto& pair : init) {
            if (linear_find(pair.first) == end()) {
               push_back_impl(pair);
            }
         }
      }

      template <class InputIt>
      ordered_small_map(InputIt first, InputIt last)
      {
         for (; first != last; ++first) {
            insert(*first);
         }
      }

      ordered_small_map(const ordered_small_map& other)
      {
         if (other.size_ > 0) {
            data_ = static_cast<value_type*>(::operator new(other.size_ * sizeof(value_type)));
            capacity_ = other.size_;
            for (uint32_t i = 0; i < other.size_; ++i) {
               std::construct_at(data_ + i, other.data_[i]);
            }
            size_ = other.size_;
         }
         // Don't copy index - it will be rebuilt lazily
      }

      ordered_small_map(ordered_small_map&& other) noexcept
         : data_(other.data_), size_(other.size_), capacity_(other.capacity_), index_(other.index_)
      {
         other.data_ = nullptr;
         other.size_ = 0;
         other.capacity_ = 0;
         other.index_ = nullptr;
      }

      ordered_small_map& operator=(const ordered_small_map& other)
      {
         if (this != &other) {
            free_data();
            free_index();
            if (other.size_ > 0) {
               data_ = static_cast<value_type*>(::operator new(other.size_ * sizeof(value_type)));
               capacity_ = other.size_;
               for (uint32_t i = 0; i < other.size_; ++i) {
                  std::construct_at(data_ + i, other.data_[i]);
               }
               size_ = other.size_;
            }
         }
         return *this;
      }

      ordered_small_map& operator=(ordered_small_map&& other) noexcept
      {
         if (this != &other) {
            free_data();
            free_index();
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            index_ = other.index_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
            other.index_ = nullptr;
         }
         return *this;
      }

      // Iterators (follow insertion order)
      iterator begin() noexcept { return data_; }
      const_iterator begin() const noexcept { return data_; }
      const_iterator cbegin() const noexcept { return data_; }

      iterator end() noexcept { return data_ + size_; }
      const_iterator end() const noexcept { return data_ + size_; }
      const_iterator cend() const noexcept { return data_ + size_; }

      reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
      const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
      const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

      reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
      const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
      const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

      // Capacity
      [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
      size_type size() const noexcept { return size_; }
      size_type capacity() const noexcept { return capacity_; }

      void reserve(size_type new_cap)
      {
         if (new_cap <= capacity_) return;
         auto* new_data = static_cast<value_type*>(::operator new(new_cap * sizeof(value_type)));
         for (uint32_t i = 0; i < size_; ++i) {
            std::construct_at(new_data + i, std::move(data_[i]));
            std::destroy_at(data_ + i);
         }
         ::operator delete(data_);
         data_ = new_data;
         capacity_ = static_cast<uint32_t>(new_cap);
      }

      void shrink_to_fit()
      {
         if (size_ == capacity_) return;
         if (size_ == 0) {
            ::operator delete(data_);
            data_ = nullptr;
            capacity_ = 0;
            return;
         }
         auto* new_data = static_cast<value_type*>(::operator new(size_ * sizeof(value_type)));
         for (uint32_t i = 0; i < size_; ++i) {
            std::construct_at(new_data + i, std::move(data_[i]));
            std::destroy_at(data_ + i);
         }
         ::operator delete(data_);
         data_ = new_data;
         capacity_ = size_;
      }

      // Modifiers
      void clear() noexcept
      {
         destroy_all();
         size_ = 0;
         free_index();
      }

      std::pair<iterator, bool> insert(const value_type& value)
      {
         if (size_ <= linear_search_threshold) {
            auto it = linear_find(value.first);
            if (it != end()) return {it, false};
            push_back_impl(value);
            return {data_ + size_ - 1, true};
         }
         const uint32_t h = hash_key(value.first);
         if (try_bloom_insert(h, [&] { push_back_impl(value); })) {
            return {data_ + size_ - 1, true};
         }
         return indexed_insert(value.first, h, [&] { push_back_impl(value); });
      }

      std::pair<iterator, bool> insert(value_type&& value)
      {
         if (size_ <= linear_search_threshold) {
            auto it = linear_find(value.first);
            if (it != end()) return {it, false};
            push_back_impl(std::move(value));
            return {data_ + size_ - 1, true};
         }
         const uint32_t h = hash_key(value.first);
         if (try_bloom_insert(h, [&] { push_back_impl(std::move(value)); })) {
            return {data_ + size_ - 1, true};
         }
         // try_bloom_insert returned false without calling the lambda, so value is still valid
         return indexed_insert(value.first, h, [&] { push_back_impl(std::move(value)); });
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
         if (size_ <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return {it, false};
            emplace_back_impl(std::forward<K>(key), T(std::forward<Args>(args)...));
            return {data_ + size_ - 1, true};
         }
         const uint32_t h = hash_key(key);
         if (try_bloom_insert(h, [&] { emplace_back_impl(std::forward<K>(key), T(std::forward<Args>(args)...)); })) {
            return {data_ + size_ - 1, true};
         }
         return indexed_insert(key, h,
                               [&] { emplace_back_impl(std::forward<K>(key), T(std::forward<Args>(args)...)); });
      }

      template <class K, class... Args>
      std::pair<iterator, bool> try_emplace(K&& key, Args&&... args)
      {
         return emplace(std::forward<K>(key), std::forward<Args>(args)...);
      }

      iterator erase(const_iterator pos)
      {
         invalidate_index();
         auto* mutable_pos = data_ + (pos - data_);
         std::destroy_at(mutable_pos);
         // Shift remaining elements left
         for (auto* p = mutable_pos; p + 1 < data_ + size_; ++p) {
            std::construct_at(p, std::move(*(p + 1)));
            std::destroy_at(p + 1);
         }
         --size_;
         return mutable_pos;
      }

      iterator erase(const_iterator first, const_iterator last)
      {
         if (first == last) return data_ + (first - data_);
         invalidate_index();
         auto* mfirst = data_ + (first - data_);
         auto* mlast = data_ + (last - data_);
         const auto count = static_cast<uint32_t>(mlast - mfirst);
         // Destroy erased elements
         for (auto* p = mfirst; p < mlast; ++p) {
            std::destroy_at(p);
         }
         // Shift remaining elements left
         for (auto* dst = mfirst; mlast < data_ + size_; ++dst, ++mlast) {
            std::construct_at(dst, std::move(*mlast));
            std::destroy_at(mlast);
         }
         size_ -= count;
         return mfirst;
      }

      size_type erase(std::string_view key)
      {
         auto it = find(key);
         if (it != end()) {
            erase(it);
            return 1;
         }
         return 0;
      }

      // Lookup
      template <class K>
      iterator find(const K& key)
      {
         if (size_ <= linear_search_threshold) {
            return linear_find(key);
         }
         return index_find(key);
      }

      template <class K>
      const_iterator find(const K& key) const
      {
         if (size_ <= linear_search_threshold) {
            return linear_find(key);
         }
         return const_index_find(key);
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
         if (size_ <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return it->second;
            emplace_back_impl(key, mapped_type{});
            return data_[size_ - 1].second;
         }
         const uint32_t h = hash_key(key);
         if (try_bloom_insert(h, [&] { emplace_back_impl(key, mapped_type{}); })) {
            return data_[size_ - 1].second;
         }
         auto [it, pos, _] = index_find_or_pos(key, h);
         if (it != end()) return it->second;
         emplace_back_impl(key, mapped_type{});
         bloom_set(h);
         if (index_size() > 0) {
            const uint32_t n = index_->size;
            ensure_index_capacity(n + 1);
            auto* entries = index_entries();
            if (pos < n) {
               std::memmove(entries + pos + 1, entries + pos, (n - pos) * sizeof(hash_index_entry));
            }
            entries[pos] = {h, size_ - 1};
            index_->size = size_;
         }
         return data_[size_ - 1].second;
      }

      mapped_type& operator[](key_type&& key)
      {
         if (size_ <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return it->second;
            emplace_back_impl(std::move(key), mapped_type{});
            return data_[size_ - 1].second;
         }
         const uint32_t h = hash_key(key);
         if (try_bloom_insert(h, [&] { emplace_back_impl(std::move(key), mapped_type{}); })) {
            return data_[size_ - 1].second;
         }
         // key may have been moved — but try_bloom_insert returned false, so the lambda didn't execute
         auto [it, pos, _] = index_find_or_pos(key, h);
         if (it != end()) return it->second;
         emplace_back_impl(std::move(key), mapped_type{});
         bloom_set(h);
         if (index_size() > 0) {
            const uint32_t n = index_->size;
            ensure_index_capacity(n + 1);
            auto* entries = index_entries();
            if (pos < n) {
               std::memmove(entries + pos + 1, entries + pos, (n - pos) * sizeof(hash_index_entry));
            }
            entries[pos] = {h, size_ - 1};
            index_->size = size_;
         }
         return data_[size_ - 1].second;
      }

      // Heterogeneous lookup version for operator[]
      // Only participates when K is convertible to string_view but not string
      template <class K>
         requires(std::convertible_to<K, std::string_view> && !std::same_as<std::decay_t<K>, std::string> &&
                  !std::same_as<std::decay_t<K>, key_type>)
      mapped_type& operator[](K&& key)
      {
         if (size_ <= linear_search_threshold) {
            auto it = linear_find(key);
            if (it != end()) return it->second;
            emplace_back_impl(std::string(std::forward<K>(key)), mapped_type{});
            return data_[size_ - 1].second;
         }
         const uint32_t h = hash_key(key);
         if (try_bloom_insert(h, [&] { emplace_back_impl(std::string(std::forward<K>(key)), mapped_type{}); })) {
            return data_[size_ - 1].second;
         }
         auto [it, pos, _] = index_find_or_pos(key, h);
         if (it != end()) return it->second;
         emplace_back_impl(std::string(std::forward<K>(key)), mapped_type{});
         bloom_set(h);
         if (index_size() > 0) {
            const uint32_t n = index_->size;
            ensure_index_capacity(n + 1);
            auto* entries = index_entries();
            if (pos < n) {
               std::memmove(entries + pos + 1, entries + pos, (n - pos) * sizeof(hash_index_entry));
            }
            entries[pos] = {h, size_ - 1};
            index_->size = size_;
         }
         return data_[size_ - 1].second;
      }

      template <class K>
      mapped_type& at(const K& key)
      {
         auto it = find(key);
         if (it == end()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("ordered_small_map::at: key not found"));
         }
         return it->second;
      }

      template <class K>
      const mapped_type& at(const K& key) const
      {
         auto it = find(key);
         if (it == end()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("ordered_small_map::at: key not found"));
         }
         return it->second;
      }

      // Direct access to underlying data
      value_type* data() noexcept { return data_; }
      const value_type* data() const noexcept { return data_; }

      // Comparison
      bool operator==(const ordered_small_map& other) const
      {
         if (size_ != other.size_) return false;
         for (uint32_t i = 0; i < size_; ++i) {
            if (data_[i] != other.data_[i]) return false;
         }
         return true;
      }

      bool operator!=(const ordered_small_map& other) const { return !(*this == other); }
   };
}
