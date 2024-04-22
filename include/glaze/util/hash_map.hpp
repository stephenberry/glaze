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

namespace glz::detail
{
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
   struct naive_prng
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

   // With perfect hash tables we can sacrifice quality of the hash function since
   // we keep generating seeds until its perfect. This allows for the usage of fast
   // but terible hashing algs.
   // This is one such terible hashing alg
   struct naive_hash
   {
      static constexpr uint64_t bitmix(uint64_t h) noexcept
      {
         h ^= (h >> 33);
         h *= 0xff51afd7ed558ccdL;
         h ^= (h >> 33);
         h *= 0xc4ceb9fe1a85ec53L;
         h ^= (h >> 33);
         return h;
      };

      // static constexpr uint64_t bitmix(uint64_t x)
      // {
      //   x *= 0x9FB21C651E98DF25L;
      //   x ^= std::rotr(x, 49);
      //   return x;
      // };

      constexpr uint64_t operator()(std::integral auto value, const uint64_t seed) noexcept
      {
         return bitmix(uint64_t(value) ^ seed);
      }

      constexpr uint64_t operator()(const std::string_view value, const uint64_t seed) noexcept
      {
         uint64_t h = (0xcbf29ce484222325 ^ seed) * 1099511628211;
         const auto n = value.size();
         const char* data = value.data();

         if (n < 8) {
            return bitmix(h ^ to_uint64_n_below_8(data, n));
         }

         const char* end7 = data + n - 7;
         for (auto d0 = data; d0 < end7; d0 += 8) {
            h = bitmix(h ^ to_uint64(d0));
         }
         // Handle potential tail. We know we have at least 8
         return bitmix(h ^ to_uint64(data + n - 8));
      }
   };

   constexpr bool contains(auto&& data, auto&& val) noexcept
   {
      const auto n = data.size();
      for (size_t i = 0; i < n; ++i) {
         if (data[i] == val) {
            return true;
         }
      }
      return false;
   }

   constexpr size_t naive_map_max_size = 32;

   template <class Value, size_t N, bool use_hash_comparison = false>
   struct naive_map
   {
      // Birthday paradox makes this unsuitable for large numbers of keys without
      // using a ton of memory.
      static_assert(N <= naive_map_max_size, "Not suitable for large numbers of keys");
      // std::bit_ceil(N * N) / 2 results in a max of around 62% collision chance (e.g. size 32).
      // This uses 512 bytes for 32 keys.
      // Keeping the bucket size a power of 2 probably makes the modulus more efficient.
      static constexpr size_t bucket_size = (N == 1) ? 1 : std::bit_ceil(N * N) / 2;
      uint64_t seed{};
      std::array<std::pair<std::string_view, Value>, N> items{};
      std::array<uint64_t, N * use_hash_comparison> hashes{};
      std::array<uint8_t, bucket_size> table{};

      explicit constexpr naive_map(const std::array<std::pair<std::string_view, Value>, N>& pairs) : items(pairs)
      {
         seed = naive_perfect_hash();
         if (seed == (std::numeric_limits<uint64_t>::max)()) {
            // Failed to find perfect hash
            std::abort();
         }

         for (size_t i = 0; i < N; ++i) {
            const auto hash = naive_hash{}(items[i].first, seed);
            if constexpr (use_hash_comparison) {
               hashes[i] = hash;
            }
            table[hash % bucket_size] = uint8_t(i);
         }
      }

      constexpr decltype(auto) begin() const { return items.begin(); }
      constexpr decltype(auto) end() const { return items.end(); }

      constexpr size_t size() const { return items.size(); }

      constexpr decltype(auto) find(auto&& key) const noexcept
      {
         const auto hash = naive_hash{}(key, seed);
         // constexpr bucket_size means the compiler can replace the modulos with
         // more efficient instructions So this is not as expensive as this looks
         const auto index = table[hash % bucket_size];
         if constexpr (use_hash_comparison) {
            // Odds of having a uint64_t hash collision is pretty small
            // And no valid/known keys could colide becuase of perfect hashing
            if (hashes[index] != hash) [[unlikely]]
               return items.end();
         }
         else {
            const auto& item = items[index];
            if (!compare_sv(item.first, key)) [[unlikely]]
               return items.end();
         }
         return items.begin() + index;
      }

