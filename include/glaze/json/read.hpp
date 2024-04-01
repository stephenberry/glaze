// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <climits>
#include <cwchar>
#include <deque>
#include <filesystem>
#include <iterator>
#include <locale>
#include <ranges>
#include <type_traits>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/json/json_t.hpp"
#include "glaze/json/skip.hpp"
#include "glaze/reflection/reflect.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/strod.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{

   // forward declare from json/quoted.hpp to avoid circular include
   template <class T>
   struct quoted_t;

   namespace detail
   {
      // Unless we can mutate the input buffer we need somewhere to store escaped strings for key lookup, etc.
      // We don't put this in the context because we don't want to continually reallocate.
      GLZ_ALWAYS_INLINE std::string& string_buffer() noexcept
      {
         static thread_local std::string buffer(256, ' ');
         return buffer;
      }

      // We use an error buffer to avoid multiple allocations in the case that errors occur multiple times.
      GLZ_ALWAYS_INLINE std::string& error_buffer() noexcept
      {
         static thread_local std::string buffer(256, ' ');
         return buffer;
      }

      template <class T = void>
      struct from_json
      {};

      template <auto Opts, class T, class Ctx, class It0, class It1>
      concept read_json_invocable = requires(T&& value, Ctx&& ctx, It0&& it, It1&& end) {
         from_json<std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                              std::forward<It0>(it), std::forward<It1>(end));
      };

      template <>
      struct read<json>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
               if constexpr (Opts.error_on_const_read) {
                  ctx.error = error_code::attempt_const_read;
               }
               else {
                  // do not read anything into the const value
                  skip_value<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
               }
            }
            else {
               if constexpr (read_json_invocable<Opts, T, Ctx, It0, It1>) {
                  using V = std::remove_cvref_t<T>;
                  from_json<V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                                  std::forward<It1>(end));
               }
               else {
                  static_assert(false_v<T>, "Glaze metadata is probably needed for your type");
               }
            }
         }

         // This unknown key handler should not be given unescaped keys, that is for the user to handle.
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void handle_unknown(const sv& key, T&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            using ValueType = std::decay_t<decltype(value)>;
            if constexpr (detail::has_unknown_reader<ValueType>) {
               constexpr auto& reader = meta_unknown_read_v<ValueType>;
               using ReaderType = meta_unknown_read_t<ValueType>;
               if constexpr (std::is_member_object_pointer_v<ReaderType>) {
                  using MemberType = typename member_value<ReaderType>::type;
                  if constexpr (detail::map_subscriptable<MemberType>) {
                     read<json>::op<Opts>((value.*reader)[key], ctx, it, end);
                  }
                  else {
                     static_assert(false_v<T>, "target must have subscript operator");
                  }
               }
               else if constexpr (std::is_member_function_pointer_v<ReaderType>) {
                  using ReturnType = typename return_type<ReaderType>::type;
                  if constexpr (std::is_void_v<ReturnType>) {
                     using TupleType = typename inputs_as_tuple<ReaderType>::type;
                     if constexpr (std::tuple_size_v<TupleType> == 2) {
                        std::decay_t<std::tuple_element_t<1, TupleType>> input{};
                        read<json>::op<Opts>(input, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        (value.*reader)(key, input);
                     }
                     else {
                        static_assert(false_v<T>, "method must have 2 args");
                     }
                  }
                  else {
                     static_assert(false_v<T>, "method must have void return");
                  }
               }
               else {
                  static_assert(false_v<T>, "unknown_read type not handled");
               }
            }
            else {
               skip_value<Opts>(ctx, it, end);
            }
         }
      };

      template <class T>
         requires(glaze_value_t<T> && !specialized_with_custom_read<T>)
      struct from_json<T>
      {
         template <auto Opts, class Value, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
            from_json<V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                            std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };

      template <is_member_function_pointer T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
         {
            ctx.error = error_code::attempt_member_func_read;
         }
      };

      template <is_bitset T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            match<'"'>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;

            const auto n = value.size();
            for (size_t i = 1; it < end; ++i, ++it) {
               if (*it == '"') {
                  ++it;
                  return;
               }

               if (i > n) {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }

               if (*it == '0') {
                  value[n - i] = 0;
               }
               else if (*it == '1') {
                  value[n - i] = 1;
               }
               else [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            ctx.error = error_code::expected_quote;
         }
      };

      template <>
      struct from_json<skip>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
         {
            skip_value<Opts>(ctx, args...);
         }
      };

      template <is_reference_wrapper T>
      struct from_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args) noexcept
         {
            using V = std::decay_t<decltype(value.get())>;
            from_json<V>::template op<Opts>(value.get(), std::forward<Args>(args)...);
         }
      };

      template <>
      struct from_json<hidden>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
         {
            ctx.error = error_code::attempt_read_hidden;
         }
      };

      template <complex_t T>
      struct from_json<T>
      {
         template <auto Options>
         GLZ_ALWAYS_INLINE static void op(auto&& v, is_context auto&& ctx, auto&&... args) noexcept
         {
            if constexpr (!Options.ws_handled) {
               skip_ws<Options>(ctx, args...);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            match<"[">(ctx, args...);
            if (bool(ctx.error)) [[unlikely]]
               return;

            constexpr auto Opts = ws_handled_off<Options>();

            auto* ptr = reinterpret_cast<typename T::value_type*>(&v);
            static_assert(sizeof(T) == sizeof(typename T::value_type) * 2);
            read<json>::op<Opts>(ptr[0], ctx, args...);

            skip_ws<Opts>(ctx, args...);
            if (bool(ctx.error)) [[unlikely]]
               return;

            match<",">(ctx, args...);
            if (bool(ctx.error)) [[unlikely]]
               return;

            read<json>::op<Opts>(ptr[1], ctx, args...);

            skip_ws<Opts>(ctx, args...);
            if (bool(ctx.error)) [[unlikely]]
               return;
            match<"]">(ctx, args...);
         }
      };

      template <always_null_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
         {
            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, args...);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            match<"null">(ctx, args...);
         }
      };

      template <bool_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(bool_t auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (Opts.quoted_num) {
               skip_ws_no_pre_check<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               match<'"'>(ctx, it);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            if (std::distance(it, end) < 4) [[unlikely]] {
               ctx.error = error_code::expected_true_or_false;
               return;
            }

            uint64_t c{};
            // Note that because our buffer must be null terminated, we can read one more index without checking:
            // std::distance(it, end) < 5
            std::memcpy(&c, &*it, 5);
            constexpr uint64_t u_true = 0b00000000'00000000'00000000'00000000'01100101'01110101'01110010'01110100;
            constexpr uint64_t u_false = 0b00000000'00000000'00000000'01100101'01110011'01101100'01100001'01100110;
            // We have to wipe the 5th character for true testing
            if ((c & 0b11111111'11111111'11111111'00000000'11111111'11111111'11111111'11111111) == u_true) {
               value = true;
               it += 4;
            }
            else {
               if (c != u_false) [[unlikely]] {
                  ctx.error = error_code::expected_true_or_false;
                  return;
               }
               value = false;
               it += 5;
            }

            if constexpr (Opts.quoted_num) {
               match<'"'>(ctx, it);
            }
         }
      };

      template <num_t T>
      struct from_json<T>
      {
         template <auto Options, class It>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
         {
            if constexpr (Options.quoted_num) {
               skip_ws<Options>(ctx, it, end);
               match<'"'>(ctx, it);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            if constexpr (!Options.ws_handled) {
               skip_ws<Options>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            using V = std::decay_t<decltype(value)>;
            if constexpr (int_t<V>) {
               static constexpr auto maximum = uint64_t((std::numeric_limits<V>::max)());
               if constexpr (std::is_unsigned_v<V>) {
                  if constexpr (std::same_as<V, uint64_t>) {
                     if (*it == '-') [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }

                     static_assert(sizeof(*it) == sizeof(char));
                     const char* cur = reinterpret_cast<const char*>(&*it);
                     const char* beg = cur;
                     if constexpr (std::is_volatile_v<decltype(value)>) {
                        // Hardware may interact with value changes, so we parse into a temporary and assign in one
                        // place
                        uint64_t i{};
                        auto s = parse_int<uint64_t, Options.force_conformance>(i, cur);
                        if (!s) [[unlikely]] {
                           ctx.error = error_code::parse_number_failure;
                           return;
                        }
                        value = i;
                     }
                     else {
                        auto s =
                           parse_int<decay_keep_volatile_t<decltype(value)>, Options.force_conformance>(value, cur);
                        if (!s) [[unlikely]] {
                           ctx.error = error_code::parse_number_failure;
                           return;
                        }
                     }

                     it += (cur - beg);
                  }
                  else {
                     uint64_t i{};
                     if (*it == '-') [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }

                     static_assert(sizeof(*it) == sizeof(char));
                     const char* cur = reinterpret_cast<const char*>(&*it);
                     const char* beg = cur;
                     auto s = parse_int<std::decay_t<decltype(i)>, Options.force_conformance>(i, cur);
                     if (!s) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }

                     if (i > maximum) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                     value = V(i);
                     it += (cur - beg);
                  }
               }
               else {
                  uint64_t i{};
                  int sign = 1;
                  if (*it == '-') {
                     sign = -1;
                     ++it;
                  }

                  static_assert(sizeof(*it) == sizeof(char));
                  const char* cur = reinterpret_cast<const char*>(&*it);
                  const char* beg = cur;
                  auto s = parse_int<decay_keep_volatile_t<decltype(i)>, Options.force_conformance>(i, cur);
                  if (!s) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }

                  if (sign == -1) {
                     static constexpr auto min_abs = uint64_t((std::numeric_limits<V>::max)()) + 1;
                     if (i > min_abs) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                     value = V(sign * i);
                  }
                  else {
                     if (i > maximum) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                     value = V(i);
                  }
                  it += (cur - beg);
               }
            }
            else {
               if constexpr (is_float128<V>) {
                  auto [ptr, ec] = std::from_chars(it, end, value);
                  if (ec != std::errc()) {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
                  it += std::distance(it, ptr);
               }
               else {
                  if constexpr (std::is_volatile_v<decltype(value)>) {
                     // Hardware may interact with value changes, so we parse into a temporary and assign in one place
                     V temp;
                     auto s = parse_float<V, Options.force_conformance>(value, it);
                     if (!s) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                     value = temp;
                  }
                  else {
                     auto s = parse_float<V, Options.force_conformance>(value, it);
                     if (!s) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                  }
               }
            }

            if constexpr (Options.quoted_num) {
               match<'"'>(ctx, it);
            }
         }
      };

      template <class T, class Val, class It, class End>
      GLZ_ALWAYS_INLINE void read_escaped_unicode(Val& value, is_context auto&& ctx, It&& it, End&& end)
      {
         // This is slow but who is escaping unicode nowadays
         // codecvt is problematic on mingw hence mixing with the c character conversion functions
         if (std::distance(it, end) < 4 || !std::all_of(it, it + 4, ::isxdigit)) [[unlikely]] {
            ctx.error = error_code::u_requires_hex_digits;
            return;
         }
         if constexpr (std::is_same_v<T, char32_t>) {
            if constexpr (char_t<Val>) {
               value = hex4_to_char32(it);
            }
            else {
               value.push_back(hex4_to_char32(it));
            }
         }
         else {
            char32_t codepoint = hex4_to_char32(it);
            if constexpr (std::is_same_v<T, char16_t>) {
               if (codepoint < 0x10000) {
                  if constexpr (char_t<Val>) {
                     value = static_cast<T>(codepoint);
                  }
                  else {
                     value.push_back(static_cast<T>(codepoint));
                  }
               }
               else {
                  if constexpr (char_t<Val>) {
                     ctx.error = error_code::unicode_escape_conversion_failure;
                     return;
                  }
                  else {
                     const auto t = codepoint - 0x10000;
                     const auto high = static_cast<T>(((t << 12) >> 22) + 0xD800);
                     const auto low = static_cast<T>(((t << 22) >> 22) + 0xDC00);
                     value.push_back(high);
                     value.push_back(low);
                  }
               }
            }
            else {
               char8_t buffer[4];
               auto& facet = std::use_facet<std::codecvt<char32_t, char8_t, mbstate_t>>(std::locale());
               std::mbstate_t mbstate{};
               const char32_t* from_next;
               char8_t* to_next;
               const auto result =
                  facet.out(mbstate, &codepoint, &codepoint + 1, from_next, buffer, buffer + 4, to_next);
               if (result != std::codecvt_base::ok) {
                  ctx.error = error_code::unicode_escape_conversion_failure;
                  return;
               }

               if constexpr (std::is_same_v<T, char> || std::is_same_v<T, char8_t>) {
                  if constexpr (char_t<Val>) {
                     if ((to_next - buffer) != 1) [[unlikely]] {
                        ctx.error = error_code::unicode_escape_conversion_failure;
                        return;
                     }
                     value = static_cast<T>(buffer[0]);
                  }
                  else {
                     value.append(reinterpret_cast<T*>(buffer), to_next - buffer);
                  }
               }
               else if constexpr (std::is_same_v<T, wchar_t>) {
                  static_assert(false_v<T>, "wchar_t is unsupported, JSON requires UTF8");
               }
            }
         }

         std::advance(it, 4);
      }

      template <string_t T>
      struct from_json<T>
      {
         template <auto Opts, class It, class End>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, It&& it, End&& end) noexcept
         {
            if constexpr (Opts.number) {
               auto start = it;
               skip_number<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               value.append(start, size_t(it - start));
            }
            else {
               if constexpr (!Opts.opening_handled) {
                  if constexpr (!Opts.ws_handled) {
                     skip_ws<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }

                  match<'"'>(ctx, it);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               // overwrite portion

               if constexpr (Opts.raw_string) {
                  auto start = it;
                  skip_till_quote(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  value = sv{start, size_t(it - start)};
                  ++it;
               }
               else {
                  auto start = it;

                  const bool escaped = skip_till_unescaped_quote<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  if (escaped) {
                     static constexpr auto Bytes = 8;

                     // The null character may be the first byte of an 8 byte uint64_t SWAR chunk
                     // so we need to at least add 7 bytes. We add 8 here, because why not.
                     const auto length = round_up_to_multiple<Bytes>(size_t(it - start)) + Bytes;
                     value.resize(length);

                     const char* c;
                     if (length < size_t(end - it)) [[likely]] {
                        c = parse_string<Bytes>(&*start, value.data(), ctx);
                     }
                     else [[unlikely]] {
                        c = parse_string<1>(&*start, value.data(), ctx);
                     }

                     if (bool(ctx.error)) [[unlikely]] {
                        it = c;
                        return;
                     }

                     value.resize(size_t(c - value.data()));
                  }
                  else {
                     value = sv{start, size_t(it - start)};
                  }

                  ++it;
               }
            }
         }
      };

      template <class T>
         requires(str_view_t<T> || char_array_t<T>)
      struct from_json<T>
      {
         template <auto Opts, class It, class End>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, It&& it, End&& end) noexcept
         {
            if constexpr (!Opts.opening_handled) {
               if constexpr (!Opts.ws_handled) {
                  skip_ws<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               match<'"'>(ctx, it);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            auto handle_escaped = [&]() {
               switch (*it) {
               case '"':
               case '\\':
               case '/':
               case 'b':
               case 'f':
               case 'n':
               case 'r':
               case 't':
               case 'u': {
                  ++it;
                  break;
               }
               default: {
                  ctx.error = error_code::invalid_escape;
                  return;
               }
               }
            };

            auto start = it;
            [[maybe_unused]] auto write_to_char_buffer = [&] {
               if constexpr (char_array_t<T>) {
                  const size_t n = it - start - 1;
                  sv str{start, n};

                  if ((sizeof(value) - 1) < n) {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  for (size_t i = 0; i < n; ++i) {
                     value[i] = str[i];
                  }
                  value[n] = '\0';
               }
            };

            while (it < end) {
               if constexpr (!Opts.force_conformance) {
                  skip_till_escape_or_quote(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  if (*it == '"') {
                     ++it;
                     if constexpr (str_view_t<T>) {
                        value = std::string_view{start, size_t(it - start - 1)};
                     }
                     else if constexpr (char_array_t<T>) {
                        write_to_char_buffer();
                     }
                     return;
                  }
                  else {
                     ++it;
                     handle_escaped();
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
               }
               else {
                  switch (*it) {
                  case '"': {
                     ++it;
                     return;
                  }
                  case '\b':
                  case '\f':
                  case '\n':
                  case '\r':
                  case '\t': {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  case '\0': {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  case '\\': {
                     ++it;
                     handle_escaped();
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     if constexpr (str_view_t<T>) {
                        value = std::string_view{start, size_t(it - start - 1)};
                     }
                     else if constexpr (char_array_t<T>) {
                        write_to_char_buffer();
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     break;
                  }
                  default:
                     ++it;
                  }
               }
            }
         }
      };

      template <char_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Opts.opening_handled) {
               if constexpr (!Opts.ws_handled) {
                  skip_ws<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               match<'"'>(ctx, it);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            if (*it == '\\') [[unlikely]] {
               ++it;
               switch (*it) {
               case '\0': {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               case '"':
               case '\\':
               case '/':
                  value = *it++;
                  break;
               case 'b':
                  value = '\b';
                  ++it;
                  break;
               case 'f':
                  value = '\f';
                  ++it;
                  break;
               case 'n':
                  value = '\n';
                  ++it;
                  break;
               case 'r':
                  value = '\r';
                  ++it;
                  break;
               case 't':
                  value = '\t';
                  ++it;
                  break;
               case 'u': {
                  ++it;
                  read_escaped_unicode<T>(value, ctx, it, end);
                  return;
               }
               default: {
                  ctx.error = error_code::invalid_escape;
                  return;
               }
               }
            }
            else {
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               value = *it++;
            }
            match<'"'>(ctx, it);
         }
      };

      template <class T>
         requires(glaze_enum_t<T> && !specialized_with_custom_read<T>)
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            const auto key = parse_key(ctx, it, end); // TODO: Use more optimal enum key parsing
            if (bool(ctx.error)) [[unlikely]]
               return;

            static constexpr auto frozen_map = detail::make_string_to_enum_map<T>();
            const auto& member_it = frozen_map.find(key);
            if (member_it != frozen_map.end()) {
               value = member_it->second;
            }
            else [[unlikely]] {
               ctx.error = error_code::unexpected_enum;
            }
         }
      };

      template <class T>
         requires(std::is_enum_v<T> && !glaze_enum_t<T> && !specialized_with_custom_read<T>)
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            // read<json>::op<Opts>(*reinterpret_cast<std::underlying_type_t<std::decay_t<decltype(value)>>*>(&value),
            // ctx, it, end);
            std::underlying_type_t<std::decay_t<T>> x{};
            read<json>::op<Opts>(x, ctx, it, end);
            value = static_cast<std::decay_t<T>>(x);
         }
      };

      template <func_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto& /*value*/, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            match<'"'>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
            skip_till_quote(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            match<'"'>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }
      };

      template <class T>
      struct from_json<basic_raw_json<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            auto it_start = it;
            skip_value<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value.str = {it_start, static_cast<size_t>(it - it_start)};
         }
      };

      template <class T>
      struct from_json<basic_text<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& it, auto&& end) noexcept
         {
            value.str = {it, static_cast<size_t>(end - it)}; // read entire contents as string
            it = end;
         }
      };

      // for set types
      template <class T>
         requires(readable_array_t<T> && !emplace_backable<T> && !resizeable<T> && emplaceable<T>)
      struct from_json<T>
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Options.ws_handled) {
               skip_ws<Options>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            constexpr auto Opts = ws_handled_off<Options>();

            match<'['>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
            skip_ws_no_pre_check<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            value.clear();
            if (*it == ']') [[unlikely]] {
               ++it;
               return;
            }

            while (true) {
               using V = range_value_t<T>;
               V v;
               read<json>::op<Opts>(v, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               value.emplace(std::move(v));
               skip_ws_no_pre_check<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (*it == ']') {
                  ++it;
                  return;
               }
               match<','>(ctx, it);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }
      };

      // for types like std::vector, std::array, std::deque, etc.
      template <class T>
         requires(readable_array_t<T> && (emplace_backable<T> || !resizeable<T>) && !emplaceable<T>)
      struct from_json<T>
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Options.ws_handled) {
               skip_ws<Options>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            constexpr auto Opts = ws_handled_off<Options>();

            match<'['>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
            skip_ws_no_pre_check<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            if (*it == ']') [[unlikely]] {
               ++it;
               if constexpr (resizeable<T>) {
                  value.clear();

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }
               return;
            }

            const auto n = value.size();

            auto value_it = value.begin();

            for (size_t i = 0; i < n; ++i) {
               read<json>::op<ws_handled<Opts>()>(*value_it++, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               skip_ws_no_pre_check<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (*it == ',') [[likely]] {
                  ++it;
                  skip_ws_no_pre_check<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else if (*it == ']') {
                  ++it;
                  if constexpr (erasable<T>) {
                     value.erase(value_it,
                                 value.end()); // use erase rather than resize for non-default constructible elements

                     if constexpr (Opts.shrink_to_fit) {
                        value.shrink_to_fit();
                     }
                  }
                  return;
               }
               else [[unlikely]] {
                  ctx.error = error_code::expected_bracket;
                  return;
               }
            }

            // growing
            if constexpr (emplace_backable<T>) {
               if constexpr (has_reserve<T> && has_capacity<T>) {
                  // If we can reserve memmory, like std::vector, then we want to check the capacity
                  // and use a temporary buffer if the capacity needs to grow
                  const auto capacity = value.capacity();
                  for (size_t i = value.size(); i < capacity; ++i) {
                     // emplace_back while we have capacity
                     read<json>::op<ws_handled<Opts>()>(value.emplace_back(), ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     skip_ws_no_pre_check<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     if (*it == ',') [[likely]] {
                        ++it;
                        skip_ws_no_pre_check<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else if (*it == ']') {
                        ++it;
                        return;
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::expected_bracket;
                        return;
                     }
                  }

                  std::deque<typename T::value_type> intermediate{};
                  while (it < end) {
                     read<json>::op<ws_handled<Opts>()>(intermediate.emplace_back(), ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     skip_ws_no_pre_check<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     if (*it == ',') [[likely]] {
                        ++it;
                        skip_ws_no_pre_check<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else if (*it == ']') {
                        ++it;
                        break;
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::expected_bracket;
                        return;
                     }
                  }

                  value.reserve(value.size() + intermediate.size());
                  const auto inter_end = intermediate.end();
                  for (auto inter = intermediate.begin(); inter < inter_end; ++inter) {
                     value.emplace_back(std::move(*inter));
                  }
               }
               else {
                  while (it < end) {
                     read<json>::op<ws_handled<Opts>()>(value.emplace_back(), ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     skip_ws_no_pre_check<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     if (*it == ',') [[likely]] {
                        ++it;
                        skip_ws_no_pre_check<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else if (*it == ']') {
                        ++it;
                        return;
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::expected_bracket;
                        return;
                     }
                  }
               }
            }
            else {
               ctx.error = error_code::exceeded_static_array_size;
            }
         }
      };

      // counts the number of JSON array elements
      // needed for classes that are resizable, but do not have an emplace_back
      // 'it' is copied so that it does not actually progress the iterator
      // expects the opening brace ([) to have already been consumed
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE size_t number_of_array_elements(is_context auto&& ctx, auto it,
                                                                      auto&& end) noexcept
      {
         skip_ws_no_pre_check<Opts>(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return {};

         if (*it == ']') [[unlikely]] {
            return 0;
         }
         size_t count = 1;
         while (true) {
            switch (*it) {
            case ',': {
               ++count;
               ++it;
               break;
            }
            case '/': {
               skip_comment(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return {};
               break;
            }
            case '{':
               skip_until_closed<'{', '}'>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return {};
               break;
            case '[':
               skip_until_closed<'[', ']'>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return {};
               break;
            case '"': {
               skip_string<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return {};
               break;
            }
            case ']': {
               return count;
            }
            case '\0': {
               ctx.error = error_code::unexpected_end;
               return {};
            }
            default:
               ++it;
            }
         }
         unreachable();
      }

      template <class T>
         requires readable_array_t<T> && (!emplace_backable<T> && resizeable<T>)
      struct from_json<T>
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Options.ws_handled) {
               skip_ws<Options>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            constexpr auto Opts = ws_handled_off<Options>();

            match<'['>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
            const auto n = number_of_array_elements<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value.resize(n);
            size_t i = 0;
            for (auto& x : value) {
               read<json>::op<Opts>(x, ctx, it, end);
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (i < n - 1) {
                  match<','>(ctx, it);
               }
               ++i;
            }
            match<']'>(ctx, it);
         }
      };

      template <class T>
         requires glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            static constexpr auto N = []() constexpr {
               if constexpr (glaze_array_t<T>) {
                  return std::tuple_size_v<meta_t<T>>;
               }
               else {
                  return std::tuple_size_v<T>;
               }
            }();

            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            match<'['>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
            skip_ws_no_pre_check<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            for_each<N>([&](auto I) {
               if (*it == ']') {
                  return;
               }
               if constexpr (I != 0) {
                  match<','>(ctx, it);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  skip_ws_no_pre_check<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               if constexpr (is_std_tuple<T>) {
                  read<json>::op<ws_handled<Opts>()>(std::get<I>(value), ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else if constexpr (glaze_array_t<T>) {
                  read<json>::op<ws_handled<Opts>()>(get_member(value, glz::get<I>(meta_v<T>)), ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else {
                  read<json>::op<ws_handled<Opts>()>(glz::get<I>(value), ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            });

            match<']'>(ctx, it);
         }
      };

      template <glaze_flags_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            match<'['>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;

            std::string& s = string_buffer();

            static constexpr auto flag_map = make_map<T>();

            while (true) {
               read<json>::op<ws_handled_off<Opts>()>(s, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               auto itr = flag_map.find(s);
               if (itr != flag_map.end()) {
                  std::visit([&](auto&& x) { get_member(value, x) = true; }, itr->second);
               }
               else {
                  ctx.error = error_code::invalid_flag_input;
                  return;
               }

               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (*it == ']') {
                  ++it;
                  return;
               }
               match<','>(ctx, it);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
         }
      };

      template <class T>
      struct from_json<includer<T>>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            constexpr auto Opts = ws_handled_off<Options>();
            std::string& buffer = string_buffer();
            read<json>::op<Opts>(buffer, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            const auto file_path = relativize_if_not_absolute(std::filesystem::path(ctx.current_file).parent_path(),
                                                              std::filesystem::path{buffer});

            const auto string_file_path = file_path.string();
            const auto ec = file_to_buffer(buffer, string_file_path);

            if (bool(ec)) [[unlikely]] {
               ctx.error = error_code::includer_error;
               auto& error_msg = error_buffer();
               error_msg = "file failed to open: " + string_file_path;
               ctx.includer_error = error_msg;
               return;
            }

            const auto current_file = ctx.current_file;
            ctx.current_file = string_file_path;

            const auto ecode = glz::read<Opts>(value.value, buffer, ctx);
            if (bool(ctx.error)) [[unlikely]] {
               ctx.error = error_code::includer_error;
               auto& error_msg = error_buffer();
               error_msg = glz::format_error(ecode, buffer);
               ctx.includer_error = error_msg;
               return;
            }

            ctx.current_file = current_file;
         }
      };

      // TODO: count the maximum number of escapes that can be seen if error_on_unknown_keys is true
      template <glaze_object_t T>
      GLZ_ALWAYS_INLINE constexpr bool keys_may_contain_escape()
      {
         auto is_unicode = [](const auto c) { return (static_cast<uint8_t>(c) >> 7) > 0; };

         bool may_escape = false;
         constexpr auto N = std::tuple_size_v<meta_t<T>>;
         for_each<N>([&](auto I) {
            constexpr auto first = get<0>(get<I>(meta_v<T>));
            using T0 = std::decay_t<decltype(first)>;
            if constexpr (std::is_member_pointer_v<T0>) {
               constexpr auto s = get_name<first>();
               for (auto& c : s) {
                  if (c == '\\' || c == '"' || is_unicode(c)) {
                     may_escape = true;
                     return;
                  }
               }
            }
            else {
               for (auto& c : first) {
                  if (c == '\\' || c == '"' || is_unicode(c)) {
                     may_escape = true;
                     return;
                  }
               }
            }
         });

         return may_escape;
      }

      template <reflectable T>
      inline constexpr bool keys_may_contain_escape()
      {
         return false; // escapes are not valid in C++ names
      }

      template <is_variant T>
      GLZ_ALWAYS_INLINE constexpr bool keys_may_contain_escape()
      {
         bool may_escape = false;
         constexpr auto N = std::variant_size_v<T>;
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            constexpr bool is_object = glaze_object_t<V>;
            if constexpr (is_object) {
               if constexpr (keys_may_contain_escape<V>()) {
                  may_escape = true;
                  return;
               }
            }
         });
         return may_escape;
      }

      // only use this if the keys cannot contain escape characters
      template <class T, string_literal tag = "">
         requires(glaze_object_t<T> || reflectable<T>)
      GLZ_ALWAYS_INLINE constexpr auto key_stats()
      {
         key_stats_t stats{};
         if constexpr (!tag.sv().empty()) {
            constexpr auto tag_size = tag.sv().size();
            stats.max_length = tag_size;
            stats.min_length = tag_size;
         }

         constexpr auto N = reflection_count<T>;

         for_each<N>([&](auto I) {
            using Element = glaze_tuple_element<I, N, T>;
            constexpr sv key = key_name<I, T, Element::use_reflection>;

            const auto n = key.size();
            if (n < stats.min_length) {
               stats.min_length = n;
            }
            if (n > stats.max_length) {
               stats.max_length = n;
            }
         });

         if constexpr (N > 0) { // avoid overflow
            stats.length_range = stats.max_length - stats.min_length;
         }

         return stats;
      }

      template <is_variant T, string_literal tag = "">
      GLZ_ALWAYS_INLINE constexpr auto key_stats()
      {
         key_stats_t stats{};
         if constexpr (!tag.sv().empty()) {
            constexpr auto tag_size = tag.sv().size();
            stats.max_length = tag_size;
            stats.min_length = tag_size;
         }

         constexpr auto N = std::variant_size_v<T>;
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            constexpr bool is_object = glaze_object_t<V>;
            if constexpr (is_object) {
               constexpr auto substats = key_stats<V>();
               if (substats.min_length < stats.min_length) {
                  stats.min_length = substats.min_length;
               }
               if (substats.max_length > stats.max_length) {
                  stats.max_length = substats.max_length;
               }
            }
         });

         stats.length_range = stats.max_length - stats.min_length;

         return stats;
      }

      template <glz::opts Opts>
      GLZ_ALWAYS_INLINE void parse_object_opening(is_context auto& ctx, auto& it, const auto end)
      {
         if constexpr (!Opts.opening_handled) {
            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            match<'{'>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }

         skip_ws_no_pre_check<Opts>(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
      }

      template <glz::opts Opts>
      GLZ_ALWAYS_INLINE void parse_object_entry_sep(is_context auto& ctx, auto& it, const auto end)
      {
         skip_ws_no_pre_check<Opts>(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
         match<':'>(ctx, it);
         if (bool(ctx.error)) [[unlikely]]
            return;
         skip_ws_no_pre_check<Opts>(ctx, it, end);
      }

      // Key parsing for meta objects or variants of meta objects.
      // We do not check for an ending quote, we simply parse up to the quote
      // TODO: We could expand this to compiletime known strings in general like enums
      template <class T, auto Opts, string_literal tag = "">
      GLZ_ALWAYS_INLINE std::string_view parse_object_key(is_context auto&& ctx, auto&& it, auto&& end)
      {
         // skip white space and escape characters and find the string
         if constexpr (!Opts.ws_handled) {
            skip_ws<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return {};
         }
         match<'"'>(ctx, it);
         if (bool(ctx.error)) [[unlikely]]
            return {};

         constexpr auto N = reflection_count<T>;

         if constexpr (keys_may_contain_escape<T>()) {
            std::string& static_key = string_buffer();
            read<json>::op<opening_handled<Opts>()>(static_key, ctx, it, end);
            --it; // reveal the quote
            return static_key;
         }
         else if constexpr (N > 0) {
            static constexpr auto stats = key_stats<T, tag>();
            if constexpr (stats.length_range < 24) {
               if ((it + stats.max_length) < end) [[likely]] {
                  return parse_key_cx<Opts, stats>(it);
               }
            }
            auto start = it;
            skip_till_quote(ctx, it, end);
            return {start, size_t(it - start)};
         }
         else {
            auto start = it;
            skip_till_quote(ctx, it, end);
            return {start, size_t(it - start)};
         }
      }

      template <pair_t T>
      struct from_json<T>
      {
         template <opts Options, string_literal tag = "">
         GLZ_FLATTEN static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            parse_object_opening<Options>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();

            // Only used if error_on_missing_keys = true
            [[maybe_unused]] bit_array<1> fields{};

            if (*it == '}') {
               if constexpr (Opts.error_on_missing_keys) {
                  ctx.error = error_code::missing_key;
               }
               return;
            }

            if constexpr (str_t<typename T::first_type>) {
               read<json>::op<Opts>(value.first, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            else {
               std::string_view key;
               read<json>::op<Opts>(key, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               read<json>::op<Opts>(value.first, ctx, key.data(), key.data() + key.size());
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            parse_object_entry_sep<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            read<json>::op<Opts>(value.second, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            skip_ws_no_pre_check<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            match<'}'>(ctx, it);
         }
      };

      template <class T>
         requires readable_map_t<T> || glaze_object_t<T> || reflectable<T>
      struct from_json<T>
      {
         template <auto Options, string_literal tag = "">
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            parse_object_opening<Options>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();

            static constexpr auto num_members = reflection_count<T>;
            if constexpr ((glaze_object_t<T> || reflectable<T>)&&num_members == 0 && Opts.error_on_unknown_keys) {
               if (*it == '}') [[likely]] {
                  ++it;
                  return;
               }
               ctx.error = error_code::unknown_key;
               return;
            }
            else {
               // Only used if error_on_missing_keys = true
               [[maybe_unused]] bit_array<num_members> fields{};

               decltype(auto) frozen_map = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T> && num_members > 0) {
                     using V = decay_keep_volatile_t<decltype(value)>;
#if ((defined _MSC_VER) && (!defined __clang__))
                     static thread_local auto cmap = make_map<V, Opts.use_hash_comparison>();
#else
                     static thread_local constinit auto cmap = make_map<V, Opts.use_hash_comparison>();
#endif
                     // We want to run this populate outside of the while loop
                     populate_map(value, cmap); // Function required for MSVC to build
                     return cmap;
                  }
                  else if constexpr (glaze_object_t<T> && num_members > 0) {
                     static constexpr auto cmap = make_map<T, Opts.use_hash_comparison>();
                     return cmap;
                  }
                  else {
                     return nullptr;
                  }
               }();

               bool first = true;
               while (true) {
                  if (*it == '}') {
                     ++it;
                     if constexpr (Opts.error_on_missing_keys) {
                        constexpr auto req_fields = required_fields<T, Opts>();
                        if ((req_fields & fields) != req_fields) {
                           ctx.error = error_code::missing_key;
                        }
                     }
                     return;
                  }
                  else if (first) {
                     first = false;
                  }
                  else {
                     match<','>(ctx, it);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     skip_ws_no_pre_check<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }

                  if constexpr ((glaze_object_t<T> || reflectable<T>)&&num_members == 0 && Opts.error_on_unknown_keys) {
                     static_assert(false_v<T>, "This should be unreachable");
                  }
                  else if constexpr ((glaze_object_t<T> || reflectable<T>)&&num_members == 0) {
                     match<'"'>(ctx, it);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     // parsing to an empty object, but at this point the JSON presents keys

                     // Unknown key handler does not unescape keys or want unescaped keys. Unknown escaped keys are
                     // handled by the user.

                     sv key;
                     const auto start = it;
                     while (true) {
                        skip_till_escape_or_quote(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        if (*it == '"') [[likely]] {
                           key = {start, size_t(it - start)};
                           ++it;
                           break;
                        }
                        else {
                           ++it; // skip the escape
                           if (*it == '"') {
                              ++it; // skip the escaped quote
                           }
                        }
                     }

                     parse_object_entry_sep<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     read<json>::handle_unknown<Opts>(key, value, ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
                  else if constexpr (glaze_object_t<T> || reflectable<T>) {
                     static_assert(!std::same_as<std::decay_t<decltype(frozen_map)>, std::nullptr_t>);
                     std::conditional_t<Opts.error_on_unknown_keys, const sv, sv> key =
                        parse_object_key<T, ws_handled<Opts>(), tag>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     // Because parse_object_key does not necessarily return a valid JSON key, the logic for handling
                     // whitespace and the colon must run after checking if the key exists

                     if constexpr (Opts.error_on_unknown_keys) {
                        if (*it != '"') [[unlikely]] {
                           ctx.error = error_code::unknown_key;
                           return;
                        }
                        ++it;

                        if (const auto& member_it = frozen_map.find(key); member_it != frozen_map.end()) [[likely]] {
                           parse_object_entry_sep<Opts>(ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;

                           if constexpr (Opts.error_on_missing_keys) {
                              // TODO: Kludge/hack. Should work but could easily cause memory issues with small changes.
                              // At the very least if we are going to do this add a get_index method to the maps and
                              // call that
                              auto index = member_it - frozen_map.begin();
                              fields[index] = true;
                           }
                           std::visit(
                              [&](auto&& member_ptr) {
                                 read<json>::op<ws_handled<Opts>()>(get_member(value, member_ptr), ctx, it, end);
                              },
                              member_it->second);
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                        }
                        else [[unlikely]] {
                           if constexpr (tag.sv().empty()) {
                              std::advance(it, -int64_t(key.size()));
                              ctx.error = error_code::unknown_key;
                              return;
                           }
                           else if (key != tag.sv()) {
                              std::advance(it, -int64_t(key.size()));
                              ctx.error = error_code::unknown_key;
                              return;
                           }
                           else {
                              // We duplicate this code to avoid generating unreachable code
                              parse_object_entry_sep<Opts>(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;

                              read<json>::handle_unknown<Opts>(key, value, ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                           }
                        }
                     }
                     else {
                        if (const auto& member_it = frozen_map.find(key); member_it != frozen_map.end()) [[likely]] {
                           // This code should not error on valid unknown keys
                           // We arrived here because the key was perhaps found, but if the quote does not exist
                           // then this does not necessarily mean have a syntax error.
                           // We may have just found the prefix of a longer, unknown key.
                           if (*it != '"') [[unlikely]] {
                              auto* start = key.data();
                              skip_till_quote(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                              key = {start, size_t(it - start)};
                              ++it; // skip the quote

                              parse_object_entry_sep<Opts>(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;

                              read<json>::handle_unknown<Opts>(key, value, ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                           }
                           else {
                              ++it; // skip the quote

                              parse_object_entry_sep<Opts>(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;

                              if constexpr (Opts.error_on_missing_keys) {
                                 // TODO: Kludge/hack. Should work but could easily cause memory issues with small
                                 // changes. At the very least if we are going to do this add a get_index method to the
                                 // maps and call that
                                 auto index = member_it - frozen_map.begin();
                                 fields[index] = true;
                              }
                              std::visit(
                                 [&](auto&& member_ptr) {
                                    read<json>::op<ws_handled<Opts>()>(get_member(value, member_ptr), ctx, it, end);
                                 },
                                 member_it->second);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                           }
                        }
                        else [[unlikely]] {
                           if (*it != '"') {
                              // we need to search until we find the ending quote of the key
                              skip_till_quote(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                              auto start = key.data();
                              key = {start, size_t(it - start)};
                           }
                           ++it; // skip the quote

                           parse_object_entry_sep<Opts>(ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;

                           read<json>::handle_unknown<Opts>(key, value, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                        }
                     }
                  }
                  else {
                     // using k_t = std::conditional_t<heterogeneous_map<T>, sv, typename T::key_type>;
                     using k_t = typename T::key_type;
                     if constexpr (std::is_same_v<k_t, std::string>) {
                        static thread_local k_t key;
                        read<json>::op<Opts>(key, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        parse_object_entry_sep<Opts>(ctx, it, end);

                        read<json>::op<ws_handled<Opts>()>(value[key], ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else if constexpr (str_t<k_t>) {
                        k_t key;
                        read<json>::op<Opts>(key, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        parse_object_entry_sep<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        read<json>::op<ws_handled<Opts>()>(value[key], ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else {
                        k_t key_value{};
                        if constexpr (glaze_enum_t<k_t>) {
                           read<json>::op<Opts>(key_value, ctx, it, end);
                        }
                        else if constexpr (std::is_arithmetic_v<k_t>) {
                           // prefer over quoted_t below to avoid double parsing of quoted_t
                           read<json>::op<opt_true<Opts, &opts::quoted_num>>(key_value, ctx, it, end);
                        }
                        else {
                           read<json>::op<opt_false<Opts, &opts::raw_string>>(quoted_t<k_t>{key_value}, ctx, it, end);
                        }
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        parse_object_entry_sep<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        read<json>::op<Opts>(value[key_value], ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
                  skip_ws<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
            }
         }
      };

      template <class T>
         requires(glaze_object_t<T> || reflectable<T>) && partial_read<T>
      struct from_json<T>
      {
         template <auto Options, string_literal tag = "">
         GLZ_FLATTEN static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            static constexpr auto num_members = [] {
               if constexpr (reflectable<T>) {
                  return std::tuple_size_v<decltype(to_tuple(std::declval<T>()))>;
               }
               else {
                  return std::tuple_size_v<meta_t<T>>;
               }
            }();

            if constexpr (num_members == 0) {
               static_assert(false_v<T>, "No members to read for partial read");
            }

            parse_object_opening<Options>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();

            // Only used if partial_read_nested = true
            [[maybe_unused]] uint32_t opening_counter = 1;

            bit_array<num_members> fields{};
            bit_array<num_members> all_fields{};
            for_each<num_members>([&](auto I) constexpr { all_fields[I] = true; });

            decltype(auto) frozen_map = [&]() -> decltype(auto) {
               if constexpr (reflectable<T> && num_members > 0) {
#if ((defined _MSC_VER) && (!defined __clang__))
                  static thread_local auto cmap = make_map<T, Opts.use_hash_comparison>();
#else
                  static thread_local constinit auto cmap = make_map<T, Opts.use_hash_comparison>();
#endif
                  // We want to run this populate outside of the while loop
                  populate_map(value, cmap); // Function required for MSVC to build
                  return cmap;
               }
               else if constexpr (glaze_object_t<T> && num_members > 0) {
                  static constexpr auto cmap = make_map<T, Opts.use_hash_comparison>();
                  return cmap;
               }
               else {
                  return nullptr;
               }
            }();

            bool first = true;
            while (true) {
               if ((all_fields & fields) == all_fields) [[unlikely]] {
                  if constexpr (Opts.partial_read_nested) {
                     while (it != end) {
                        if (*it == '}') [[unlikely]]
                           --opening_counter;
                        if (*it == '{') [[unlikely]]
                           ++opening_counter;
                        ++it;
                        if (opening_counter == 0) [[unlikely]]
                           return;
                     }
                  }
                  else
                     return;
               }
               else if (*it == '}') [[unlikely]] {
                  if constexpr (Opts.error_on_missing_keys) {
                     ctx.error = error_code::missing_key;
                  }
                  return;
               }
               else if (first) [[unlikely]]
                  first = false;
               else [[likely]] {
                  match<','>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  skip_ws_no_pre_check<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               std::conditional_t<Opts.error_on_unknown_keys, const sv, sv> key =
                  parse_object_key<T, ws_handled<Opts>(), tag>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               // Because parse_object_key does not necessarily return a valid JSON key, the logic for handling
               // whitespace and the colon must run after checking if the key exists

               if constexpr (Opts.error_on_unknown_keys) {
                  if (*it != '"') [[unlikely]] {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
                  ++it;

                  if (const auto& member_it = frozen_map.find(key); member_it != frozen_map.end()) [[likely]] {
                     parse_object_entry_sep<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     // TODO: Kludge/hack. Should work but could easily cause memory issues with small changes.
                     // At the very least if we are going to do this add a get_index method to the maps and
                     // call that
                     auto index = member_it - frozen_map.begin();
                     fields[index] = true;

                     std::visit(
                        [&](auto&& member_ptr) {
                           read<json>::op<ws_handled<Opts>()>(get_member(value, member_ptr), ctx, it, end);
                        },
                        member_it->second);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
                  else [[unlikely]] {
                     if constexpr (tag.sv().empty()) {
                        std::advance(it, -int64_t(key.size()));
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                     else if (key != tag.sv()) {
                        std::advance(it, -int64_t(key.size()));
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                     else {
                        // We duplicate this code to avoid generating unreachable code
                        parse_object_entry_sep<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        read<json>::handle_unknown<Opts>(key, value, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
               }
               else {
                  if (const auto& member_it = frozen_map.find(key); member_it != frozen_map.end()) [[likely]] {
                     // This code should not error on valid unknown keys
                     // We arrived here because the key was perhaps found, but if the quote does not exist
                     // then this does not necessarily mean have a syntax error.
                     // We may have just found the prefix of a longer, unknown key.
                     if (*it != '"') [[unlikely]] {
                        auto* start = key.data();
                        skip_till_quote(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        key = {start, size_t(it - start)};
                     }
                     ++it; // skip the quote

                     parse_object_entry_sep<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     // TODO: Kludge/hack. Should work but could easily cause memory issues with small changes.
                     // At the very least if we are going to do this add a get_index method to the maps and
                     // call that
                     auto index = member_it - frozen_map.begin();
                     fields[index] = true;

                     std::visit(
                        [&](auto&& member_ptr) {
                           read<json>::op<ws_handled<Opts>()>(get_member(value, member_ptr), ctx, it, end);
                        },
                        member_it->second);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
                  else [[unlikely]] {
                     it -= key.size(); // rewind to skip the potentially escaped key

                     // Unknown key handler does not unescape keys or want unescaped keys. Unknown escaped keys
                     // are handled by the user.

                     const auto start = it;
                     while (true) {
                        skip_till_escape_or_quote(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        if (*it == '"') [[likely]] {
                           key = {start, size_t(it - start)};
                           ++it;
                           break;
                        }
                        else {
                           ++it; // skip the escape
                           if (*it == '"') {
                              ++it; // skip the escaped quote
                           }
                        }
                     }

                     // We duplicate this code to avoid generating unreachable code
                     parse_object_entry_sep<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;

                     read<json>::handle_unknown<Opts>(key, value, ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
               }
            }
         }
      };

      template <is_variant T>
      consteval auto variant_is_auto_deducible()
      {
         // Contains at most one each of the basic json types bool, numeric, string, object, array
         // If all objects are meta-objects then we can attempt to deduce them as well either through a type tag or
         // unique combinations of keys
         int bools{}, numbers{}, strings{}, objects{}, meta_objects{}, arrays{};
         constexpr auto N = std::variant_size_v<T>;
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            // ICE workaround
            bools += bool_t<V>;
            numbers += num_t<V>;
            strings += str_t<V>;
            strings += glaze_enum_t<V>;
            objects += pair_t<V>;
            objects += (writable_map_t<V> || readable_map_t<V>);
            objects += glaze_object_t<V>;
            meta_objects += glaze_object_t<V> || reflectable<V>;
            arrays += glaze_array_t<V>;
            arrays += array_t<V>;
            // TODO null
         });
         return bools < 2 && numbers < 2 && strings < 2 && (objects < 2 || meta_objects == objects) && arrays < 2;
      }

      template <class>
      struct variant_types;

      template <class... Ts>
      struct variant_types<std::variant<Ts...>>
      {
         // TODO: this way of filtering types is compile time intensive.
         using bool_types = decltype(tuplet::tuple_cat(
            std::conditional_t<bool_t<remove_meta_wrapper_t<Ts>>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
         using number_types = decltype(tuplet::tuple_cat(
            std::conditional_t<num_t<remove_meta_wrapper_t<Ts>>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
         using string_types = decltype(tuplet::tuple_cat( // glaze_enum_t remove_meta_wrapper_t supports constexpr types
                                                          // while the other supports non const
            std::conditional_t < str_t<remove_meta_wrapper_t<Ts>> || glaze_enum_t<remove_meta_wrapper_t<Ts>> ||
               glaze_enum_t<Ts>,
            tuplet::tuple<Ts>, tuplet::tuple < >> {}...));
         using object_types = decltype(tuplet::tuple_cat(
            std::conditional_t < reflectable<Ts> || readable_map_t<Ts> || writable_map_t<Ts> || glaze_object_t<Ts>,
            tuplet::tuple<Ts>, tuplet::tuple < >> {}...));
         using array_types =
            decltype(tuplet::tuple_cat(std::conditional_t < array_t<remove_meta_wrapper_t<Ts>> || glaze_array_t<Ts>,
                                       tuplet::tuple<Ts>, tuplet::tuple < >> {}...));
         using nullable_types =
            decltype(tuplet::tuple_cat(std::conditional_t<null_t<Ts>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
      };

      // post process output of variant_types
      template <class>
      struct tuple_types;

      template <class... Ts>
      struct tuple_types<tuplet::tuple<Ts...>>
      {
         using glaze_const_types = decltype(tuplet::tuple_cat(
            std::conditional_t<glaze_const_value_t<Ts>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
         using glaze_non_const_types = decltype(tuplet::tuple_cat(
            std::conditional_t<!glaze_const_value_t<Ts>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
      };

      template <class>
      struct variant_type_count;

      template <class... Ts>
      struct variant_type_count<std::variant<Ts...>>
      {
         using V = variant_types<std::variant<Ts...>>;
         static constexpr auto n_bool = std::tuple_size_v<typename V::bool_types>;
         static constexpr auto n_number = std::tuple_size_v<typename V::number_types>;
         static constexpr auto n_string = std::tuple_size_v<typename V::string_types>;
         static constexpr auto n_object = std::tuple_size_v<typename V::object_types>;
         static constexpr auto n_array = std::tuple_size_v<typename V::array_types>;
         static constexpr auto n_null = std::tuple_size_v<typename V::nullable_types>;
      };

      template <class Tuple>
      struct process_arithmetic_boolean_string_or_array
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (std::tuple_size_v<Tuple> < 1) {
               ctx.error = error_code::no_matching_variant_type;
            }
            else {
               using const_glaze_types = typename tuple_types<Tuple>::glaze_const_types;
               bool found_match{};
               for_each<std::tuple_size_v<const_glaze_types>>([&]([[maybe_unused]] auto I) mutable {
                  if (found_match) {
                     return;
                  }
                  using V = std::tuple_element_t<I, const_glaze_types>;
                  // run time substitute to compare to const value
                  std::remove_const_t<std::remove_pointer_t<std::remove_const_t<meta_wrapper_t<V>>>> substitute{};
                  auto copy_it{it};
                  read<json>::op<ws_handled<Options>()>(substitute, ctx, it, end);
                  static constexpr auto const_value{*meta_wrapper_v<V>};
                  if (substitute == const_value) {
                     found_match = true;
                     if (!std::holds_alternative<V>(value)) value = V{};
                  }
                  else {
                     it = copy_it;
                  }
               });
               if (found_match) {
                  return;
               }

               using non_const_types = typename tuple_types<Tuple>::glaze_non_const_types;
               if constexpr (std::tuple_size_v < non_const_types >> 0) {
                  using V = std::tuple_element_t<0, non_const_types>;
                  if (!std::holds_alternative<V>(value)) value = V{};
                  read<json>::op<ws_handled<Options>()>(std::get<V>(value), ctx, it, end);
               }
               else {
                  ctx.error = error_code::no_matching_variant_type;
               }
            }
         }
      };

      template <is_variant T>
      struct from_json<T>
      {
         // Note that items in the variant are required to be default constructable for us to switch types
         template <auto Options>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (variant_is_auto_deducible<T>()) {
               if constexpr (!Options.ws_handled) {
                  skip_ws<Options>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               constexpr auto Opts = ws_handled_off<Options>();
               switch (*it) {
               case '\0':
                  ctx.error = error_code::unexpected_end;
                  return;
               case '{':
                  ++it;
                  using object_types = typename variant_types<T>::object_types;
                  if constexpr (std::tuple_size_v<object_types> < 1) {
                     ctx.error = error_code::no_matching_variant_type;
                     return;
                  }
                  else if constexpr (std::tuple_size_v<object_types> == 1) {
                     using V = std::tuple_element_t<0, object_types>;
                     if (!std::holds_alternative<V>(value)) value = V{};
                     read<json>::op<opening_handled<Opts>()>(std::get<V>(value), ctx, it, end);
                     return;
                  }
                  else {
                     auto possible_types = bit_array<std::variant_size_v<T>>{}.flip();
                     static constexpr auto deduction_map = detail::make_variant_deduction_map<T>();
                     static constexpr auto tag_literal = string_literal_from_view<tag_v<T>.size()>(tag_v<T>);
                     skip_ws<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     auto start = it;
                     while (*it != '}') {
                        if (it != start) {
                           match<','>(ctx, it);
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                        }
                        const sv key = parse_object_key<T, Opts, tag_literal>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        match<'"'>(ctx, it);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        if constexpr (deduction_map.size()) {
                           auto deduction_it = deduction_map.find(key);
                           if (deduction_it != deduction_map.end()) [[likely]] {
                              possible_types &= deduction_it->second;
                           }
                           else [[unlikely]] {
                              if constexpr (!tag_v<T>.empty()) {
                                 if (key == tag_v<T>) {
                                    parse_object_entry_sep<Opts>(ctx, it, end);
                                    if (bool(ctx.error)) [[unlikely]]
                                       return;
                                    std::string_view type_id{};
                                    read<json>::op<ws_handled<Opts>()>(type_id, ctx, it, end);
                                    if (bool(ctx.error)) [[unlikely]]
                                       return;
                                    skip_ws_no_pre_check<Opts>(ctx, it, end);
                                    if (bool(ctx.error)) [[unlikely]]
                                       return;
                                    if (!(*it == ',' || *it == '}')) {
                                       ctx.error = error_code::syntax_error;
                                       return;
                                    }

                                    static constexpr auto id_map = make_variant_id_map<T>();
                                    auto id_it = id_map.find(type_id);
                                    if (id_it != id_map.end()) [[likely]] {
                                       it = start;
                                       const auto type_index = id_it->second;
                                       if (value.index() != type_index) value = runtime_variant_map<T>()[type_index];
                                       std::visit(
                                          [&](auto&& v) {
                                             using V = std::decay_t<decltype(v)>;
                                             constexpr bool is_object = glaze_object_t<V> || reflectable<V>;
                                             if constexpr (is_object) {
                                                from_json<V>::template op<opening_handled<Opts>(), tag_literal>(
                                                   v, ctx, it, end);
                                             }
                                          },
                                          value);
                                       return;
                                    }
                                    else {
                                       ctx.error = error_code::no_matching_variant_type;
                                       return;
                                    }
                                 }
                                 else if constexpr (Opts.error_on_unknown_keys) {
                                    ctx.error = error_code::unknown_key;
                                    return;
                                 }
                              }
                              else if constexpr (Opts.error_on_unknown_keys) {
                                 ctx.error = error_code::unknown_key;
                                 return;
                              }
                           }
                        }
                        else if constexpr (!tag_v<T>.empty()) {
                           // empty object case for variant
                           if (key == tag_v<T>) {
                              parse_object_entry_sep<Opts>(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;

                              std::string_view type_id{};
                              read<json>::op<ws_handled<Opts>()>(type_id, ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                              skip_ws_no_pre_check<Opts>(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;

                              static constexpr auto id_map = make_variant_id_map<T>();
                              auto id_it = id_map.find(type_id);
                              if (id_it != id_map.end()) [[likely]] {
                                 it = start;
                                 const auto type_index = id_it->second;
                                 if (value.index() != type_index) value = runtime_variant_map<T>()[type_index];
                                 return;
                              }
                              else {
                                 ctx.error = error_code::no_matching_variant_type;
                                 return;
                              }
                           }
                           else if constexpr (Opts.error_on_unknown_keys) {
                              ctx.error = error_code::unknown_key;
                              return;
                           }
                        }
                        else if constexpr (Opts.error_on_unknown_keys) {
                           ctx.error = error_code::unknown_key;
                           return;
                        }

                        auto matching_types = possible_types.popcount();
                        if (matching_types == 0) {
                           ctx.error = error_code::no_matching_variant_type;
                           return;
                        }
                        else if (matching_types == 1) {
                           it = start;
                           const auto type_index = possible_types.countr_zero();
                           if (value.index() != static_cast<size_t>(type_index))
                              value = runtime_variant_map<T>()[type_index];
                           std::visit(
                              [&](auto&& v) {
                                 using V = std::decay_t<decltype(v)>;
                                 constexpr bool is_object = glaze_object_t<V> || reflectable<V>;
                                 if constexpr (is_object) {
                                    from_json<V>::template op<opening_handled<Opts>(), tag_literal>(v, ctx, it, end);
                                 }
                              },
                              value);
                           return;
                        }
                        parse_object_entry_sep<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        skip_value<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        skip_ws_no_pre_check<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     ctx.error = error_code::no_matching_variant_type;
                  }
                  break;
               case '[':
                  using array_types = typename variant_types<T>::array_types;
                  process_arithmetic_boolean_string_or_array<array_types>::template op<Opts>(value, ctx, it, end);
                  break;
               case '"': {
                  using string_types = typename variant_types<T>::string_types;
                  process_arithmetic_boolean_string_or_array<string_types>::template op<Opts>(value, ctx, it, end);
                  break;
               }
               case 't':
               case 'f': {
                  using bool_types = typename variant_types<T>::bool_types;
                  process_arithmetic_boolean_string_or_array<bool_types>::template op<Opts>(value, ctx, it, end);
                  break;
               }
               case 'n':
                  using nullable_types = typename variant_types<T>::nullable_types;
                  if constexpr (std::tuple_size_v<nullable_types> < 1) {
                     ctx.error = error_code::no_matching_variant_type;
                  }
                  else {
                     using V = std::tuple_element_t<0, nullable_types>;
                     if (!std::holds_alternative<V>(value)) value = V{};
                     match<"null">(ctx, it, end);
                  }
                  break;
               default: {
                  // Not bool, string, object, or array so must be number or null
                  using number_types = typename variant_types<T>::number_types;
                  process_arithmetic_boolean_string_or_array<number_types>::template op<Opts>(value, ctx, it, end);
               }
               }
            }
            else {
               std::visit([&](auto&& v) { read<json>::op<Options>(v, ctx, it, end); }, value);
            }
         }
      };

      template <class T>
      struct from_json<array_variant_wrapper<T>>
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto&& wrapper, is_context auto&& ctx, auto&& it, auto&& end)
         {
            auto& value = wrapper.value;

            if constexpr (!Options.ws_handled) {
               skip_ws<Options>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            constexpr auto Opts = ws_handled_off<Options>();

            match<'['>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
            skip_ws_no_pre_check<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            // TODO Use key parsing for compiletime known keys
            match<'"'>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;
            auto start = it;
            skip_till_quote(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            sv type_id = {start, static_cast<size_t>(it - start)};
            match<'"'>(ctx, it);
            if (bool(ctx.error)) [[unlikely]]
               return;

            static constexpr auto id_map = make_variant_id_map<T>();
            auto id_it = id_map.find(type_id);
            if (id_it != id_map.end()) [[likely]] {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               match<','>(ctx, it);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               const auto type_index = id_it->second;
               if (value.index() != type_index) value = runtime_variant_map<T>()[type_index];
               std::visit([&](auto&& v) { read<json>::op<Opts>(v, ctx, it, end); }, value);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            else {
               ctx.error = error_code::no_matching_variant_type;
               return;
            }

            skip_ws<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            match<']'>(ctx, it);
         }
      };

      template <is_expected T>
      struct from_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Opts.ws_handled) {
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            if (*it == '{') {
               auto start = it;
               ++it;
               skip_ws<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (*it == '}') {
                  it = start;
                  // empty object
                  if (value) {
                     read<json>::op<Opts>(*value, ctx, it, end);
                  }
                  else {
                     value.emplace();
                     read<json>::op<Opts>(*value, ctx, it, end);
                  }
               }
               else {
                  // either we have an unexpected value or we are decoding an object
                  auto& key = string_buffer();
                  read<json>::op<Opts>(key, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  if (key == "unexpected") {
                     skip_ws<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     match<':'>(ctx, it);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     // read in unexpected value
                     if (!value) {
                        read<json>::op<Opts>(value.error(), ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else {
                        // set value to unexpected
                        using error_type = typename std::decay_t<decltype(value)>::error_type;
                        std::decay_t<error_type> error{};
                        read<json>::op<Opts>(error, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        value = glz::unexpected(error);
                     }
                     skip_ws<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     match<'}'>(ctx, it);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
                  else {
                     it = start;
                     if (value) {
                        read<json>::op<Opts>(*value, ctx, it, end);
                     }
                     else {
                        value.emplace();
                        read<json>::op<Opts>(*value, ctx, it, end);
                     }
                  }
               }
            }
            else {
               // this is not an object and therefore cannot be an unexpected value
               if (value) {
                  read<json>::op<Opts>(*value, ctx, it, end);
               }
               else {
                  value.emplace();
                  read<json>::op<Opts>(*value, ctx, it, end);
               }
            }
         }
      };

      template <nullable_t T>
         requires(!is_expected<T>)
      struct from_json<T>
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Options.ws_handled) {
               skip_ws<Options>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            constexpr auto Opts = ws_handled_off<Options>();

            if (*it == 'n') {
               ++it;
               match<"ull">(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if constexpr (!std::is_pointer_v<T>) {
                  value.reset();
               }
            }
            else {
               if (!value) {
                  if constexpr (is_specialization_v<T, std::optional>) {
                     if constexpr (requires { value.emplace(); }) {
                        value.emplace();
                     }
                     else {
                        value = typename T::value_type{};
                     }
                  }
                  else if constexpr (is_specialization_v<T, std::unique_ptr>)
                     value = std::make_unique<typename T::element_type>();
                  else if constexpr (is_specialization_v<T, std::shared_ptr>)
                     value = std::make_shared<typename T::element_type>();
                  else if constexpr (constructible<T>) {
                     value = meta_construct_v<T>();
                  }
                  else {
                     ctx.error = error_code::invalid_nullable_read;
                     return;
                     // Cannot read into unset nullable that is not std::optional, std::unique_ptr, or std::shared_ptr
                  }
               }
               read<json>::op<Opts>(*value, ctx, it, end);
            }
         }
      };

      template <filesystem_path T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            std::string& buffer = string_buffer();
            read<json>::op<Opts>(buffer, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value = buffer;
         }
      };
   } // namespace detail

   template <class Buffer>
   [[nodiscard]] inline parse_error validate_json(Buffer&& buffer) noexcept
   {
      context ctx{};
      glz::skip skip_value{};
      return read<opts{.force_conformance = true}>(skip_value, std::forward<Buffer>(buffer), ctx);
   }

   template <class Buffer>
   [[nodiscard]] inline parse_error validate_jsonc(Buffer&& buffer) noexcept
   {
      context ctx{};
      glz::skip skip_value{};
      return read<opts{}>(skip_value, std::forward<Buffer>(buffer), ctx);
   }

   template <class T, class Buffer>
   [[nodiscard]] inline parse_error read_json(T& value, Buffer&& buffer) noexcept
   {
      context ctx{};
      return read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
   }

   template <class T, class Buffer>
   [[nodiscard]] inline expected<T, parse_error> read_json(Buffer&& buffer) noexcept
   {
      T value{};
      context ctx{};
      const auto ec = read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec) {
         return unexpected(ec);
      }
      return value;
   }

   template <auto Opts = opts{}, class T>
   inline parse_error read_file_json(T& value, const sv file_name, auto&& buffer) noexcept
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {ec};
      }

      return read<set_json<Opts>()>(value, buffer, ctx);
   }

   template <class T>
   concept read_json_supported =
      detail::read_json_invocable<glz::opts{}, std::add_lvalue_reference<T>, glz::context, const char*, const char*>;
}
