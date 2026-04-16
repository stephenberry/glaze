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
#include "glaze/jsonb/header.hpp"
#include "glaze/jsonb/skip.hpp"
#include "glaze/util/compare.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/parse.hpp"

namespace glz
{
   namespace jsonb_detail
   {
      // Decode a JSON-style escape (TEXTJ) starting at `it` (pointing at the char AFTER the
      // backslash). Appends the decoded byte(s) to `out` and advances `it` past the escape.
      // Returns false on malformed escape.
      template <class It>
      [[nodiscard]] inline bool decode_json_escape(It& it, It end, std::string& out) noexcept
      {
         if (it >= end) return false;
         const char c = static_cast<char>(*it);
         ++it;
         switch (c) {
         case '"':
            out.push_back('"');
            return true;
         case '\\':
            out.push_back('\\');
            return true;
         case '/':
            out.push_back('/');
            return true;
         case 'b':
            out.push_back('\b');
            return true;
         case 'f':
            out.push_back('\f');
            return true;
         case 'n':
            out.push_back('\n');
            return true;
         case 'r':
            out.push_back('\r');
            return true;
         case 't':
            out.push_back('\t');
            return true;
         case 'u': {
            // 4 hex digits, possibly followed by a `\uXXXX` surrogate pair.
            auto hex4 = [](It p) -> int32_t {
               uint32_t v = 0;
               for (int i = 0; i < 4; ++i) {
                  const char ch = static_cast<char>(p[i]);
                  uint32_t d;
                  if (ch >= '0' && ch <= '9')
                     d = ch - '0';
                  else if (ch >= 'a' && ch <= 'f')
                     d = ch - 'a' + 10;
                  else if (ch >= 'A' && ch <= 'F')
                     d = ch - 'A' + 10;
                  else
                     return -1;
                  v = (v << 4) | d;
               }
               return static_cast<int32_t>(v);
            };
            if ((end - it) < 4) return false;
            int32_t high = hex4(it);
            if (high < 0) return false;
            it += 4;
            uint32_t code_point = static_cast<uint32_t>(high);

            using namespace glz::unicode;
            if ((code_point & generic_surrogate_mask) == generic_surrogate_value) {
               if ((code_point & surrogate_mask) != high_surrogate_value) return false;
               if ((end - it) < 6) return false;
               if (static_cast<char>(it[0]) != '\\' || static_cast<char>(it[1]) != 'u') return false;
               it += 2;
               const int32_t low = hex4(it);
               if (low < 0) return false;
               it += 4;
               if ((static_cast<uint32_t>(low) & surrogate_mask) != low_surrogate_value) return false;
               code_point = ((code_point & surrogate_codepoint_mask) << surrogate_codepoint_bits) |
                            (static_cast<uint32_t>(low) & surrogate_codepoint_mask);
               code_point += surrogate_codepoint_offset;
            }

            char utf8[4];
            const uint32_t n = glz::code_point_to_utf8(code_point, utf8);
            if (n == 0) return false;
            out.append(utf8, n);
            return true;
         }
         default:
            return false;
         }
      }

      // Decode a JSON5-style escape. Handles all JSON escapes plus \x, \', line continuations,
      // \0, and \v.
      template <class It>
      [[nodiscard]] inline bool decode_json5_escape(It& it, It end, std::string& out) noexcept
      {
         if (it >= end) return false;
         const char c = static_cast<char>(*it);
         switch (c) {
         case '\'':
            ++it;
            out.push_back('\'');
            return true;
         case 'v':
            ++it;
            out.push_back('\v');
            return true;
         case '0':
            ++it;
            out.push_back('\0');
            return true;
         case 'x': {
            ++it;
            if ((end - it) < 2) return false;
            auto hex = [](char ch) -> int {
               if (ch >= '0' && ch <= '9') return ch - '0';
               if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
               if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
               return -1;
            };
            const int hi = hex(static_cast<char>(it[0]));
            const int lo = hex(static_cast<char>(it[1]));
            if (hi < 0 || lo < 0) return false;
            out.push_back(static_cast<char>((hi << 4) | lo));
            it += 2;
            return true;
         }
         case '\n':
            // Line continuation: \<LF> — consume, emit nothing.
            ++it;
            return true;
         case '\r':
            ++it;
            if (it < end && static_cast<char>(*it) == '\n') ++it; // eat CRLF
            return true;
         default:
            return decode_json_escape(it, end, out);
         }
      }

