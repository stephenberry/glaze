// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/json/generic.hpp"
#include "glaze/jsonb/header.hpp"
#include "glaze/jsonb/skip.hpp"
#include "glaze/jsonb/text_decode.hpp"
#include "glaze/util/bit_array.hpp"
#include "glaze/util/compare.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/fast_float.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/parse.hpp"

namespace glz
{
   namespace jsonb_detail
   {
      // decode_json_escape, decode_json5_escape, and decode_text moved to text_decode.hpp
      // so the JSONB→JSON converter can reuse them.

      // RAII guard that bumps ctx.depth on entry to a container reader and pops it on
      // destruction. Errors with exceeded_max_recursive_depth if the cap (256) would be
      // exceeded — deeply nested blobs can stack-overflow otherwise, and untrusted blobs
      // (e.g. from user-supplied SQLite JSONB columns) are a real DoS vector.
      template <class Ctx>
      struct depth_guard
      {
         Ctx& ctx;
         bool entered = false;

         depth_guard(Ctx& c) : ctx(c)
         {
            if (ctx.depth >= max_recursive_depth_limit) [[unlikely]] {
               ctx.error = error_code::exceeded_max_recursive_depth;
               return;
            }
            ++ctx.depth;
            entered = true;
         }
         ~depth_guard()
         {
            if (entered) --ctx.depth;
         }
         explicit operator bool() const noexcept { return entered; }
      };

      // Parse a JSONB integer payload (INT or INT5) and store into target.
      template <class T>
      inline void parse_int_payload(is_context auto& ctx, uint8_t type_code, const char* p, size_t n, T& out) noexcept
      {
         if (n == 0) [[unlikely]] {
            ctx.error = error_code::parse_number_failure;
            return;
         }

         const char* start = p;
         const char* stop = p + n;

         if (type_code == jsonb::type::int5) {
            // JSON5: allow optional leading sign, followed by decimal digits or a 0x/0X hex
            // prefix. Peek and strip the sign first so the hex branch covers "-0x1A" too
            // (previously only '+' was stripped, so negative hex fell through to decimal
            // parsing and always failed — see converter in jsonb_to_json.hpp for the
            // matching logic).
            bool negative = false;
            if (start < stop && (*start == '+' || *start == '-')) {
               negative = (*start == '-');
               ++start;
            }
            if ((stop - start) >= 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
               start += 2;
               uint64_t mag = 0;
               auto [ptr, ec] = std::from_chars(start, stop, mag, 16);
               if (ec != std::errc{} || ptr != stop) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               if (negative) {
                  if constexpr (std::is_signed_v<T>) {
                     // |min()| == max() + 1 for two's complement; allow that edge case.
                     constexpr auto max_neg_mag = static_cast<uint64_t>((std::numeric_limits<T>::max)()) + 1u;
                     if (mag > max_neg_mag) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                     // Negate via unsigned arithmetic to avoid UB at INT_MIN.
                     out = static_cast<T>(uint64_t{0} - mag);
                  }
                  else {
                     // Unsigned target can't represent a negative value.
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
               }
               else {
                  if (mag > static_cast<uint64_t>((std::numeric_limits<T>::max)())) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
                  out = static_cast<T>(mag);
               }
               return;
            }
            // Not hex — keep the stripped sign so the decimal path below sees it.
            if (negative) {
               --start; // restore the '-' for std::from_chars
            }
         }

         // Default: decimal.
         if constexpr (std::is_signed_v<T>) {
            int64_t tmp = 0;
            auto [ptr, ec] = std::from_chars(start, stop, tmp, 10);
            if (ec != std::errc{} || ptr != stop) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            if (tmp < (std::numeric_limits<T>::min)() || tmp > (std::numeric_limits<T>::max)()) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            out = static_cast<T>(tmp);
         }
         else {
            uint64_t tmp = 0;
            auto [ptr, ec] = std::from_chars(start, stop, tmp, 10);
            if (ec != std::errc{} || ptr != stop) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            if (tmp > (std::numeric_limits<T>::max)()) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            out = static_cast<T>(tmp);
         }
      }

      // Parse a JSONB float/double payload (FLOAT, FLOAT5, INT, or INT5) into a floating target.
      template <class T>
      inline void parse_float_payload(is_context auto& ctx, uint8_t type_code, const char* p, size_t n, T& out) noexcept
      {
         if (n == 0) [[unlikely]] {
            ctx.error = error_code::parse_number_failure;
            return;
         }

         // JSON5-specific sentinel values.
         if (type_code == jsonb::type::float5) {
            sv s{p, n};
            if (s == "NaN") {
               out = std::numeric_limits<T>::quiet_NaN();
               return;
            }
            if (s == "Infinity" || s == "+Infinity") {
               out = std::numeric_limits<T>::infinity();
               return;
            }
            if (s == "-Infinity") {
               out = -std::numeric_limits<T>::infinity();
               return;
            }
         }

         const char* start = p;
         const char* stop = p + n;
         if (type_code == jsonb::type::float5 && *start == '+') ++start;

         double tmp = 0.0;
         // Use fast_float rather than std::from_chars: the floating-point overload of
         // std::from_chars is unavailable on older Apple platforms (pre-macOS 26).
         auto [ptr, ec] = glz::fast_float::from_chars(start, stop, tmp);
         if (ec != std::errc{} || ptr != stop) [[unlikely]] {
            ctx.error = error_code::parse_number_failure;
            return;
         }
         out = static_cast<T>(tmp);
      }
   }