      constexpr uint64_t naive_perfect_hash() noexcept
      {
         std::array<size_t, N> bucket_index{};

         naive_prng gen{};
         for (size_t i = 0; i < 1024; ++i) {
            seed = gen();
            size_t index = 0;
            for (const auto& kv : items) {
               const auto hash = naive_hash{}(kv.first, seed);
               const auto bucket = hash % bucket_size;
               if (contains(std::span{bucket_index.data(), index}, bucket)) {
                  break;
               }
               bucket_index[index] = bucket;
               ++index;
            }

            if (index == N) return seed;
         }

         return uint64_t(-1);
      }
   };

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
      using hash_alg = naive_hash;
      uint64_t seed{};
      // The extra info in the bucket most likely does not need to be 64 bits
      std::array<int64_t, N> buckets{};
      using storage_type = decltype(fit_unsigned_type<N>());
      std::array<storage_type, storage_size> table{};
      std::array<std::pair<Key, Value>, N> items{};
      std::array<uint64_t, N + 1> hashes{}; // Save one more hash value of 0 for unknown keys

      constexpr decltype(auto) begin() const { return items.begin(); }
      constexpr decltype(auto) end() const { return items.end(); }

      constexpr decltype(auto) begin() { return items.begin(); }
      constexpr decltype(auto) end() { return items.end(); }

      constexpr size_t size() const { return items.size(); }

      constexpr size_t index(auto&& key) const { return find(key) - begin(); }

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

      explicit constexpr normal_map(const std::array<std::pair<Key, Value>, N>& pairs) : items(pairs)
      {
         find_perfect_hash();
      }

      // Not a very good combine but much like the hash it doesnt matter
      static constexpr uint64_t combine(uint64_t a, uint64_t b) { return hash_alg::bitmix(a ^ b); }

