// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>
#include <span>
#include <string_view>

#include "glaze/concepts/container_concepts.hpp"
#include "glaze/util/compare.hpp"

#ifdef _MSC_VER
// Turn off broken warning from MSVC for << operator precedence
#pragma warning(push)
#pragma warning(disable : 4554)
#endif

// Notes on padding:
// We only need to do buffer extensions on very short keys (n < 8)
// Our static thread_local string_buffer always has enough padding for short strings (n < 8)
// Only short string_view keys from the primary buffer are potentially an issue
// Typically short keys are not going to be at the end of the buffer
// For valid keys we always have a quote and a null character '\0'
// Our key can be empty, which means we need 6 bytes of additional padding

// To provide a mechanism to short circuit hashing when we know an unknown key is provided
// we allow hashing algorithms to return the seed when a hash does not need to be performed.
// To improve performance, we can ensure that the seed never hashes with any of the buckets as well.

namespace glz
{
   template <class T1, class T2>
   struct pair
   {
      using first_type = T1;
      using second_type = T2;
      T1 first{};
      T2 second{};
   };

   template <class T1, class T2>
   pair(T1, T2) -> pair<T1, T2>;

   template <size_t I, pair_t T>
   constexpr decltype(auto) get(T&& p) noexcept
   {
      if constexpr (I == 0) {
         return p.first;
      }
      else if constexpr (I == 1) {
         return p.second;
      }
      else {
         static_assert(false_v<decltype(p)>, "Invalid get for pair");
      }
   }
}

namespace glz
{
   inline constexpr size_t naive_map_max_size = 128;

   struct naive_map_desc
   {
      size_t N{};
      uint64_t seed{};
      size_t bucket_size{};
      bool use_hash_comparison = false;
      size_t min_length = (std::numeric_limits<size_t>::max)();
      size_t max_length{};
   };

   inline constexpr uint64_t to_uint64_n_below_8(const char* bytes, const size_t N) noexcept
   {
      static_assert(std::endian::native == std::endian::little);
      uint64_t res{};
      if (std::is_constant_evaluated()) {
         for (size_t i = 0; i < N; ++i) {
            res |= (uint64_t(uint8_t(bytes[i])) << (i << 3));
         }
      }
      else {
         switch (N) {
         case 1: {
            std::memcpy(&res, bytes, 1);
            break;
         }
         case 2: {
            std::memcpy(&res, bytes, 2);
            break;
         }
         case 3: {
            std::memcpy(&res, bytes, 3);
            break;
         }
         case 4: {
            std::memcpy(&res, bytes, 4);
            break;
         }
         case 5: {
            std::memcpy(&res, bytes, 5);
            break;
         }
         case 6: {
            std::memcpy(&res, bytes, 6);
            break;
         }
         case 7: {
            std::memcpy(&res, bytes, 7);
            break;
         }
         default: {
            // zero size case
            break;
         }
         }
      }
      return res;
   }

   template <size_t N = 8>
   constexpr uint64_t to_uint64(const char* bytes) noexcept
   {
      static_assert(N <= sizeof(uint64_t));
      static_assert(std::endian::native == std::endian::little);
      if (std::is_constant_evaluated()) {
         uint64_t res{};
         for (size_t i = 0; i < N; ++i) {
            res |= (uint64_t(uint8_t(bytes[i])) << (i << 3));
         }
         return res;
      }
      else if constexpr (N == 8) {
         uint64_t res;
         std::memcpy(&res, bytes, N);
         return res;
      }
      else {
         uint64_t res{};
         std::memcpy(&res, bytes, N);
         constexpr auto num_bytes = sizeof(uint64_t);
         constexpr auto shift = (uint64_t(num_bytes - N) << 3);
         if constexpr (shift == 0) {
            return res;
         }
         else {
            return (res << shift) >> shift;
         }
      }
   }

   // https://en.wikipedia.org/wiki/Xorshift
   struct naive_prng final
   {
      uint64_t x = 7185499250578500046;
      constexpr uint64_t operator()() noexcept
      {
         x ^= x >> 12;
         x ^= x << 25;
         x ^= x >> 27;
         return x * 0x2545F4914F6CDD1DULL;
      }
   };

   // Check if a size_t value exists inside of a container like std::array<size_t, N>
   // Using a pointer and size rather than std::span for faster compile times
   constexpr bool contains(const size_t* data, const size_t size, const size_t val) noexcept
   {
      for (size_t i = 0; i < size; ++i) {
         if (data[i] == val) {
            return true;
         }
      }
      return false;
   }

   template <uint64_t N>
   consteval auto fit_unsigned_type() noexcept
   {
      if constexpr (N <= (std::numeric_limits<uint8_t>::max)()) {
         return uint8_t{};
      }
      else if constexpr (N <= (std::numeric_limits<uint16_t>::max)()) {
         return uint16_t{};
      }
      else if constexpr (N <= (std::numeric_limits<uint32_t>::max)()) {
         return uint32_t{};
      }
      else if constexpr (N <= (std::numeric_limits<uint64_t>::max)()) {
         return uint64_t{};
      }
      else {
         return;
      }
   }