      // Read string bytes [it, it+size) into an out-string, applying the appropriate unescape
      // rule for the type code.
      template <class It>
      inline void decode_text(is_context auto& ctx, uint8_t type_code, It it, It end, size_t size,
                              std::string& out) noexcept
      {
         if (static_cast<size_t>(end - it) < size) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const It stop = it + size;

         if (type_code == jsonb::type::text || type_code == jsonb::type::textraw) {
            out.assign(reinterpret_cast<const char*>(it), size);
            return;
         }

         out.clear();
         out.reserve(size);
         while (it < stop) {
            const char c = static_cast<char>(*it);
            if (c == '\\') {
               ++it;
               if (it >= stop) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               const bool ok = (type_code == jsonb::type::textj) ? decode_json_escape(it, stop, out)
                                                                  : decode_json5_escape(it, stop, out);
               if (!ok) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            else {
               out.push_back(c);
               ++it;
            }
         }
      }

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
            // JSON5: allow leading '+', hex (0x...), leading '.', NaN, Infinity aren't ints.
            if (*start == '+') ++start;
            // Detect hex.
            if ((stop - start) >= 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
               start += 2;
               unsigned long long tmp = 0;
               auto [ptr, ec] = std::from_chars(start, stop, tmp, 16);
               if (ec != std::errc{} || ptr != stop) [[unlikely]] {
                  ctx.error = error_code::parse_number_failure;
                  return;
               }
               // The sign is implicit in INT5: a leading '-' would have appeared before the 0x.
               out = static_cast<T>(tmp);
               return;
            }
         }