      constexpr void find_perfect_hash() noexcept
      {
         if constexpr (N == 0) {
            return;
         }

         std::array<std::array<storage_type, max_bucket_size>, N> full_buckets{};
         std::array<size_t, N> bucket_sizes{};
         detail::naive_prng gen{};

         bool failed;
         do {
            failed = false;
            seed = gen() + 1;
            for (storage_type i{}; i < N; ++i) {
               const auto hash = hash_alg{}(items[i].first, seed);
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
   };

   struct single_char_hash_desc
   {
      size_t N{};
      bool valid{};
      uint8_t min_diff{};
      uint8_t front{};
      uint8_t back{};
      bool is_front_hash = true;
      bool is_sum_hash = false;
   };

   struct single_char_hash_opts
   {
      bool is_front_hash = true;
      bool is_sum_hash = false; // sums the size of the key
   };

   template <size_t N, single_char_hash_opts Opts = single_char_hash_opts{}>
      requires(N < 256)
   inline constexpr single_char_hash_desc single_char_hash(const std::array<std::string_view, N>& v) noexcept
   {
      std::array<uint8_t, N> hashes;
      for (size_t i = 0; i < N; ++i) {
         if (v[i].size() == 0) {
            return {};
         }
         if constexpr (Opts.is_front_hash) {
            if constexpr (Opts.is_sum_hash) {
               hashes[i] = uint8_t(v[i][0]) + uint8_t(v[i].size());
            }
            else {
               hashes[i] = uint8_t(v[i][0]);
            }
         }
         else {
            hashes[i] = uint8_t(v[i].back());
         }
      }

      std::sort(hashes.begin(), hashes.end());

      uint8_t min_diff = (std::numeric_limits<uint8_t>::max)();
      for (size_t i = 0; i < N - 1; ++i) {
         const auto diff = uint8_t(hashes[i + 1] - hashes[i]);
         if (diff == 0) {
            return {};
         }
         if (diff < min_diff) {
            min_diff = diff;
         }
      }

      return single_char_hash_desc{
         N, min_diff > 0, min_diff, hashes.front(), hashes.back(), Opts.is_front_hash, Opts.is_sum_hash};
   }

   template <class T, single_char_hash_desc D>
      requires(D.N < 256)
   struct single_char_map
   {
      static constexpr auto N = D.N;
      std::array<std::pair<std::string_view, T>, N> items{};
      static constexpr size_t N_table = D.back - D.front + 1;
      std::array<uint8_t, N_table> table{};

      constexpr decltype(auto) begin() const { return items.begin(); }
      constexpr decltype(auto) end() const { return items.end(); }

      constexpr decltype(auto) find(auto&& key) const noexcept
      {
         if (key.size() == 0) [[unlikely]] {
            return items.end();
         }

         const auto k = [&]() -> uint8_t {
            if constexpr (D.is_front_hash) {
               if constexpr (D.is_sum_hash) {
                  return uint8_t(uint8_t(key[0]) + uint8_t(key.size()) - D.front);
               }
               else {
                  return uint8_t(uint8_t(key[0]) - D.front);
               }
            }
            else {
               return uint8_t(uint8_t(key.back()) - D.front);
            }
         }();

         if (k >= uint8_t(N_table)) [[unlikely]] {
            return items.end();
         }
         const auto index = table[k];
         const auto& item = items[index];
         if (!compare_sv(item.first, key)) [[unlikely]]
            return items.end();
         return items.begin() + index;
      }
   };

   template <class T, single_char_hash_desc D>
      requires(D.N < 256)
   constexpr auto make_single_char_map(std::initializer_list<std::pair<std::string_view, T>> pairs)
   {
      constexpr auto N = D.N;
      if (pairs.size() != N) {
         std::abort();
      }
      single_char_map<T, D> ht{};

      uint8_t i = 0;
      for (const auto& pair : pairs) {
         ht.items[i] = pair;
         const auto& key = pair.first;
         if constexpr (D.is_front_hash) {
            if constexpr (D.is_sum_hash) {
               ht.table[uint8_t(key[0]) + uint8_t(key.size()) - D.front] = i;
            }
            else {
               ht.table[uint8_t(key[0]) - D.front] = i;
            }
         }
         else {
            ht.table[uint8_t(key.back()) - D.front] = i;
         }
         ++i;
      }

      return ht;
   }

   template <class T, const std::string_view& S>
   struct micro_map1
   {
      std::array<std::pair<std::string_view, T>, 1> items{};

      constexpr decltype(auto) begin() const { return items.begin(); }
      constexpr decltype(auto) end() const { return items.end(); }

      constexpr decltype(auto) find(auto&& key) const noexcept
      {
         if (compare_sv<S>(key)) [[likely]] {
            return items.begin();
         }
         else [[unlikely]] {
            return items.end();
         }
      }
   };

   template <const std::string_view& S, bool CheckSize = true>
   inline constexpr bool cx_string_cmp(const std::string_view key) noexcept
   {
      if (std::is_constant_evaluated()) {
         return key == S;
      }
      else {
         if constexpr (CheckSize) {
            return compare_sv<S>(key);
         }
         else {
            return compare<S.size()>(key.data(), S.data());
         }
      }
   }

   template <class T, const std::string_view& S0, const std::string_view& S1>
   struct micro_map2
   {
      std::array<std::pair<std::string_view, T>, 2> items{};

      static constexpr bool same_size =
         S0.size() == S1.size(); // if we need to check the size again on the second compare
      static constexpr bool check_size = !same_size;

      constexpr decltype(auto) begin() const { return items.begin(); }
      constexpr decltype(auto) end() const { return items.end(); }

      constexpr decltype(auto) find(auto&& key) const noexcept
      {
         if constexpr (same_size) {
            constexpr auto n = S0.size();
            if (key.size() != n) {
               return items.end();
            }
         }

         if (cx_string_cmp<S0, check_size>(key)) {
            return items.begin();
         }
         else if (cx_string_cmp<S1, check_size>(key)) {
            return items.begin() + 1;
         }
         else [[unlikely]] {
            return items.end();
         }
      }
   };
}

#ifdef _MSC_VER
// restore disabled warning
#pragma warning(pop)
#endif
