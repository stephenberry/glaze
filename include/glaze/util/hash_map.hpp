#include <array>
#include <cassert>
#include <limits>
#include <span>
#include <vector>

#include <frozen/random.h>

namespace glz
{
   namespace detail
   {
      constexpr size_t hash_string(auto &&value, size_t seed)
      {
         size_t d = (0x811c9dc5 ^ seed) * static_cast<size_t>(0x01000193);
         for (const auto &c : value) d = (d ^ static_cast<size_t>(c)) * static_cast<size_t>(0x01000193);
         return d >> 8;
      }

      constexpr bool contains(auto &&data, auto &&val)
      {
         for (std::size_t i = 0; i < data.size(); ++i)
            if (data[i] == val) return true;
         return false;
      }

      template <size_t N>
      constexpr std::size_t naive_perfect_hash(auto &&keys)
      {
         constexpr auto m = 2 * N;
         std::array<size_t, N> hashes{};
         std::array<size_t, N> buckets{};

         frozen::default_prg_t gen{};
         for (size_t i = 0; i < 10000; ++i) {
            size_t seed = gen();
            size_t index = 0;
            for (const auto &key : keys) {
               auto hash = hash_string(key, seed);
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

      template <class Value, std::size_t N, bool allow_hash_check = false>
      struct naive_map
      {
         static constexpr size_t m = 2 * N;
         size_t seed{};
         std::array<std::pair<std::string_view, Value>, N> items{};
         std::array<size_t, N> hashes{};
         std::array<size_t, m> table{};

         constexpr decltype(auto) begin() const { return items.begin(); }
         constexpr decltype(auto) end() const { return items.end(); }

         constexpr decltype(auto) at(auto &&key) const
         {
            const auto hash = hash_string(key, seed);
            const auto index = table[hash % m];
            const auto &item = items[index];
            if constexpr (allow_hash_check) {
               if (hashes[index] != hash) [[unlikely]]
                  throw std::runtime_error("Invalid key");
            }
            else {
               if (item.first != key) [[unlikely]]
                  throw std::runtime_error("Invalid key");
            }
            return item.second;
         }

         constexpr decltype(auto) find(auto &&key) const
         {
            const auto hash = hash_string(key, seed);
            const auto index = table[hash % m];
            if (hashes[index] != hash) return items.end();
            return items.begin() + index;
         }
      };

      template <class T, size_t N, bool allow_hash_check = false>
      constexpr auto make_naive_map(std::initializer_list<std::pair<std::string_view, T>> pairs)
      {
         assert(pairs.size() == N);
         naive_map<T, N> ht{};
         constexpr auto m = 2 * N;

         std::array<std::string_view, N> keys{};
         size_t i = 0;
         for (const auto &pair : pairs) {
            ht.items[i] = pair;
            keys[i] = pair.first;
            ++i;
         }
         ht.seed = naive_perfect_hash<N>(keys);
         if (ht.seed == std::numeric_limits<size_t>::max()) throw std::runtime_error("Unable to find perfect hash.");

         for (size_t i = 0; i < N; ++i) {
            const auto hash = hash_string(keys[i], ht.seed);
            ht.hashes[i] = hash;
            ht.table[hash % m] = i;
         }

         return ht;
      }
   }
}