   template <class Key, class Value, size_t N, bool use_hash_comparison = false>
   struct normal_map
   {
      // From serge-sans-paille/frozen
      static constexpr uint64_t storage_size = std::bit_ceil(N) * (N < 32 ? 2 : 1);
      static constexpr auto max_bucket_size = 2 * std::bit_width(N);
      using hash_alg = naive_hash<use_hash_comparison>;
      uint64_t seed{};
      // The extra info in the bucket most likely does not need to be 64 bits
      std::array<int64_t, N> buckets{};
      using storage_type = decltype(fit_unsigned_type<N>());
      std::array<storage_type, storage_size> table{};
      std::array<pair<Key, Value>, N> items{};
      std::array<uint64_t, N + 1> hashes{}; // Save one more hash value of 0 for unknown keys

      constexpr decltype(auto) begin() const noexcept { return items.begin(); }
      constexpr decltype(auto) end() const noexcept { return items.end(); }

      constexpr decltype(auto) begin() noexcept { return items.begin(); }
      constexpr decltype(auto) end() noexcept { return items.end(); }

      constexpr size_t size() const noexcept { return items.size(); }

      constexpr size_t index(auto&& key) const noexcept { return find(key) - begin(); }

      constexpr decltype(auto) find(auto&& key) const noexcept
      {
         const auto hash = hash_alg{}(key, seed);
         // constexpr bucket_size means the compiler can replace the modulos with
         // more efficient instructions So this is not as expensive as this looks
         const auto extra = buckets[hash % N];
         const size_t index = extra < 1 ? -extra : table[combine(hash, extra) % storage_size];
         if constexpr (!std::integral<Key> && use_hash_comparison) {
            // Odds of having a uint64_t hash collision is pretty small
            // And no valid/known keys could colide becuase of perfect hashing
            if (hashes[index] != hash) [[unlikely]]
               return items.end();
         }
         else {
            if (index >= N) [[unlikely]] {
               return items.end();
            }
            const auto& item = items[index];
            if constexpr (std::integral<Key>) {
               if (item.first != key) [[unlikely]]
                  return items.end();
            }
            else {
               if (!compare_sv(item.first, key)) [[unlikely]]
                  return items.end();
            }
         }
         return items.begin() + index;
      }

      constexpr decltype(auto) find(auto&& key) noexcept
      {
         const auto hash = hash_alg{}(key, seed);
         const auto extra = buckets[hash % N];
         const size_t index = extra < 1 ? -extra : table[combine(hash, extra) % storage_size];
         if constexpr (!std::integral<Key> && use_hash_comparison) {
            if (hashes[index] != hash) [[unlikely]]
               return items.end();
         }
         else {
            if (index >= N) [[unlikely]] {
               return items.end();
            }
            const auto& item = items[index];
            if constexpr (std::integral<Key>) {
               if (item.first != key) [[unlikely]]
                  return items.end();
            }
            else {
               if (!compare_sv(item.first, key)) [[unlikely]]
                  return items.end();
            }
         }
         return items.begin() + index;
      }

      constexpr normal_map(const std::array<pair<Key, Value>, N>& pairs) : items(pairs) { find_perfect_hash(); }

      // Not a very good combine but much like the hash it doesnt matter
      static constexpr uint64_t combine(uint64_t a, uint64_t b) { return hash_alg::bitmix(a ^ b); }

      constexpr void find_perfect_hash() noexcept
      {
         if constexpr (N == 0) {
            return;
         }
         else {
            std::array<std::array<storage_type, max_bucket_size>, N> full_buckets{};
            std::array<size_t, N> bucket_sizes{};
            naive_prng gen{};

            bool failed;
            do {
               failed = false;
               seed = gen() + 1;
               for (storage_type i{}; i < N; ++i) {
                  const auto hash = hash_alg{}(items[i].first, seed);
                  if (hash == seed) {
                     failed = true;
                     break;
                  }
                  hashes[i] = hash;
                  const auto bucket = hash % N;
                  const auto bucket_size = bucket_sizes[bucket]++;
                  if (bucket_size == max_bucket_size) {
                     failed = true;
                     bucket_sizes = {};
                     break;
                  }
                  else {
                     full_buckets[bucket][bucket_size] = i;
                  }
               }
            } while (failed);

            std::array<size_t, N> buckets_index{};
            std::iota(buckets_index.begin(), buckets_index.end(), 0);
            std::sort(buckets_index.begin(), buckets_index.end(),
                      [&bucket_sizes](size_t i1, size_t i2) { return bucket_sizes[i1] > bucket_sizes[i2]; });

            constexpr auto unknown_key_indice = N;
            std::fill(table.begin(), table.end(), storage_type(unknown_key_indice));
            for (auto bucket_index : buckets_index) {
               const auto bucket_size = bucket_sizes[bucket_index];
               if (bucket_size < 1) break;
               if (bucket_size == 1) {
                  buckets[bucket_index] = -int64_t(full_buckets[bucket_index][0]);
                  continue;
               }
               const auto table_old = table;
               do {
                  failed = false;
                  // We need to reserve top bit for bucket_size == 1
                  const auto secondary_seed = gen() >> 1;
                  for (size_t i = 0; i < bucket_size; ++i) {
                     const auto index = full_buckets[bucket_index][i];
                     const auto hash = hashes[index];
                     const auto slot = combine(hash, secondary_seed) % storage_size;
                     if (table[slot] != unknown_key_indice) {
                        failed = true;
                        table = table_old;
                        break;
                     }
                     table[slot] = index;
                  }
                  buckets[bucket_index] = secondary_seed;
               } while (failed);
            }
         }
      }
   };
}

#ifdef _MSC_VER
// restore disabled warning
#pragma warning(pop)
#endif
