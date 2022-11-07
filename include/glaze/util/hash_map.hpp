#include <array>
#include <cassert>
#include <limits>
#include <span>
#include <vector>

#include <frozen/random.h>
#include "glaze/util/string_cmp.hpp"

namespace glz
{
   namespace detail
   {
      // https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
      // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
      static constexpr uint64_t fnv64_prime = 1099511628211;
      static constexpr uint64_t fnv64_offset_basis = 0xcbf29ce484222325;
      
      template <class HashType>
      struct xsm1 {};
      
      template <>
      struct xsm1<uint64_t>
      {
         constexpr uint64_t operator()(auto&& value, const uint64_t seed) noexcept
         {
            uint64_t h = (fnv64_offset_basis ^ seed) * fnv64_prime;
            const auto n = value.size();

            if (n < 8) {
               const auto shift = 64 - 8 * n;
               h ^= (!std::is_constant_evaluated() && ((reinterpret_cast<std::uintptr_t>(value.data()) & 4095) > 4088))
                       ? ((to_uint64(value.data() - 8 + n, n) >> shift) << shift)
                       : (to_uint64(value.data(), n) << shift);
               h *= fnv64_prime;
               h ^= h >> 33;
               return h;
            }

            const char* d0 = value.data();
            const char* end7 = value.data() + n - 7;
            for (; d0 < end7; d0 += 8) {
               h ^= to_uint64(d0);
               h *= fnv64_prime;
               h ^= h >> 33;
            }

            const uint64_t nm8 = n - 8;
            h ^= to_uint64(value.data() + nm8);
            h *= fnv64_prime;
            h ^= h >> 33;
            return h;
         }
      };

      template <>
      struct xsm1<uint32_t>
      {
         constexpr uint32_t operator()(auto&& value, const uint32_t seed) noexcept
         {
            return xsm1<uint64_t>{}(value, seed);
         }
      };

      constexpr bool contains(auto&& data, auto&& val) noexcept
      {
         const auto n = data.size();
         for (size_t i = 0; i < n; ++i) {
            if (data[i] == val) { return true; }
         }
         return false;
      }
      
      template <size_t N>
      constexpr auto naive_bucket_size() noexcept {
         return N < 8 ? 2 * N : 4 * N;
      }

      template <size_t N, class HashType>
      constexpr HashType naive_perfect_hash(auto&& keys) noexcept
      {
         static_assert(N <= 16);
         constexpr size_t m = naive_bucket_size<N>();
         std::array<size_t, N> hashes{};
         std::array<size_t, N> buckets{};
         
         auto hash_alg = xsm1<HashType>{};

         frozen::default_prg_t gen{};
         for (size_t i = 0; i < 1024; ++i) {
            HashType seed = gen();
            size_t index = 0;
            for (const auto& key : keys) {
               const auto hash = hash_alg(key, seed);
               if (contains(std::span{hashes.data(), index}, hash)) break;
               hashes[index] = hash;

               auto bucket = hash % m;
               if (contains(std::span{buckets.data(), index}, bucket)) break;
               buckets[index] = bucket;

               ++index;
            }

            if (index == N) return seed;
         }

         return std::numeric_limits<size_t>::max();
      }

      inline bool sv_neq(const std::string_view s0, const std::string_view s1) noexcept { return s0 != s1; }

      template <class Value, std::size_t N, class HashType, bool allow_hash_check = false>
      struct naive_map
      {
         static_assert(N <= 16);
         static constexpr size_t m = naive_bucket_size<N>();
         HashType seed{};
         std::array<std::pair<std::string_view, Value>, N> items{};
         std::array<HashType, N * allow_hash_check> hashes{};
         std::array<uint8_t, m> table{};

         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }

