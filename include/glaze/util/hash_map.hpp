#include <array>
#include <cassert>
#include <limits>
#include <span>
#include <vector>

#include "glaze/frozen/random.hpp"
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
               h ^= to_uint64(value.data(), n) << shift;
               h ^= h >> 33;
               h *= fnv64_prime;
               return h;
            }

            const char* d0 = value.data();
            const char* end7 = value.data() + n - 7;
            for (; d0 < end7; d0 += 8) {
               h ^= to_uint64_n<8>(d0);
               h ^= h >> 33;
               h *= fnv64_prime;
            }

            const uint64_t nm8 = n - 8;
            h ^= to_uint64_n<8>(value.data() + nm8);
            h ^= h >> 33;
            h *= fnv64_prime;
            return h;
         }
      };

      template <>
      struct xsm1<uint32_t>
      {
         constexpr uint32_t operator()(auto&& value, const uint32_t seed) noexcept
         {
            uint64_t hash = xsm1<uint64_t>{}(value, seed);
            return hash >> 32;
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
         static_assert(N <= 20);
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

         return std::numeric_limits<HashType>::max();
      }

      inline bool sv_neq(const sv s0, const sv s1) noexcept { return s0 != s1; }

      template <class Value, std::size_t N, class HashType, bool allow_hash_check = false>
      struct naive_map
      {
         static_assert(N <= 20);
         static constexpr size_t m = naive_bucket_size<N>();
         HashType seed{};
         std::array<std::pair<sv, Value>, N> items{};
         std::array<HashType, N * allow_hash_check> hashes{};
         std::array<uint8_t, m> table{};

         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }

         constexpr decltype(auto) at(auto &&key) const
         {
            const auto hash = xsm1<HashType>{}(key, seed);
            const auto index = table[hash % m]; // modulus should be fast because m is known compile time
            const auto& item = items[index];
            if constexpr (allow_hash_check) {
               if (hashes[index] != hash) [[unlikely]]
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
            const auto hash = xsm1<HashType>{}(key, seed);
            const auto index = table[hash % m];
            if constexpr (allow_hash_check) {
               if (hashes[index] != hash) [[unlikely]]
                  return items.end();
            }
            else {
               const auto& item = items[index];
               if (!string_cmp(item.first, key)) [[unlikely]]
                  return items.end();
            }
            return items.begin() + index;
         }
      };

      template <class T, size_t N, class HashType, bool allow_hash_check = false>
      constexpr auto make_naive_map(std::initializer_list<std::pair<sv, T>> pairs)
      {
         static_assert(N <= 20);
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
         if (ht.seed == std::numeric_limits<HashType>::max()) {
            throw std::runtime_error("Unable to find perfect hash.");
         }

         for (size_t i = 0; i < N; ++i) {
            const auto hash = xsm1<HashType>{}(keys[i], ht.seed);
            if constexpr (allow_hash_check) {
               ht.hashes[i] = hash;
            }
            ht.table[hash % m] = static_cast<uint8_t>(i);
         }

         return ht;
      }
      
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
         if (N > 255) {
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
         
         uint8_t min_diff = std::numeric_limits<uint8_t>::max();
         for (size_t i = 0; i < N - 1; ++i) {
            if ((hashes[i + 1] - hashes[i]) < min_diff) {
               min_diff = hashes[i + 1] - hashes[i];
            }
         }
         
         return single_char_hash_desc{ N, min_diff > 0, min_diff, hashes.front(), hashes.back(), IsFrontHash };
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
         
         constexpr decltype(auto) at(auto&& key) const
         {
            if (key.size() == 0) [[unlikely]] {
               throw std::runtime_error("Invalid key");
            }
            
            if constexpr (D.is_front_hash) {
               const auto k = static_cast<uint8_t>(key[0]) - D.front;
               if (k > N_table) [[unlikely]] {
                  throw std::runtime_error("Invalid key");
               }
               const auto index = table[k];
               const auto& item = items[index];
               if (!string_cmp(item.first, key)) [[unlikely]]
                  throw std::runtime_error("Invalid key");
               return item.second;
            }
            else {
               const auto k = static_cast<uint8_t>(key.back()) - D.front;
               if (k > N_table) [[unlikely]] {
                  throw std::runtime_error("Invalid key");
               }
               const auto index = table[k];
               const auto& item = items[index];
               if (!string_cmp(item.first, key)) [[unlikely]]
                  throw std::runtime_error("Invalid key");
               return item.second;
            }
         }
         
         constexpr decltype(auto) find(auto&& key) const
         {
            if (key.size() == 0) [[unlikely]] {
               return items.end();
            }
            
            if constexpr (D.is_front_hash) {
               const auto k = static_cast<uint8_t>(key[0]) - D.front;
               if (k > N_table) [[unlikely]] {
                  return items.end();
               }
               const auto index = table[k];
               const auto& item = items[index];
               if (!string_cmp(item.first, key)) [[unlikely]]
                  return items.end();
               return items.begin() + index;
            }
            else {
               const auto k = static_cast<uint8_t>(key.back()) - D.front;
               if (k > N_table) [[unlikely]] {
                  return items.end();
               }
               const auto index = table[k];
               const auto& item = items[index];
               if (!string_cmp(item.first, key)) [[unlikely]]
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
      
      template <class T, const sv& S>
      struct micro_map1
      {
         std::array<std::pair<sv, T>, 1> items{};
         
         static constexpr auto s = S; // Needed for MSVC to avoid an internal compiler error
         
         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }
         
         constexpr decltype(auto) at(auto&& key) const
         {
            if (cx_string_cmp<s>(key)) [[likely]] {
               return items[0].second;
            }
            else [[unlikely]] {
               throw std::runtime_error("Invalid key");
            }
         }
         
         constexpr decltype(auto) find(auto&& key) const
         {
            if (cx_string_cmp<s>(key)) [[likely]] {
               return items.begin();
            }
            else [[unlikely]] {
               return items.end();
            }
         }
      };
      
      template <class T, const sv& S0, const sv& S1>
      struct micro_map2
      {
         std::array<std::pair<sv, T>, 2> items{};
         
         static constexpr auto s0 = S0; // Needed for MSVC to avoid an internal compiler error
         static constexpr auto s1 = S1; // Needed for MSVC to avoid an internal compiler error
         
         static constexpr bool same_size = s0.size() == s1.size(); // if we need to check the size again on the second compare
         static constexpr bool check_size = !same_size;
         
         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }
         
         constexpr decltype(auto) at(auto&& key) const
         {
            if constexpr (same_size) {
               constexpr auto n = s0.size();
               if (key.size() != n) {
                  throw std::runtime_error("Invalid key");
               }
            }
            
            if (cx_string_cmp<s0, check_size>(key)) {
               return items[0].second;
            }
            else if (cx_string_cmp<s1, check_size>(key)) {
               return items[1].second;
            }
            else [[unlikely]] {
               throw std::runtime_error("Invalid key");
            }
         }
         
         constexpr decltype(auto) find(auto&& key) const
         {
            if constexpr (same_size) {
               constexpr auto n = s0.size();
               if (key.size() != n) {
                  throw std::runtime_error("Invalid key");
               }
            }
            
            if (cx_string_cmp<s0, check_size>(key)) {
               return items.begin();
            }
            else if (cx_string_cmp<s1, check_size>(key)) {
               return items.begin() + 1;
            }
            else [[unlikely]] {
               return items.end();
            }
         }
      };
   }
}