   template <>
   struct parse<JSONB>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         if constexpr (const_value_v<T>) {
            if constexpr (check_error_on_const_read(Opts)) {
               ctx.error = error_code::attempt_const_read;
            }
            else {
               skip_value<JSONB>::op<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), end);
            }
         }
         else {
            using V = std::remove_cvref_t<T>;
            from<JSONB, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                              end);
         }
      }
   };

   // Null. Per the SQLite JSONB spec: "Legacy implementations that see an element type
   // of 0 with a non-zero payload size should continue to interpret that element as 'null'
   // for compatibility." We tolerate the payload bytes (skip past them) rather than
   // rejecting, so a future spec extension that uses the payload won't break us.
   template <always_null_t T>
   struct from<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto& ctx, auto& it, auto end) noexcept
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::null_) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         it += sz;
      }
   };

   // Skip
   template <>
   struct from<JSONB, skip>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
      {
         skip_value<JSONB>::op<Opts>(ctx, args...);
      }
   };

   // Boolean. Per the SQLite JSONB spec: "Legacy implementations that see an element
   // type of 1 [or 2] with a non-zero payload size should continue to interpret that
   // element as 'true' [or 'false'] for compatibility." Tolerate the payload bytes.
   template <boolean_like T>
   struct from<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc == jsonb::type::true_) {
            value = true;
         }
         else if (tc == jsonb::type::false_) {
            value = false;
         }
         else [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         it += sz;
      }
   };

   // Integers
   template <class T>
      requires(std::integral<T> && !std::same_as<T, bool>)
   struct from<JSONB, T>
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         if (tc == jsonb::type::int_ || tc == jsonb::type::int5) {
            jsonb_detail::parse_int_payload(ctx, tc, reinterpret_cast<const char*>(it), static_cast<size_t>(sz), value);
            if (bool(ctx.error)) [[unlikely]]
               return;
            it += sz;
         }
         else if (tc == jsonb::type::float_ || tc == jsonb::type::float5) {
            // Accept floats that happen to be integral.
            double tmp{};
            jsonb_detail::parse_float_payload(ctx, tc, reinterpret_cast<const char*>(it), static_cast<size_t>(sz), tmp);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value = static_cast<T>(tmp);
            it += sz;
         }
         else [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Floating-point
   template <std::floating_point T>
   struct from<JSONB, T>
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         if (tc == jsonb::type::float_ || tc == jsonb::type::float5 || tc == jsonb::type::int_ ||
             tc == jsonb::type::int5) {
            jsonb_detail::parse_float_payload(ctx, tc, reinterpret_cast<const char*>(it), static_cast<size_t>(sz),
                                              value);
            if (bool(ctx.error)) [[unlikely]]
               return;
            it += sz;
         }
         else [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Text strings
   template <str_t T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if constexpr (check_max_string_length(Opts) > 0) {
            if (sz > check_max_string_length(Opts)) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
         }

         if (tc == jsonb::type::text || tc == jsonb::type::textraw) {
            if constexpr (string_view_t<T>) {
               value = {reinterpret_cast<const char*>(it), static_cast<size_t>(sz)};
            }
            else {
               value.assign(reinterpret_cast<const char*>(it), static_cast<size_t>(sz));
            }
            it += sz;
            return;
         }
         if (tc == jsonb::type::textj || tc == jsonb::type::text5) {
            if constexpr (string_view_t<T>) {
               // Can't materialize escape-decoded bytes into a view.
               ctx.error = error_code::syntax_error;
               return;
            }
            else {
               std::string buf;
               jsonb_detail::decode_text(ctx, tc, it, end, static_cast<size_t>(sz), buf);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               value.assign(buf.data(), buf.size());
               it += sz;
               return;
            }
         }
         ctx.error = error_code::syntax_error;
      }
   };

   // Helpers shared by array/object readers: read a key for reflected objects.
   namespace jsonb_detail
   {
      template <class It>
      inline bool read_string_key(is_context auto& ctx, It& it, It end, std::string& scratch, sv& key_out)
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return false;
         if (static_cast<uint64_t>(end - it) < sz) {
            ctx.error = error_code::unexpected_end;
            return false;
         }
         if (tc == jsonb::type::text || tc == jsonb::type::textraw) {
            key_out = sv{reinterpret_cast<const char*>(it), static_cast<size_t>(sz)};
            it += sz;
            return true;
         }
         if (tc == jsonb::type::textj || tc == jsonb::type::text5) {
            decode_text(ctx, tc, it, end, static_cast<size_t>(sz), scratch);
            if (bool(ctx.error)) return false;
            it += sz;
            key_out = sv{scratch.data(), scratch.size()};
            return true;
         }
         ctx.error = error_code::syntax_error;
         return false;
      }
   }

   // Writable arrays (std::vector, std::array, std::deque, etc.)
   template <writable_array_t T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto stop = it + sz;

         using V = range_value_t<std::remove_cvref_t<T>>;

         if constexpr (resizable<T>) {
            value.clear();
            while (it < stop) {
               if constexpr (emplace_backable<T>) {
                  value.emplace_back();
                  parse<JSONB>::op<Opts>(value.back(), ctx, it, end);
               }
               else {
                  V tmp{};
                  parse<JSONB>::op<Opts>(tmp, ctx, it, end);
                  value.push_back(std::move(tmp));
               }
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }
         else {
            // Fixed-size container (std::array). Parse up to capacity.
            size_t i = 0;
            const size_t cap = value.size();
            while (it < stop) {
               if (i >= cap) [[unlikely]] {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }
               parse<JSONB>::op<Opts>(value[i++], ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }

         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Writable maps (string-keyed)
   template <writable_map_t T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         using Key = typename std::remove_cvref_t<T>::key_type;
         static_assert(str_t<Key> || std::same_as<Key, std::string>,
                       "JSONB objects only support string keys (types 7-10).");

         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::object) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto stop = it + sz;

         value.clear();
         std::string key_scratch;
         while (it < stop) {
            sv key{};
            if (!jsonb_detail::read_string_key(ctx, it, stop, key_scratch, key)) return;

            if (it >= stop) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            Key k{};
            if constexpr (std::is_assignable_v<Key&, sv>) {
               k = Key{key.begin(), key.end()};
            }
            else {
               k = Key{key};
            }

            parse<JSONB>::op<Opts>(value[std::move(k)], ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }
         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Pairs — encoded as a single-entry object
   template <pair_t T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(T& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::object) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto stop = it + sz;
         parse<JSONB>::op<Opts>(value.first, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
         parse<JSONB>::op<Opts>(value.second, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Reflected objects
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_read<T>)
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::object) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto stop = it + sz;
         static constexpr auto N = reflect<T>::size;

         // Track which fields have been read. Only materialized when needed — an empty pointer
         // type when both error_on_missing_keys and partial_read are off, so the compiler
         // erases the tracking entirely.
         auto fields = [&]() -> decltype(auto) {
            if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
               return bit_array<N>{};
            }
            else {
               return nullptr;
            }
         }();

         // Hash-based lookup against a key byte-range. Returns true on a full match (key
         // parsed into its member), false if no field matched (caller must skip the value).
         // Advances `it` past the value on match; on miss `it` is unchanged.
         auto hash_match_and_parse = [&](const char* key_data, size_t key_len) {
            if constexpr (N > 0) {
               static constexpr auto HashInfo = hash_info<T>;
               const auto index = decode_hash_with_size<JSONB, T, HashInfo, HashInfo.type>::op(
                  key_data, key_data + key_len, key_len);
               if (index >= N) return false;

               bool matched = false;
               visit<N>(
                  [&]<size_t I>() {
                     static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                     static constexpr auto Length = TargetKey.size();
                     if ((Length == key_len) && compare<Length>(TargetKey.data(), key_data)) [[likely]] {
                        matched = true;
                        if constexpr (reflectable<T>) {
                           parse<JSONB>::op<Opts>(get_member(value, get<I>(to_tie(value))), ctx, it, end);
                        }
                        else {
                           parse<JSONB>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
                        }
                        if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
                           fields[I] = true;
                        }
                     }
                  },
                  index);
               return matched;
            }
            else {
               (void)key_data;
               (void)key_len;
               return false;
            }
         };

         std::string key_scratch;
         while (it < stop) {
            // Read key header first so we can branch on TEXT variant. Literal TEXT/TEXTRAW
            // keys hash directly from the buffer; TEXTJ/TEXT5 keys must be escape-decoded
            // into a scratch string before hashing.
            uint8_t key_tc{};
            uint64_t key_sz{};
            if (!jsonb::read_header(ctx, it, stop, key_tc, key_sz)) return;
            if (static_cast<uint64_t>(stop - it) < key_sz) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            const bool literal_key = (key_tc == jsonb::type::text || key_tc == jsonb::type::textraw);
            const bool escaped_key = (key_tc == jsonb::type::textj || key_tc == jsonb::type::text5);
            if (!literal_key && !escaped_key) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            if constexpr (N > 0) {
               bool matched = false;
               if (literal_key) {
                  const char* key_data = reinterpret_cast<const char*>(it);
                  const size_t key_len = static_cast<size_t>(key_sz);
                  it += key_sz;
                  matched = hash_match_and_parse(key_data, key_len);
               }
               else {
                  jsonb_detail::decode_text(ctx, key_tc, it, stop, static_cast<size_t>(key_sz), key_scratch);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  it += key_sz;
                  matched = hash_match_and_parse(key_scratch.data(), key_scratch.size());
               }
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (!matched) {
                  if constexpr (Opts.error_on_unknown_keys) {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
                  skip_value<JSONB>::op<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
            }
            else if constexpr (Opts.error_on_unknown_keys) {
               ctx.error = error_code::unknown_key;
               return;
            }
            else {
               it += key_sz;
               skip_value<JSONB>::op<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }

         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         if constexpr (Opts.error_on_missing_keys) {
            static constexpr auto req_fields = required_fields<T, Opts>();
            if ((req_fields & fields) != req_fields) {
               for (size_t i = 0; i < N; ++i) {
                  if (!fields[i] && req_fields[i]) {
                     ctx.custom_error_message = reflect<T>::keys[i];
                     break;
                  }
               }
               ctx.error = error_code::missing_key;
            }
         }
      }
   };

   // Tuples
   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto stop = it + sz;

         static constexpr auto N = glz::tuple_size_v<std::decay_t<T>>;
         if constexpr (is_std_tuple<T>) {
            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) return;
               parse<JSONB>::op<Opts>(std::get<I>(value), ctx, it, end);
            });
         }
         else {
            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) return;
               parse<JSONB>::op<Opts>(glz::get<I>(value), ctx, it, end);
            });
         }

         if (bool(ctx.error)) [[unlikely]]
            return;
         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Glaze arrays
   template <class T>
      requires glaze_array_t<T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto stop = it + sz;

         for_each<reflect<T>::size>([&]<size_t I>() {
            if (bool(ctx.error)) return;
            parse<JSONB>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
         });

         if (bool(ctx.error)) [[unlikely]]
            return;
         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Optional / unique_ptr / shared_ptr
   template <nullable_like T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         uint8_t initial;
         std::memcpy(&initial, it, 1);

         // Accept any null element (size 0 or, per spec, non-zero payload that a legacy
         // reader must still interpret as null). Delegate to the null reader so it handles
         // the full header + optional payload skip.
         if (jsonb::get_type(initial) == jsonb::type::null_) {
            std::nullptr_t n;
            from<JSONB, std::nullptr_t>::template op<Opts>(n, ctx, it, end);
            if (bool(ctx.error)) return;
            if constexpr (requires { value.reset(); }) {
               value.reset();
            }
            return;
         }

         if (!value) {
            if constexpr (constructible<T>) {
               value = meta_construct_v<T>();
            }
            else if constexpr (requires { value.emplace(); }) {
               value.emplace();
            }
            else {
               ctx.error = error_code::invalid_nullable_read;
               return;
            }
         }
         parse<JSONB>::op<Opts>(*value, ctx, it, end);
      }
   };

   // nullable_value_t (e.g. std::optional without full nullable_like)
   template <class T>
      requires(nullable_value_t<T> && !nullable_like<T> && !is_expected<T> && !custom_read<T>)
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         uint8_t initial;
         std::memcpy(&initial, it, 1);
         if (jsonb::get_type(initial) == jsonb::type::null_) {
            std::nullptr_t n;
            from<JSONB, std::nullptr_t>::template op<Opts>(n, ctx, it, end);
            if (bool(ctx.error)) return;
            if constexpr (requires { value.reset(); }) {
               value.reset();
            }
            return;
         }
         if (!value.has_value()) {
            if constexpr (constructible<T>) {
               value = meta_construct_v<T>();
            }
            else if constexpr (requires { value.emplace(); }) {
               value.emplace();
            }
            else {
               ctx.error = error_code::invalid_nullable_read;
               return;
            }
         }
         parse<JSONB>::op<Opts>(value.value(), ctx, it, end);
      }
   };

   // std::expected — on read, peek the first payload bytes to decide whether we're looking
   // at an error wrapper ({"unexpected": ...}) or the value itself.
   template <is_expected T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         auto parse_val = [&] {
            if constexpr (not std::is_void_v<typename std::decay_t<T>::value_type>) {
               if (value) {
                  parse<JSONB>::op<Opts>(*value, ctx, it, end);
               }
               else {
                  value.emplace();
                  parse<JSONB>::op<Opts>(*value, ctx, it, end);
               }
            }
            else {
               value.emplace();
            }
         };

         // Peek the outer type code without advancing.
         uint8_t initial;
         std::memcpy(&initial, it, 1);
         if (jsonb::get_type(initial) != jsonb::type::object) {
            // Not an object → must be the value directly.
            parse_val();
            return;
         }

         // It's an object. We need to decide: is it {"unexpected": <error>} or a value that
         // happens to be an object? Save the position, consume the object header, then look
         // at the first key.
         const auto start = it;
         uint8_t obj_tc{};
         uint64_t obj_sz{};
         if (!jsonb::read_header(ctx, it, end, obj_tc, obj_sz)) return;
         if (static_cast<uint64_t>(end - it) < obj_sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto payload_end = it + obj_sz;

         if (obj_sz == 0) {
            // Empty object: for void-value expected, emplace success; otherwise rewind and
            // let the value type decide (maybe it's an empty object-typed value).
            if constexpr (std::is_void_v<typename std::decay_t<T>::value_type>) {
               value.emplace();
               return;
            }
            it = start;
            parse_val();
            return;
         }

         // Try to read the first key. If it's TEXT/TEXTRAW equal to "unexpected", this is an
         // error wrapper. Otherwise rewind to `start` and parse as value.
         auto key_scan_it = it;
         uint8_t key_tc{};
         uint64_t key_sz{};
         if (!jsonb::read_header(ctx, key_scan_it, payload_end, key_tc, key_sz)) return;

         static constexpr sv unexpected_key = "unexpected";
         const bool is_literal_text = (key_tc == jsonb::type::text || key_tc == jsonb::type::textraw);
         if (is_literal_text && key_sz == unexpected_key.size() &&
             static_cast<uint64_t>(payload_end - key_scan_it) >= key_sz &&
             std::memcmp(key_scan_it, unexpected_key.data(), key_sz) == 0) {
            // It's an unexpected wrapper. Advance past the key and parse the error value.
            key_scan_it += key_sz;
            it = key_scan_it;
            using error_type = typename std::decay_t<T>::error_type;
            std::decay_t<error_type> error{};
            parse<JSONB>::op<Opts>(error, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            if (it != payload_end) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            value = glz::unexpected(std::move(error));
            return;
         }

         // Not an unexpected wrapper. Rewind and parse as the value.
         it = start;
         parse_val();
      }
   };

   // Variants — expect a 2-element array [index, value]
   template <is_variant T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         jsonb_detail::depth_guard g{ctx};
         if (!g) return;
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (static_cast<uint64_t>(end - it) < sz) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const auto stop = it + sz;

         uint64_t type_index{};
         parse<JSONB>::op<Opts>(type_index, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;

         using V = std::decay_t<decltype(value)>;
         if (type_index >= std::variant_size_v<V>) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         if (value.index() != type_index) {
            emplace_runtime_variant(value, type_index);
         }

         std::visit([&](auto& v) { parse<JSONB>::op<Opts>(v, ctx, it, end); }, value);
         if (bool(ctx.error)) [[unlikely]]
            return;
         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   // Glaze value wrapper
   template <class T>
      requires(glaze_value_t<T> && !custom_read<T>)
   struct from<JSONB, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         from<JSONB, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                           std::forward<Ctx>(ctx), std::forward<It0>(it), end);
      }
   };

   // Enums
   template <glaze_enum_t T>
   struct from<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         using V = std::underlying_type_t<std::decay_t<T>>;
         V tmp{};
         from<JSONB, V>::template op<Opts>(tmp, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
         value = static_cast<std::decay_t<T>>(tmp);
      }
   };

   template <class T>
      requires(std::is_enum_v<T> && !glaze_enum_t<T>)
   struct from<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         using V = std::underlying_type_t<std::decay_t<T>>;
         V tmp{};
         from<JSONB, V>::template op<Opts>(tmp, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
         value = static_cast<std::decay_t<T>>(tmp);
      }
   };

   // Member function pointers
   template <is_member_function_pointer T>
   struct from<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&, auto&&) noexcept
      {}
   };

   // Hidden
   template <>
   struct from<JSONB, hidden>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
      {
         ctx.error = error_code::attempt_read_hidden;
      }
   };

   // Generic JSON value — peek the type code and dispatch to the appropriate variant alternative.
   template <num_mode Mode, template <class> class MapType>
   struct from<JSONB, generic_json<Mode, MapType>> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         using G = std::decay_t<decltype(value)>;
         using array_t = typename G::array_t;
         using object_t = typename G::object_t;

         // Peek the type code without advancing.
         uint8_t initial;
         std::memcpy(&initial, it, 1);
         const uint8_t tc = jsonb::get_type(initial);

         switch (tc) {
         case jsonb::type::null_: {
            std::nullptr_t n;
            from<JSONB, std::nullptr_t>::template op<Opts>(n, ctx, it, end);
            if (bool(ctx.error)) return;
            value.data = nullptr;
            return;
         }
         case jsonb::type::true_:
         case jsonb::type::false_: {
            bool b = false;
            from<JSONB, bool>::template op<Opts>(b, ctx, it, end);
            if (bool(ctx.error)) return;
            value.data = b;
            return;
         }
         case jsonb::type::int_:
         case jsonb::type::int5: {
            // Prefer the widest integer alternative available in this Mode, fall back to double.
            if constexpr (Mode == num_mode::u64) {
               uint64_t u{};
               from<JSONB, uint64_t>::template op<Opts>(u, ctx, it, end);
               if (bool(ctx.error)) return;
               value.data = u;
            }
            else if constexpr (Mode == num_mode::i64) {
               int64_t i{};
               from<JSONB, int64_t>::template op<Opts>(i, ctx, it, end);
               if (bool(ctx.error)) return;
               value.data = i;
            }
            else {
               double d{};
               from<JSONB, double>::template op<Opts>(d, ctx, it, end);
               if (bool(ctx.error)) return;
               value.data = d;
            }
            return;
         }
         case jsonb::type::float_:
         case jsonb::type::float5: {
            double d{};
            from<JSONB, double>::template op<Opts>(d, ctx, it, end);
            if (bool(ctx.error)) return;
            value.data = d;
            return;
         }
         case jsonb::type::text:
         case jsonb::type::textj:
         case jsonb::type::text5:
         case jsonb::type::textraw: {
            std::string s;
            from<JSONB, std::string>::template op<Opts>(s, ctx, it, end);
            if (bool(ctx.error)) return;
            value.data = std::move(s);
            return;
         }
         case jsonb::type::array: {
            array_t a;
            from<JSONB, array_t>::template op<Opts>(a, ctx, it, end);
            if (bool(ctx.error)) return;
            value.data = std::move(a);
            return;
         }
         case jsonb::type::object: {
            object_t o;
            from<JSONB, object_t>::template op<Opts>(o, ctx, it, end);
            if (bool(ctx.error)) return;
            value.data = std::move(o);
            return;
         }
         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   };

   // ===== High-level read APIs =====
   //
   // Per the SQLite JSONB spec: "A valid JSONB BLOB consists of a single JSON element.
   // The element must exactly fill the BLOB." Glaze's core parse only advances across
   // one element; trailing bytes after a valid top-level element would otherwise be
   // silently ignored. Enforce the spec at the public boundary by comparing bytes
   // consumed (ec.count) against the input size.
   namespace jsonb_detail
   {
      template <class Buffer>
      GLZ_ALWAYS_INLINE error_ctx enforce_exact_fill(const Buffer& buffer, error_ctx ec) noexcept
      {
         if (!ec && ec.count != buffer.size()) [[unlikely]] {
            return {ec.count, error_code::syntax_error};
         }
         return ec;
      }
   }

   template <read_supported<JSONB> T, class Buffer>
   [[nodiscard]] inline error_ctx read_jsonb(T&& value, Buffer&& buffer)
   {
      auto ec = read<opts{.format = JSONB}>(value, buffer);
      return jsonb_detail::enforce_exact_fill(buffer, ec);
   }

   template <read_supported<JSONB> T, class Buffer>
   [[nodiscard]] inline expected<T, error_ctx> read_jsonb(Buffer&& buffer)
   {
      T value{};
      auto ec = read<opts{.format = JSONB}>(value, buffer);
      ec = jsonb_detail::enforce_exact_fill(buffer, ec);
      if (ec) [[unlikely]] {
         return unexpected(ec);
      }
      return value;
   }

   template <auto Opts = opts{}, read_supported<JSONB> T>
   [[nodiscard]] inline error_ctx read_file_jsonb(T& value, const sv file_name, auto&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;
      const auto file_error = file_to_buffer(buffer, ctx.current_file);
      if (bool(file_error)) [[unlikely]] {
         return error_ctx{0, file_error};
      }
      auto ec = read<set_jsonb<Opts>()>(value, buffer, ctx);
      return jsonb_detail::enforce_exact_fill(buffer, ec);
   }
}
