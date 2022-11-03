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
      static constexpr uint32_t fnv32_prime = 16777619;
      static constexpr uint32_t fnv32_offset_basis = 2166136261;
      
      static constexpr uint64_t fnv64_prime = 1099511628211;
      static constexpr uint64_t fnv64_offset_basis = 0xcbf29ce484222325;
      
      template <class HashType>
      struct fnv1a {};
      
      template <>
      struct fnv1a<uint32_t> {
         constexpr uint32_t operator()(auto&& value, const uint32_t seed) noexcept
         {
            uint32_t h = (fnv32_offset_basis ^ seed) * fnv32_prime;
            for (const auto& c : value) { h = (h ^ static_cast<uint32_t>(c)) * fnv32_prime; }
            return h >> 8; // shift needed for seeding
         }
      };
      
      template <>
      struct fnv1a<uint64_t> {
         constexpr uint64_t operator()(auto&& value, const uint64_t seed) noexcept
         {
            uint64_t h = (fnv64_offset_basis ^ seed) * fnv64_prime;
            for (const auto& c : value) { h = (h ^ static_cast<uint64_t>(c)) * fnv64_prime; }
            return h >> 8; // shift needed for seeding
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

      template <size_t N, class HashType>
      constexpr HashType naive_perfect_hash(auto&& keys) noexcept
      {
         static_assert(N <= 16);
         constexpr size_t m = N > 10 ? 3 * N : 2 * N;
         std::array<size_t, N> hashes{};
         std::array<size_t, N> buckets{};
         
         auto hash_alg = fnv1a<HashType>{};

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
         static constexpr size_t m = N > 10 ? 3 * N : 2 * N;
         HashType seed{};
         std::array<std::pair<std::string_view, Value>, N> items{};
         std::array<HashType, N * allow_hash_check> hashes{};
         std::array<uint8_t, m> table{};

         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }

         constexpr decltype(auto) at(auto &&key) const
         {
            const auto hash = fnv1a<HashType>{}(key, seed);
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
            const auto hash = fnv1a<HashType>{}(key, seed);
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
         constexpr size_t m = N > 10 ? 3 * N : 2 * N;

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
            const auto hash = fnv1a<HashType>{}(keys[i], ht.seed);
            if constexpr (allow_hash_check) {
               ht.hashes[i] = hash;
            }
            ht.table[hash % m] = i;
         }

         return ht;
      }
   }
}