         constexpr decltype(auto) at(auto &&key) const
         {
            const auto hash = xsm1<HashType>{}(key, seed);
            const auto index = table[hash % m];
            const auto& item = items[index];
            if constexpr (allow_hash_check) {
               if (hashes[index] != hash) [[unlikely]]
                  throw std::runtime_error("Invalid key");
            }
            else {
               if (std::is_constant_evaluated()) {
                  if (item.first != key) [[unlikely]]
                     throw std::runtime_error("Invalid key");
               }
               else {
                  if (!string_cmp(item.first, key)) [[unlikely]]
                     throw std::runtime_error("Invalid key");
               }
            }
            return item.second;
         }

         constexpr decltype(auto) find(auto&& key) const
         {
            const auto hash = xsm1<HashType>{}(key, seed);
            const auto index = table[hash % m];
            if constexpr (allow_hash_check) {
               if (hashes[index] != hash) [[unlikely]]
                  return items.end();
            }
            else {
               const auto& item = items[index];
               if (std::is_constant_evaluated()) {
                  if (item.first != key) [[unlikely]]
                     return items.end();
               }
               else {
                  if (!string_cmp(item.first, key)) [[unlikely]]
                     return items.end();
               }
            }
            return items.begin() + index;
         }
      };

      template <class T, size_t N, class HashType, bool allow_hash_check = false>
      constexpr auto make_naive_map(std::initializer_list<std::pair<std::string_view, T>> pairs)
      {
         static_assert(N <= 16);
         assert(pairs.size() == N);
         naive_map<T, N, HashType, allow_hash_check> ht{};
         constexpr size_t m = naive_bucket_size<N>();

         std::array<std::string_view, N> keys{};
         size_t i = 0;
         for (const auto &pair : pairs) {
            ht.items[i] = pair;
            keys[i] = pair.first;
            ++i;
         }
         ht.seed = naive_perfect_hash<N, HashType>(keys);
         if (ht.seed == std::numeric_limits<HashType>::max()) throw std::runtime_error("Unable to find perfect hash.");

         for (size_t i = 0; i < N; ++i) {
            const auto hash = xsm1<HashType>{}(keys[i], ht.seed);
            if constexpr (allow_hash_check) {
               ht.hashes[i] = hash;
            }
            ht.table[hash % m] = i;
         }

         return ht;
      }
      
      struct first_char_hash_desc
      {
         size_t N{};
         bool valid{};
         uint8_t min_diff{};
         uint8_t front{};
         uint8_t back{};
      };

      template <size_t N>
      inline constexpr first_char_hash_desc first_char_hash(const std::array<std::string_view, N>& v) noexcept
      {
         if (N > 255) {
            return {};
         }
         
         std::array<uint8_t, N> hashes;
         for (size_t i = 0; i < N; ++i) {
            if (v[i].size() == 0) {
               return {};
            }
            hashes[i] = static_cast<uint8_t>(v[i][0]);
         }
         
         std::sort(hashes.begin(), hashes.end());
         
         uint8_t min_diff = std::numeric_limits<uint8_t>::max();
         for (size_t i = 0; i < N - 1; ++i) {
            if ((hashes[i + 1] - hashes[i]) < min_diff) {
               min_diff = hashes[i + 1] - hashes[i];
            }
         }
         
         return first_char_hash_desc{ N, min_diff > 0, min_diff, hashes.front(), hashes.back() };
      }

      template <class T, first_char_hash_desc D>
      struct first_char_map
      {
         static constexpr auto N = D.N;
         static_assert(N < 256);
         std::array<std::pair<std::string_view, T>, N> items{};
         static constexpr size_t N_table = D.back - D.front + 1;
         std::array<uint8_t, N_table> table{};
         
         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }
         
         constexpr decltype(auto) at(auto&& key) const
         {
            if (key.size() == 0) {
               throw std::runtime_error("Invalid key");
            }
            const auto k = static_cast<uint8_t>(key[0]) - D.front;
            if (k > N_table) {
               throw std::runtime_error("Invalid key");
            }
            const auto index = table[k];
            const auto& item = items[index];
            if (std::is_constant_evaluated()) {
               if (item.first != key) [[unlikely]]
                  throw std::runtime_error("Invalid key");
            }
            else {
               if (!string_cmp(item.first, key)) [[unlikely]]
                  throw std::runtime_error("Invalid key");
            }
            return item.second;
         }
         