         // Default: decimal.
         if constexpr (std::is_signed_v<T>) {
            long long tmp = 0;
            auto [ptr, ec] = std::from_chars(start, stop, tmp, 10);
            if (ec != std::errc{} || ptr != stop) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            out = static_cast<T>(tmp);
         }
         else {
            unsigned long long tmp = 0;
            auto [ptr, ec] = std::from_chars(start, stop, tmp, 10);
            if (ec != std::errc{} || ptr != stop) [[unlikely]] {
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
         auto [ptr, ec] = std::from_chars(start, stop, tmp);
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

   // Null
   template <always_null_t T>
   struct from<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto& ctx, auto& it, auto end) noexcept
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (tc != jsonb::type::null_ || sz != 0) [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
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

   // Boolean
   template <boolean_like T>
   struct from<JSONB, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto& ctx, auto& it, auto end) noexcept
      {
         uint8_t tc{};
         uint64_t sz{};
         if (!jsonb::read_header(ctx, it, end, tc, sz)) return;
         if (sz != 0) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         if (tc == jsonb::type::true_) {
            value = true;
         }
         else if (tc == jsonb::type::false_) {
            value = false;
         }
         else [[unlikely]] {
            ctx.error = error_code::syntax_error;
         }
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

         // Helper: linear scan on an already-materialized key (used for TEXTJ/TEXT5 keys whose
         // payloads must be unescape-decoded before comparison).
         auto linear_match_and_parse = [&](sv key) {
            bool matched = false;
            for_each<N>([&]<size_t I>() {
               if (matched || bool(ctx.error)) return;
               static constexpr auto TargetKey = get<I>(reflect<T>::keys);
               static constexpr auto Length = TargetKey.size();
               if (Length == key.size() && std::memcmp(TargetKey.data(), key.data(), Length) == 0) {
                  matched = true;
                  if constexpr (reflectable<T>) {
                     parse<JSONB>::op<Opts>(get_member(value, get<I>(to_tie(value))), ctx, it, end);
                  }
                  else {
                     parse<JSONB>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
                  }
               }
            });
            return matched;
         };

         std::string key_scratch;
         while (it < stop) {
            // Read key header directly so we can branch on TEXT variant and take the
            // hash-accelerated fast path for literal TEXT/TEXTRAW keys.
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
                  // Fast path: hash the first few bytes directly at `it` without copying.
                  static constexpr auto HashInfo = hash_info<T>;
                  if constexpr (HashInfo.N > 0) {
                     const auto index = decode_hash_with_size<JSONB, T, HashInfo, HashInfo.type>::op(
                        reinterpret_cast<const char*>(it), reinterpret_cast<const char*>(stop),
                        static_cast<size_t>(key_sz));
                     if (index < N) [[likely]] {
                        const sv key{reinterpret_cast<const char*>(it), static_cast<size_t>(key_sz)};
                        it += key_sz;
                        visit<N>(
                           [&]<size_t I>() {
                              static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                              static constexpr auto Length = TargetKey.size();
                              if ((Length == key.size()) &&
                                  compare<Length>(TargetKey.data(), key.data())) [[likely]] {
                                 matched = true;
                                 if constexpr (reflectable<T>) {
                                    parse<JSONB>::op<Opts>(get_member(value, get<I>(to_tie(value))), ctx, it, end);
                                 }
                                 else {
                                    parse<JSONB>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it,
                                                           end);
                                 }
                              }
                           },
                           index);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else {
                        // Hash did not find a candidate — unknown key. Advance past key bytes
                        // and skip the value below.
                        it += key_sz;
                     }
                  }
                  else {
                     // No hash info (degenerate case): fall back to linear scan.
                     const sv key{reinterpret_cast<const char*>(it), static_cast<size_t>(key_sz)};
                     it += key_sz;
                     matched = linear_match_and_parse(key);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
               }
               else {
                  // Escaped key: must decode before comparing.
                  jsonb_detail::decode_text(ctx, key_tc, it, stop, static_cast<size_t>(key_sz), key_scratch);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  it += key_sz;
                  matched = linear_match_and_parse(sv{key_scratch.data(), key_scratch.size()});
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               if (!matched) {
                  if constexpr (Opts.error_on_unknown_keys) {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
                  else {
                     skip_value<JSONB>::op<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
               }
            }
            else if constexpr (Opts.error_on_unknown_keys) {
               ctx.error = error_code::unknown_key;
               return;
            }
            else {
               it += key_sz; // skip key bytes
               skip_value<JSONB>::op<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }

         if (it != stop) [[unlikely]] {
            ctx.error = error_code::syntax_error;
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

         if (jsonb::get_type(initial) == jsonb::type::null_ && jsonb::get_size_nibble(initial) == 0) {
            ++it;
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
         if (jsonb::get_type(initial) == jsonb::type::null_ && jsonb::get_size_nibble(initial) == 0) {
            ++it;
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

   // Variants — expect a 2-element array [index, value]
   template <is_variant T>
   struct from<JSONB, T> final
   {
      template <auto Opts>
      static void op(auto& value, is_context auto& ctx, auto& it, auto end)
      {
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

   // ===== High-level read APIs =====

   template <read_supported<JSONB> T, class Buffer>
   [[nodiscard]] inline error_ctx read_jsonb(T&& value, Buffer&& buffer)
   {
      return read<opts{.format = JSONB}>(value, std::forward<Buffer>(buffer));
   }

   template <read_supported<JSONB> T, class Buffer>
   [[nodiscard]] inline expected<T, error_ctx> read_jsonb(Buffer&& buffer)
   {
      T value{};
      const auto pe = read<opts{.format = JSONB}>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
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
         return error_ctx{file_error};
      }
      return read<set_jsonb<Opts>()>(value, buffer, ctx);
   }
}
