// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
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

// Notes on padding:
// We only need to do buffer extensions on very short keys (n < 8)
// Our static thread_local string_buffer always has enough padding for short strings (n < 8)
// Only short string_view keys from the primary buffer are potentially an issue
// Typically short keys are not going to be at the end of the buffer
// For valid keys we always have a quote and a null character '\0'
// Our key can be empty, which means we need 6 bytes of additional padding
// Getting rid of the page boundary check is an improvement to performance and safer overall

namespace glz::detail
{
   inline constexpr uint64_t to_uint64_n_below_8(const char* bytes, const size_t N) noexcept
   {
      static_assert(std::endian::native == std::endian::little);
      uint64_t res{};
      if (std::is_constant_evaluated()) {
         for (size_t i = 0; i < N; ++i) {
            res |= (uint64_t(bytes[i]) << (i << 3));
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
            res |= (uint64_t(bytes[i]) << (i << 3));
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

         if (n < 8) {
            return bitmix(h ^ to_uint64_n_below_8(value.data(), n));
         }

         const char* end7 = value.data() + n - 7;
         for (auto d0 = value.data(); d0 < end7; d0 += 8) {
            h = bitmix(h ^ to_uint64(d0));
         }
         // Handle potential tail. We know we have at least 8
         return bitmix(h ^ to_uint64(value.data() + n - 8));
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

   template <class Value, std::size_t N, bool use_hash_comparison = false>
   struct naive_map
   {
      // Birthday paradox makes this unsuitable for large numbers of keys without
      // using a ton of memory Could resonably use it for 64 or so keys if the
      // bucketsize scalled more agressively But I would swhitch to a more space
      // efficient perfect map
      static_assert(N <= 32, "Not suitable for large numbers of keys");
      static constexpr size_t bucket_size = (N > 16 ? 8 : 4) * std::bit_ceil(N);
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
            table[hash % bucket_size] = static_cast<uint8_t>(i);
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
            // And no valid keys could colide becuase of perfect hashing so this
            // isnt that bad
            if (hashes[index] != hash) [[unlikely]]
               return items.end();
         }
         else {
            const auto& item = items[index];
            if (item.first != key) [[unlikely]]
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
      // TODO: We can probably save space by using smaller items in the table (We know the range stored)
      // The extra info in the bucket most likely does not need to be 64 bits
      std::array<int64_t, N> buckets{};
      using storage_type = decltype(fit_unsigned_type<N>());
      std::array<storage_type, storage_size> table{};
      std::array<std::pair<Key, Value>, N> items{};
      std::array<uint64_t, N> hashes{};

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
            // And no valid keys could colide becuase of perfect hashing so this
            // isnt that bad
            if (hashes[index] != hash) [[unlikely]]
               return items.end();
         }
         else {
            if (index >= N) [[unlikely]] {
               return items.end();
            }
            const auto& item = items[index];
            if (item.first != key) [[unlikely]]
               return items.end();
         }
         return items.begin() + index;
      }

      constexpr decltype(auto) find(auto&& key) noexcept
      {
         const auto hash = hash_alg{}(key, seed);
         // constexpr bucket_size means the compiler can replace the modulos with
         // more efficient instructions So this is not as expensive as this looks
         const auto extra = buckets[hash % N];
         auto index = extra < 1 ? -extra : table[combine(hash, extra) % storage_size];
         if constexpr (!std::integral<Key> && use_hash_comparison) {
            // Odds of having a uint64_t hash collision is pretty small
            // And no valid keys could colide becuase of perfect hashing so this
            // isnt that bad
            if (hashes[index] != hash) [[unlikely]]
               return items.end();
         }
         else {
            const auto& item = items[index];
            if (item.first != key) [[unlikely]]
               return items.end();
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

         std::fill(table.begin(), table.end(), storage_type(-1));
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
               auto secondary_seed = gen() >> 1;
               for (size_t i = 0; i < bucket_size; ++i) {
                  const auto index = full_buckets[bucket_index][i];
                  const auto hash = hashes[index];
                  const auto slot = combine(hash, secondary_seed) % storage_size;
                  if (table[slot] != storage_type(-1)) {
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
   };

   template <size_t N, bool IsFrontHash = true>
   inline constexpr single_char_hash_desc single_char_hash(const std::array<std::string_view, N>& v) noexcept
   {
      if constexpr (N > 255) {
         return {};
      }

      std::array<uint8_t, N> hashes;
      for (size_t i = 0; i < N; ++i) {
         if (v[i].size() == 0) {
            return {};
         }
         if constexpr (IsFrontHash) {
            hashes[i] = static_cast<uint8_t>(v[i][0]);
         }
         else {
            hashes[i] = static_cast<uint8_t>(v[i].back());
         }
      }

      std::sort(hashes.begin(), hashes.end());

      uint8_t min_diff = (std::numeric_limits<uint8_t>::max)();
      for (size_t i = 0; i < N - 1; ++i) {
         if ((hashes[i + 1] - hashes[i]) < min_diff) {
            min_diff = hashes[i + 1] - hashes[i];
         }
      }

      return single_char_hash_desc{N, min_diff > 0, min_diff, hashes.front(), hashes.back(), IsFrontHash};
   }

   template <class T, single_char_hash_desc D>
   struct single_char_map
   {
      static constexpr auto N = D.N;
      static_assert(N < 256);
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

         if constexpr (D.is_front_hash) {
            const auto k = static_cast<uint8_t>(static_cast<uint8_t>(key[0]) - D.front);
            if (k >= static_cast<uint8_t>(N_table)) [[unlikely]] {
               return items.end();
            }
            const auto index = table[k];
            const auto& item = items[index];
            if (item.first != key) [[unlikely]]
               return items.end();
            return items.begin() + index;
         }
         else {
            const auto k = static_cast<uint8_t>(static_cast<uint8_t>(key.back()) - D.front);
            if (k >= static_cast<uint8_t>(N_table)) [[unlikely]] {
               return items.end();
            }
            const auto index = table[k];
            const auto& item = items[index];
            if (item.first != key) [[unlikely]]
               return items.end();
            return items.begin() + index;
         }
      }
   };

   template <class T, single_char_hash_desc D>
   constexpr auto make_single_char_map(std::initializer_list<std::pair<std::string_view, T>> pairs)
   {
      constexpr auto N = D.N;
      static_assert(N < 256);
      assert(pairs.size() == N);
      single_char_map<T, D> ht{};

      uint8_t i = 0;
      for (const auto& pair : pairs) {
         ht.items[i] = pair;
         if constexpr (D.is_front_hash) {
            ht.table[static_cast<uint8_t>(pair.first[0]) - D.front] = i;
         }
         else {
            ht.table[static_cast<uint8_t>(pair.first.back()) - D.front] = i;
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
         if (S == key) [[likely]] {
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
      constexpr auto n = S.size();
      if (std::is_constant_evaluated()) {
         return key == S;
      }
      else {
         if constexpr (CheckSize) {
            return (key.size() == n) && (std::memcmp(key.data(), S.data(), n) == 0);
         }
         else {
            return std::memcmp(key.data(), S.data(), n) == 0;
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