         constexpr decltype(auto) find(auto&& key) const
         {
            if (key.size() == 0) {
               return items.end();
            }
            const auto k = static_cast<uint8_t>(key[0]) - D.front;
            if (k > N_table) {
               return items.end();
            }
            const auto index = table[k];
            const auto& item = items[index];
            if (std::is_constant_evaluated()) {
               if (item.first != key) [[unlikely]]
                  return items.end();
            }
            else {
               if (!string_cmp(item.first, key)) [[unlikely]]
                  return items.end();
            }
            return items.begin() + index;
         }
      };

      template <class T, first_char_hash_desc D>
      constexpr auto make_first_char_map(std::initializer_list<std::pair<std::string_view, T>> pairs)
      {
         constexpr auto N = D.N;
         static_assert(N < 256);
         assert(pairs.size() == N);
         first_char_map<T, D> ht{};
         
         uint8_t i = 0;
         for (const auto& pair : pairs) {
            ht.items[i] = pair;
            ht.table[static_cast<uint8_t>(pair.first[0]) - D.front] = i;
            ++i;
         }
         
         return ht;
      }
      
      template <class T, size_t N>
      struct micro_map {};
      
      template <class T>
      struct micro_map<T, 1>
      {
         std::array<std::pair<std::string_view, T>, 1> items{};
         
         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }
         
         constexpr decltype(auto) at(auto&& key) const
         {
            if (std::is_constant_evaluated()) {
               if (items[0].first == key) {
                  return items[0].second;
               }
               else {
                  throw std::runtime_error("Invalid key");
               }
            }
            else {
               if (string_cmp(items[0].first, key)) {
                  return items[0].second;
               }
               else {
                  throw std::runtime_error("Invalid key");
               }
            }
         }
         
         constexpr decltype(auto) find(auto&& key) const
         {
            if (std::is_constant_evaluated()) {
               if (items[0].first == key) {
                  return items.begin();
               }
               else {
                  return items.end();
               }
            }
            else {
               if (string_cmp(items[0].first, key)) {
                  return items.begin();
               }
               else {
                  return items.end();
               }
            }
         }
      };
      
      template <class T>
      struct micro_map<T, 2>
      {
         std::array<std::pair<std::string_view, T>, 2> items{};
         
         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }
         
         constexpr decltype(auto) at(auto&& key) const
         {
            if (std::is_constant_evaluated()) {
               if (items[0].first == key) {
                  return items[0].second;
               }
               else if (items[1].first == key) {
                  return items[1].second;
               }
               else {
                  throw std::runtime_error("Invalid key");
               }
            }
            else {
               if (string_cmp(items[0].first, key)) {
                  return items[0].second;
               }
               else if (string_cmp(items[1].first, key)) {
                  return items[1].second;
               }
               else {
                  throw std::runtime_error("Invalid key");
               }
            }
         }
         
         constexpr decltype(auto) find(auto&& key) const
         {
            if (std::is_constant_evaluated()) {
               if (items[0].first == key) {
                  return items.begin();
               }
               else if (items[1].first == key) {
                  return items.begin() + 1;
               }
               else {
                  return items.end();
               }
            }
            else {
               if (string_cmp(items[0].first, key)) {
                  return items.begin();
               }
               else if (string_cmp(items[1].first, key)) {
                  return items.begin() + 1;
               }
               else {
                  return items.end();
               }
            }
         }
      };
      
      template <class T, size_t N>
      constexpr auto make_micro_map(std::initializer_list<std::pair<std::string_view, T>> pairs)
      {
         assert(pairs.size() == N);
         micro_map<T, N> ht{};
         
         size_t i = 0;
         for (const auto& pair : pairs) {
            ht.items[i] = pair;
            ++i;
         }
         
         return ht;
      }
   }
}
