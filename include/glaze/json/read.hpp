// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <climits>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <ranges>
#include <type_traits>

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/json/json_concepts.hpp"
#include "glaze/json/skip.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/glaze_fast_float.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/variant.hpp"

#ifdef _MSC_VER
// Turn off MSVC warning for unreachable code due to constexpr branching
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace glz
{

   // forward declare from json/wrappers.hpp to avoid circular include
   template <class T>
   struct quoted_t;

   namespace detail
   {
      // Unless we can mutate the input buffer we need somewhere to store escaped strings for key lookup, etc.
      // We don't put this in the context because we don't want to continually reallocate.
      // IMPORTANT: Do not call use this when nested calls may be made that need additional buffers on the same thread.
      inline std::string& string_buffer() noexcept
      {
         static thread_local std::string buffer(256, '\0');
         return buffer;
      }

      // We use an error buffer to avoid multiple allocations in the case that errors occur multiple times.
      inline std::string& error_buffer() noexcept
      {
         static thread_local std::string buffer(256, '\0');
         return buffer;
      }

      template <>
      struct read<JSON>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end)
         {
            if constexpr (const_value_v<T>) {
               if constexpr (Opts.error_on_const_read) {
                  ctx.error = error_code::attempt_const_read;
               }
               else {
                  // do not read anything into the const value
                  skip_value<JSON>::op<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
               }
            }
            else {
               using V = std::remove_cvref_t<T>;
               from<JSON, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                                std::forward<It1>(end));
            }
         }

         // This unknown key handler should not be given unescaped keys, that is for the user to handle.
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         static void handle_unknown(const sv& key, T&& value, Ctx&& ctx, It0&& it, It1&& end)
         {
            using ValueType = std::decay_t<decltype(value)>;
            if constexpr (detail::has_unknown_reader<ValueType>) {
               constexpr auto& reader = meta_unknown_read_v<ValueType>;
               using ReaderType = meta_unknown_read_t<ValueType>;
               if constexpr (std::is_member_object_pointer_v<ReaderType>) {
                  using MemberType = typename member_value<ReaderType>::type;
                  if constexpr (detail::map_subscriptable<MemberType>) {
                     read<JSON>::op<Opts>((value.*reader)[key], ctx, it, end);
                  }
                  else {
                     static_assert(false_v<T>, "target must have subscript operator");
                  }
               }
               else if constexpr (std::is_member_function_pointer_v<ReaderType>) {
                  using ReturnType = typename return_type<ReaderType>::type;
                  if constexpr (std::is_void_v<ReturnType>) {
                     using TupleType = typename inputs_as_tuple<ReaderType>::type;
                     if constexpr (glz::tuple_size_v<TupleType> == 2) {
                        std::decay_t<glz::tuple_element_t<1, TupleType>> input{};
                        read<JSON>::op<Opts>(input, ctx, it, end);
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
               skip_value<JSON>::op<Opts>(ctx, it, end);
            }
         }
      };

      template <class T>
         requires(glaze_value_t<T> && !custom_read<T>)
      struct from<JSON, T>
      {
         template <auto Opts, class Value, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1&& end)
         {
            using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
            from<JSON, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                             std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };

#define GLZ_PARSE_WS_COLON \
   GLZ_SKIP_WS();          \
   GLZ_MATCH_COLON();      \
   GLZ_SKIP_WS();

      template <opts Opts, class T, size_t I, class Value, class... SelectedIndex>
         requires(glaze_object_t<T> || reflectable<T>)
      void decode_index(Value&& value, is_context auto&& ctx, auto&& it, auto&& end, SelectedIndex&&... selected_index)
      {
         static constexpr auto Key = get<I>(reflect<T>::keys);
         static constexpr auto KeyWithEndQuote = join_v<Key, chars<"\"">>;
         static constexpr auto Length = KeyWithEndQuote.size();

         if (((it + Length) < end) && comparitor<KeyWithEndQuote>(it)) [[likely]] {
            it += Length;
            GLZ_INVALID_END();

            GLZ_SKIP_WS();
            GLZ_MATCH_COLON();
            GLZ_SKIP_WS();

            using V = refl_t<T, I>;

            if constexpr (const_value_v<V>) {
               if constexpr (Opts.error_on_const_read) {
                  ctx.error = error_code::attempt_const_read;
               }
               else {
                  // do not read anything into the const value
                  skip_value<JSON>::op<Opts>(ctx, it, end);
               }
            }
            else {
               if constexpr (glaze_object_t<T>) {
                  from<JSON, std::remove_cvref_t<V>>::template op<ws_handled<Opts>()>(
                     get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
               }
               else {
                  from<JSON, std::remove_cvref_t<V>>::template op<ws_handled<Opts>()>(
                     get_member(value, get<I>(to_tuple(value))), ctx, it, end);
               }
            }

            if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
               ((selected_index = I), ...);
            }
         }
         else [[unlikely]] {
            if constexpr (Opts.error_on_unknown_keys) {
               ctx.error = error_code::unknown_key;
            }
            else {
               auto* start = it;
               skip_string_view<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               const sv key = {start, size_t(it - start)};
               ++it;
               GLZ_INVALID_END();

               GLZ_PARSE_WS_COLON;

               read<JSON>::handle_unknown<Opts>(key, value, ctx, it, end);
            }
         }
      }

      template <opts Opts, class T, size_t I, class Value>
         requires(glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<T>))
      void decode_index(Value&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         static constexpr auto TargetKey = glz::get<I>(reflect<T>::keys);
         static constexpr auto Length = TargetKey.size();

         if (((it + Length) < end) && comparitor<TargetKey>(it)) [[likely]] {
            it += Length;
            if (*it != '"') [[unlikely]] {
               ctx.error = error_code::unexpected_enum;
               return;
            }
            value = get<I>(reflect<T>::values);

            ++it;
            GLZ_VALID_END();
         }
         else [[unlikely]] {
            ctx.error = error_code::unexpected_enum;
            return;
         }
      }

      template <opts Opts, class T, auto& HashInfo, class Value, class... SelectedIndex>
         requires(glaze_object_t<T> || reflectable<T>)
      GLZ_ALWAYS_INLINE constexpr void parse_and_invoke(Value&& value, is_context auto&& ctx, auto&& it, auto&& end,
                                                        SelectedIndex&&... selected_index)
      {
         constexpr auto type = HashInfo.type;
         constexpr auto N = reflect<T>::size;

         if constexpr (not bool(type)) {
            static_assert(false_v<T>, "invalid hash algorithm");
         }

         if constexpr (N == 1) {
            decode_index<Opts, T, 0>(value, ctx, it, end, selected_index...);
         }
         else {
            const auto index = decode_hash<JSON, T, HashInfo, HashInfo.type>::op(it, end);

            if (index >= N) [[unlikely]] {
               if constexpr (Opts.error_on_unknown_keys) {
                  ctx.error = error_code::unknown_key;
                  return;
               }
               else {
                  // we need to search until we find the ending quote of the key
                  auto start = it;
                  skip_string_view<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  const sv key = {start, size_t(it - start)};
                  ++it; // skip the quote
                  GLZ_INVALID_END();

                  GLZ_PARSE_WS_COLON;

                  read<JSON>::handle_unknown<Opts>(key, value, ctx, it, end);
                  return;
               }
            }

            // We see better performance with function pointers than a glz::jump_table here.
            visit<N>([&]<size_t I>() { decode_index<Opts, T, I>(value, ctx, it, end, selected_index...); }, index);
         }
      }

      template <is_member_function_pointer T>
      struct from<JSON, T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
         {
            ctx.error = error_code::attempt_member_func_read;
         }
      };

      template <is_includer T>
      struct from<JSON, T>
      {
         template <auto Opts, class... Args>
         static void op(auto&&, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }

            match<R"("")", Opts>(ctx, it, end);
         }
      };

      template <is_bitset T>
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_MATCH_QUOTE;
            GLZ_INVALID_END();

            const auto n = value.size();
            for (size_t i = 1; it < end; ++i, ++it) {
               if (*it == '"') {
                  ++it;
                  GLZ_VALID_END();
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
      struct from<JSON, skip>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
         {
            skip_value<JSON>::op<Opts>(ctx, args...);
         }
      };

      template <is_reference_wrapper T>
      struct from<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args)
         {
            using V = std::decay_t<decltype(value.get())>;
            from<JSON, V>::template op<Opts>(value.get(), std::forward<Args>(args)...);
         }
      };

      template <>
      struct from<JSON, hidden>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
         {
            ctx.error = error_code::attempt_read_hidden;
         }
      };

      template <complex_t T>
      struct from<JSON, T>
      {
         template <auto Options>
         static void op(auto&& v, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            constexpr auto Opts = ws_handled_off<Options>();
            if constexpr (!has_ws_handled(Options)) {
               GLZ_SKIP_WS();
            }
            GLZ_MATCH_OPEN_BRACKET;
            GLZ_ADD_LEVEL;

            auto* ptr = reinterpret_cast<typename T::value_type*>(&v);
            static_assert(sizeof(T) == sizeof(typename T::value_type) * 2);
            read<JSON>::op<Opts>(ptr[0], ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            GLZ_SKIP_WS();

            GLZ_MATCH_COMMA;

            read<JSON>::op<Opts>(ptr[1], ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            GLZ_SKIP_WS();
            GLZ_MATCH_CLOSE_BRACKET;
            GLZ_SUB_LEVEL;
         }
      };

      template <always_null_t T>
      struct from<JSON, T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }
            static constexpr sv null_string = "null";
            if constexpr (not has_is_padded(Opts)) {
               const auto n = size_t(end - it);
               if ((n < 4) || not comparitor<null_string>(it)) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
               }
            }
            else {
               if (not comparitor<null_string>(it)) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
               }
            }
            it += 4; // always advance for performance
         }
      };

      template <bool_t T>
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(bool_t auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (Opts.quoted_num) {
               GLZ_SKIP_WS();
               GLZ_MATCH_QUOTE;
               GLZ_INVALID_END();
            }

            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }

            if constexpr (Opts.bools_as_numbers) {
               if (*it == '1') {
                  value = true;
                  ++it;
               }
               else if (*it == '0') {
                  value = false;
                  ++it;
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            else {
               if constexpr (not has_is_padded(Opts)) {
                  if (size_t(end - it) < 4) [[unlikely]] {
                     ctx.error = error_code::expected_true_or_false;
                     return;
                  }
               }

               uint64_t c{};
               static constexpr uint64_t u_true =
                  0b00000000'00000000'00000000'00000000'01100101'01110101'01110010'01110100;
               static constexpr uint64_t u_false =
                  0b00000000'00000000'00000000'01100101'01110011'01101100'01100001'01100110;
               if constexpr (Opts.null_terminated) {
                  // Note that because our buffer must be null terminated, we can read one more index without checking:
                  std::memcpy(&c, it, 5);
                  // We have to wipe the 5th character for true testing
                  if ((c & 0xFF'FF'FF'00'FF'FF'FF'FF) == u_true) {
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
               }
               else {
                  std::memcpy(&c, it, 4);
                  if (c == u_true) {
                     value = true;
                     it += 4;
                  }
                  else if (size_t(end - it) < 5) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  else {
                     std::memcpy(&c, it, 5);
                     if (c == u_false) [[likely]] {
                        value = false;
                        it += 5;
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::expected_true_or_false;
                        return;
                     }
                  }
               }
            }

            if constexpr (Opts.quoted_num) {
               GLZ_INVALID_END();
               GLZ_MATCH_QUOTE;
               GLZ_VALID_END();
            }
            else {
               GLZ_VALID_END();
            }
         }
      };

      template <num_t T>
      struct from<JSON, T>
      {
         template <auto Opts, class It>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
         {
            if constexpr (Opts.quoted_num) {
               GLZ_SKIP_WS();
               GLZ_MATCH_QUOTE;
               GLZ_INVALID_END();
            }

            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }

            using V = std::decay_t<decltype(value)>;
            if constexpr (int_t<V>) {
               static_assert(sizeof(*it) == sizeof(char));

               if constexpr (Opts.null_terminated) {
                  if (not glz::detail::atoi(value, it)) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
               }
               else {
                  if (not glz::detail::atoi(value, it, end)) [[unlikely]] {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
               }
            }
            else {
               if constexpr (is_float128<V>) {
                  auto [ptr, ec] = std::from_chars(it, end, value);
                  if (ec != std::errc()) {
                     ctx.error = error_code::parse_number_failure;
                     return;
                  }
                  it = ptr;
               }
               else {
                  if constexpr (std::is_volatile_v<std::remove_reference_t<decltype(value)>>) {
                     // Hardware may interact with value changes, so we parse into a temporary and assign in one
                     // place
                     V temp;
                     auto [ptr, ec] = glz::from_chars<Opts.null_terminated>(it, end, temp);
                     if (ec != std::errc()) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                     value = temp;
                     it = ptr;
                  }
                  else {
                     auto [ptr, ec] = glz::from_chars<Opts.null_terminated>(it, end, value);
                     if (ec != std::errc()) [[unlikely]] {
                        ctx.error = error_code::parse_number_failure;
                        return;
                     }
                     it = ptr;
                  }
               }
            }

            if constexpr (Opts.quoted_num) {
               GLZ_INVALID_END();
               GLZ_MATCH_QUOTE;
               GLZ_VALID_END();
            }
            else {
               GLZ_VALID_END();
            }
         }
      };

      template <string_t T>
      struct from<JSON, T>
      {
         template <auto Opts, class It, class End>
            requires(has_is_padded(Opts))
         static void op(auto& value, is_context auto&& ctx, It&& it, End&& end)
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
               if constexpr (!has_opening_handled(Opts)) {
                  if constexpr (!has_ws_handled(Opts)) {
                     GLZ_SKIP_WS();
                  }

                  GLZ_MATCH_QUOTE;
                  GLZ_INVALID_END();
               }

               if constexpr (not Opts.raw_string) {
                  static constexpr auto string_padding_bytes = 8;

                  auto start = it;
                  while (true) {
                     if (it >= end) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }

                     uint64_t chunk;
                     std::memcpy(&chunk, it, 8);
                     const uint64_t test_chars = has_quote(chunk);
                     if (test_chars) {
                        it += (countr_zero(test_chars) >> 3);

                        auto* prev = it - 1;
                        while (*prev == '\\') {
                           --prev;
                        }
                        if (size_t(it - prev) % 2) {
                           break;
                        }
                        ++it; // skip the escaped quote
                     }
                     else {
                        it += 8;
                     }
                  }

                  auto n = size_t(it - start);
                  value.resize(n + string_padding_bytes);

                  auto* p = value.data();

                  while (true) {
                     if (start >= it) {
                        break;
                     }

                     std::memcpy(p, start, 8);
                     uint64_t swar;
                     std::memcpy(&swar, p, 8);

                     constexpr uint64_t lo7_mask = repeat_byte8(0b01111111);
                     const uint64_t lo7 = swar & lo7_mask;
                     const uint64_t backslash = (lo7 ^ repeat_byte8('\\')) + lo7_mask;
                     const uint64_t less_32 = (swar & repeat_byte8(0b01100000)) + lo7_mask;
                     uint64_t next = ~((backslash & less_32) | swar);

                     next &= repeat_byte8(0b10000000);
                     if (next == 0) {
                        start += 8;
                        p += 8;
                        continue;
                     }

                     next = countr_zero(next) >> 3;
                     start += next;
                     if (start >= it) {
                        break;
                     }

                     if ((*start & 0b11100000) == 0) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     ++start; // skip the escape
                     if (*start == 'u') {
                        ++start;
                        p += next;
                        const auto mark = start;
                        const auto offset = handle_unicode_code_point(start, p, end);
                        if (offset == 0) [[unlikely]] {
                           ctx.error = error_code::unicode_escape_conversion_failure;
                           return;
                        }
                        n += offset;
                        // escape + u + unicode code points
                        n -= 2 + uint32_t(start - mark);
                     }
                     else {
                        p += next;
                        *p = char_unescape_table[uint8_t(*start)];
                        if (*p == 0) [[unlikely]] {
                           ctx.error = error_code::invalid_escape;
                           return;
                        }
                        ++p;
                        ++start;
                        --n;
                     }
                  }

                  value.resize(n);
                  ++it;
               }
               else {
                  // raw_string
                  auto start = it;
                  skip_string_view<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  value.assign(start, size_t(it - start));
                  ++it;
               }
            }
         }

         template <auto Opts, class It, class End>
            requires(not has_is_padded(Opts))
         static void op(auto& value, is_context auto&& ctx, It&& it, End&& end)
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
               if constexpr (!has_opening_handled(Opts)) {
                  if constexpr (!has_ws_handled(Opts)) {
                     GLZ_SKIP_WS();
                  }

                  GLZ_MATCH_QUOTE;
                  GLZ_INVALID_END();
               }

               if constexpr (not Opts.raw_string) {
                  static constexpr auto string_padding_bytes = 8;

                  if (size_t(end - it) >= 8) {
                     auto start = it;
                     const auto end8 = end - 8;
                     while (true) {
                        if (it >= end8) [[unlikely]] {
                           break;
                        }

                        uint64_t chunk;
                        std::memcpy(&chunk, it, 8);
                        const uint64_t test_chars = has_quote(chunk);
                        if (test_chars) {
                           it += (countr_zero(test_chars) >> 3);

                           auto* prev = it - 1;
                           while (*prev == '\\') {
                              --prev;
                           }
                           if (size_t(it - prev) % 2) {
                              goto continue_decode;
                           }
                           ++it; // skip the escaped quote
                        }
                        else {
                           it += 8;
                        }
                     }

                     while (it[-1] == '\\') [[unlikely]] {
                        // if we ended on an escape character then we need to rewind
                        // because we lost our context
                        --it;
                     }

                     for (; it < end; ++it) {
                        if (*it == '"') {
                           auto* prev = it - 1;
                           while (*prev == '\\') {
                              --prev;
                           }
                           if (size_t(it - prev) % 2) {
                              goto continue_decode;
                           }
                        }
                     }

                     ctx.error = error_code::unexpected_end;
                     return;

                  continue_decode:

                     const auto available_padding = size_t(end - it);
                     auto n = size_t(it - start);
                     if (available_padding >= 8) [[likely]] {
                        value.resize(n + string_padding_bytes);

                        auto* p = value.data();

                        while (true) {
                           if (start >= it) {
                              break;
                           }

                           std::memcpy(p, start, 8);
                           uint64_t swar;
                           std::memcpy(&swar, p, 8);

                           constexpr uint64_t lo7_mask = repeat_byte8(0b01111111);
                           const uint64_t lo7 = swar & lo7_mask;
                           const uint64_t backslash = (lo7 ^ repeat_byte8('\\')) + lo7_mask;
                           const uint64_t less_32 = (swar & repeat_byte8(0b01100000)) + lo7_mask;
                           uint64_t next = ~((backslash & less_32) | swar);

                           next &= repeat_byte8(0b10000000);
                           if (next == 0) {
                              start += 8;
                              p += 8;
                              continue;
                           }

                           next = countr_zero(next) >> 3;
                           start += next;
                           if (start >= it) {
                              break;
                           }

                           if ((*start & 0b11100000) == 0) [[unlikely]] {
                              ctx.error = error_code::syntax_error;
                              return;
                           }
                           ++start; // skip the escape
                           if (*start == 'u') {
                              ++start;
                              p += next;
                              const auto mark = start;
                              const auto offset = handle_unicode_code_point(start, p, end);
                              if (offset == 0) [[unlikely]] {
                                 ctx.error = error_code::unicode_escape_conversion_failure;
                                 return;
                              }
                              n += offset;
                              // escape + u + unicode code points
                              n -= 2 + uint32_t(start - mark);
                           }
                           else {
                              p += next;
                              *p = char_unescape_table[uint8_t(*start)];
                              if (*p == 0) [[unlikely]] {
                                 ctx.error = error_code::invalid_escape;
                                 return;
                              }
                              ++p;
                              ++start;
                              --n;
                           }
                        }

                        value.resize(n);
                        ++it;
                     }
                     else {
                        // For large inputs this case of running out of buffer is very rare
                        value.resize(n);
                        auto* p = value.data();

                        it = start;
                        while (it < end) [[likely]] {
                           *p = *it;
                           if (*it == '"') {
                              value.resize(size_t(p - value.data()));
                              ++it;
                              return;
                           }
                           else if (*it == '\\') {
                              ++it; // skip the escape
                              if (*it == 'u') {
                                 ++it;
                                 if (!handle_unicode_code_point(it, p, end)) [[unlikely]] {
                                    ctx.error = error_code::unicode_escape_conversion_failure;
                                    return;
                                 }
                              }
                              else {
                                 *p = char_unescape_table[uint8_t(*it)];
                                 if (*p == 0) [[unlikely]] {
                                    ctx.error = error_code::invalid_escape;
                                    return;
                                 }
                                 ++p;
                                 ++it;
                              }
                           }
                           else {
                              ++it;
                              ++p;
                           }
                        }

                        ctx.error = error_code::unexpected_end;
                     }
                  }
                  else {
                     // For short strings

                     std::array<char, 8> buffer{};

                     auto* p = buffer.data();

                     while (it < end) [[likely]] {
                        *p = *it;
                        if (*it == '"') {
                           value.assign(buffer.data(), size_t(p - buffer.data()));
                           ++it;
                           GLZ_VALID_END();
                           return;
                        }
                        else if (*it == '\\') {
                           ++it; // skip the escape
                           GLZ_INVALID_END();
                           if (*it == 'u') {
                              ++it;
                              GLZ_INVALID_END();
                              if (!handle_unicode_code_point(it, p, end)) [[unlikely]] {
                                 ctx.error = error_code::unicode_escape_conversion_failure;
                                 return;
                              }
                           }
                           else {
                              *p = char_unescape_table[uint8_t(*it)];
                              if (*p == 0) [[unlikely]] {
                                 ctx.error = error_code::invalid_escape;
                                 return;
                              }
                              ++p;
                              ++it;
                           }
                        }
                        else {
                           ++it;
                           ++p;
                        }
                     }

                     ctx.error = error_code::unexpected_end;
                  }
               }
               else {
                  // raw_string
                  auto start = it;
                  skip_string_view<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  value.assign(start, size_t(it - start));
                  ++it;
               }
            }
         }
      };

      template <class T>
         requires(string_view_t<T> || char_array_t<T> || array_char_t<T>)
      struct from<JSON, T>
      {
         template <auto Opts, class It, class End>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, It&& it, End&& end) noexcept
         {
            if constexpr (!has_opening_handled(Opts)) {
               if constexpr (!has_ws_handled(Opts)) {
                  GLZ_SKIP_WS();
               }

               GLZ_MATCH_QUOTE;
               GLZ_INVALID_END();
            }

            auto start = it;
            skip_string_view<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            if constexpr (string_view_t<T>) {
               value = {start, size_t(it - start)};
            }
            else if constexpr (char_array_t<T>) {
               const size_t n = it - start;
               if ((n + 1) > sizeof(value)) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               std::memcpy(value, start, n);
               value[n] = '\0';
            }
            else if constexpr (array_char_t<T>) {
               const size_t n = it - start;
               if ((n + 1) > value.size()) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               std::memcpy(value.data(), start, n);
               value[n] = '\0';
            }
            ++it; // skip closing quote
            GLZ_VALID_END();
         }
      };

      template <char_t T>
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!has_opening_handled(Opts)) {
               if constexpr (!has_ws_handled(Opts)) {
                  GLZ_SKIP_WS();
               }

               GLZ_MATCH_QUOTE;
               GLZ_INVALID_END();
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
                  ctx.error = error_code::unicode_escape_conversion_failure;
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
            GLZ_INVALID_END();
            GLZ_MATCH_QUOTE;
            GLZ_VALID_END();
         }
      };

      template <class T>
         requires((glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<T>)) && !custom_read<T>)
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }

            constexpr auto N = reflect<T>::size;

            if (*it != '"') [[unlikely]] {
               ctx.error = error_code::expected_quote;
               return;
            }
            ++it;
            GLZ_INVALID_END();

            if constexpr (N == 1) {
               decode_index<Opts, T, 0>(value, ctx, it, end);
            }
            else {
               static constexpr auto HashInfo = hash_info<T>;

               const auto index = decode_hash<JSON, T, HashInfo, HashInfo.type>::op(it, end);

               if (index >= N) [[unlikely]] {
                  ctx.error = error_code::unexpected_enum;
                  return;
               }

               jump_table<N>([&]<size_t I>() { decode_index<Opts, T, I>(value, ctx, it, end); }, index);
            }
         }
      };

      template <class T>
         requires(std::is_enum_v<T> && !glaze_enum_t<T> && !meta_keys<T> && !custom_read<T>)
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            // TODO: use std::bit_cast???
            std::underlying_type_t<std::decay_t<T>> x{};
            read<JSON>::op<Opts>(x, ctx, it, end);
            value = static_cast<std::decay_t<T>>(x);
         }
      };

      template <func_t T>
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto& /*value*/, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }
            GLZ_MATCH_QUOTE;
            GLZ_INVALID_END();
            skip_string_view<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            GLZ_MATCH_QUOTE;
            GLZ_VALID_END();
         }
      };

      template <class T>
      struct from<JSON, basic_raw_json<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            auto it_start = it;
            if (*it == 'n') {
               match<"null", Opts>(ctx, it, end);
            }
            else if (is_digit(uint8_t(*it))) {
               skip_number<Opts>(ctx, it, end);
            }
            else {
               skip_value<JSON>::op<Opts>(ctx, it, end);
            }
            if (bool(ctx.error)) [[unlikely]]
               return;
            value.str = {it_start, static_cast<size_t>(it - it_start)};
         }
      };

      template <class T>
      struct from<JSON, basic_text<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& it, auto&& end)
         {
            value.str = {it, static_cast<size_t>(end - it)}; // read entire contents as string
            it = end;
         }
      };

      // for set types
      template <class T>
         requires(readable_array_t<T> && !emplace_backable<T> && !resizable<T> && emplaceable<T>)
      struct from<JSON, T>
      {
         template <auto Options>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            constexpr auto Opts = ws_handled_off<Options>();
            if constexpr (!has_ws_handled(Options)) {
               GLZ_SKIP_WS();
            }

            GLZ_MATCH_OPEN_BRACKET;
            GLZ_ADD_LEVEL;
            GLZ_SKIP_WS();

            value.clear();
            if (*it == ']') [[unlikely]] {
               GLZ_SUB_LEVEL;
               ++it;
               return;
            }

            while (true) {
               using V = range_value_t<T>;
               V v;
               read<JSON>::op<Opts>(v, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               value.emplace(std::move(v));
               GLZ_SKIP_WS();
               if (*it == ']') {
                  GLZ_SUB_LEVEL;
                  ++it;
                  return;
               }
               GLZ_MATCH_COMMA;
               GLZ_SKIP_WS();
            }
         }
      };

      // for types like std::vector, std::array, std::deque, etc.
      template <class T>
         requires(readable_array_t<T> && (emplace_backable<T> || !resizable<T>) && !emplaceable<T>)
      struct from<JSON, T>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            constexpr auto Opts = ws_handled_off<Options>();
            if constexpr (!has_ws_handled(Options)) {
               GLZ_SKIP_WS();
            }

            GLZ_MATCH_OPEN_BRACKET;
            GLZ_ADD_LEVEL;

            const auto ws_start = it;
            GLZ_SKIP_WS();

            if (*it == ']') {
               GLZ_SUB_LEVEL;
               ++it;
               if constexpr (resizable<T>) {
                  value.clear();

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }
               return;
            }

            const size_t ws_size = size_t(it - ws_start);

            const auto n = value.size();

            auto value_it = value.begin();

            for (size_t i = 0; i < n; ++i) {
               read<JSON>::op<ws_handled<Opts>()>(*value_it++, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               GLZ_SKIP_WS();
               if (*it == ',') {
                  ++it;

                  if constexpr (!Opts.minified) {
                     if (ws_size && ws_size < size_t(end - it)) {
                        skip_matching_ws(ws_start, it, ws_size);
                     }
                  }

                  GLZ_SKIP_WS();
               }
               else if (*it == ']') {
                  GLZ_SUB_LEVEL;
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

            if constexpr (Opts.partial_read) {
               return;
            }
            else {
               // growing
               if constexpr (emplace_backable<T>) {
                  while (it < end) {
                     read<JSON>::op<ws_handled<Opts>()>(value.emplace_back(), ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     GLZ_SKIP_WS();
                     if (*it == ',') [[likely]] {
                        ++it;

                        if constexpr (!Opts.minified) {
                           if (ws_size && ws_size < size_t(end - it)) {
                              skip_matching_ws(ws_start, it, ws_size);
                           }
                        }

                        GLZ_SKIP_WS();
                     }
                     else if (*it == ']') {
                        GLZ_SUB_LEVEL;
                        ++it;
                        return;
                     }
                     else [[unlikely]] {
                        ctx.error = error_code::expected_bracket;
                        return;
                     }
                  }
               }
               else {
                  ctx.error = error_code::exceeded_static_array_size;
               }
            }
         }

         // for types like std::vector<std::pair...> that can't look up with operator[]
         // Intead of hashing or linear searching, we just clear the input and overwrite the entire contents
         template <auto Options>
            requires(pair_t<range_value_t<T>> && Options.concatenate == true)
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();
            if constexpr (!has_opening_handled(Options)) {
               if constexpr (!has_ws_handled(Options)) {
                  GLZ_SKIP_WS();
               }
               GLZ_MATCH_OPEN_BRACE;
               GLZ_INVALID_END();
               GLZ_ADD_LEVEL;
            }

            // clear all contents and repopulate
            value.clear();

            while (it < end) {
               GLZ_SKIP_WS();

               if (*it == '}') {
                  ++it;
                  GLZ_SUB_LEVEL;
                  GLZ_VALID_END();
                  return;
               }

               auto& item = value.emplace_back();

               using V = std::decay_t<decltype(item)>;

               if constexpr (str_t<typename V::first_type>) {
                  read<JSON>::op<Opts>(item.first, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else {
                  std::string_view key;
                  read<JSON>::op<Opts>(key, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  if constexpr (Opts.null_terminated) {
                     read<JSON>::op<Opts>(item.first, ctx, key.data(), key.data() + key.size());
                  }
                  else {
                     if (size_t(end - it) == key.size()) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                     // For the non-null terminated case we just want one more character so that we don't parse
                     // until the end of the buffer and create an end_reached code (unless there is an error).
                     read<JSON>::op<Opts>(item.first, ctx, key.data(), key.data() + key.size() + 1);
                  }
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }

               GLZ_PARSE_WS_COLON;

               read<JSON>::op<Opts>(item.second, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               GLZ_SKIP_WS();

               if (*it == ',') {
                  ++it;
                  GLZ_INVALID_END();
               }
            }

            ctx.error = error_code::unexpected_end;
         }
      };

      // counts the number of JSON array elements
      // needed for classes that are resizable, but do not have an emplace_back
      // 'it' is copied so that it does not actually progress the iterator
      // expects the opening brace ([) to have already been consumed
      template <auto Opts>
      [[nodiscard]] size_t number_of_array_elements(is_context auto&& ctx, auto it, auto&& end) noexcept
      {
         skip_ws<Opts>(ctx, it, end);
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
               ++it;
               skip_until_closed<Opts, '{', '}'>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return {};
               break;
            case '[':
               ++it;
               skip_until_closed<Opts, '[', ']'>(ctx, it, end);
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

      // For types like std::forward_list
      template <class T>
         requires readable_array_t<T> && (!emplace_backable<T> && resizable<T>)
      struct from<JSON, T>
      {
         template <auto Options>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            constexpr auto Opts = ws_handled_off<Options>();
            if constexpr (!has_ws_handled(Options)) {
               GLZ_SKIP_WS();
            }

            GLZ_MATCH_OPEN_BRACKET;
            GLZ_ADD_LEVEL;
            const auto n = number_of_array_elements<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            value.resize(n);
            size_t i = 0;
            for (auto& x : value) {
               read<JSON>::op<Opts>(x, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               GLZ_SKIP_WS();
               if (i < n - 1) {
                  GLZ_MATCH_COMMA;
               }
               ++i;
            }
            match<']'>(ctx, it);
            GLZ_SUB_LEVEL;
         }
      };

      template <class T>
         requires glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            static constexpr auto N = []() constexpr {
               if constexpr (glaze_array_t<T>) {
                  return reflect<T>::size;
               }
               else {
                  return glz::tuple_size_v<T>;
               }
            }();

            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }

            GLZ_MATCH_OPEN_BRACKET;
            GLZ_ADD_LEVEL;
            GLZ_SKIP_WS();

            invoke_table<N>([&]<size_t I>() {
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (*it == ']') {
                  GLZ_SUB_LEVEL;
                  return;
               }
               if constexpr (I != 0) {
                  GLZ_MATCH_COMMA;
                  GLZ_SKIP_WS();
               }
               if constexpr (is_std_tuple<T>) {
                  read<JSON>::op<ws_handled<Opts>()>(std::get<I>(value), ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else if constexpr (glaze_array_t<T>) {
                  read<JSON>::op<ws_handled<Opts>()>(get_member(value, glz::get<I>(meta_v<T>)), ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               else {
                  read<JSON>::op<ws_handled<Opts>()>(glz::get<I>(value), ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
               GLZ_SKIP_WS();
            });

            if constexpr (Opts.partial_read) {
               return;
            }
            else {
               if (bool(ctx.error)) [[unlikely]]
                  return;
               match<']'>(ctx, it);
               GLZ_SUB_LEVEL;
               GLZ_VALID_END();
            }
         }
      };

      template <glaze_flags_t T>
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }

            GLZ_MATCH_OPEN_BRACKET;
            GLZ_ADD_LEVEL;

            std::string& s = string_buffer();

            static constexpr auto flag_map = make_map<T>();

            while (true) {
               read<JSON>::op<ws_handled_off<Opts>()>(s, ctx, it, end);
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

               GLZ_SKIP_WS();
               if (*it == ']') {
                  GLZ_SUB_LEVEL;
                  ++it;
                  GLZ_VALID_END();
                  return;
               }
               GLZ_MATCH_COMMA;
            }
         }
      };

      template <class T>
      struct from<JSON, includer<T>>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            constexpr auto Opts = ws_handled_off<Options>();
            std::string buffer{};
            read<JSON>::op<Opts>(buffer, ctx, it, end);
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

            // We need to allocate a new buffer here because we could call another includer that uses the buffer
            std::string nested_buffer = buffer;
            static constexpr auto NestedOpts = opt_true<disable_padding_on<Opts>(), &opts::null_terminated>;
            const auto ecode = glz::read<NestedOpts>(value.value, nested_buffer, ctx);
            if (bool(ctx.error)) [[unlikely]] {
               ctx.error = error_code::includer_error;
               auto& error_msg = error_buffer();
               error_msg = glz::format_error(ecode, nested_buffer);
               ctx.includer_error = error_msg;
               return;
            }

            ctx.current_file = current_file;
         }
      };

      template <pair_t T>
      struct from<JSON, T>
      {
         template <opts Options, string_literal tag = "">
         static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();
            if constexpr (!has_opening_handled(Options)) {
               if constexpr (!has_ws_handled(Options)) {
                  GLZ_SKIP_WS();
               }
               GLZ_MATCH_OPEN_BRACE;
               GLZ_ADD_LEVEL;
            }
            GLZ_SKIP_WS();

            // Only used if error_on_missing_keys = true
            [[maybe_unused]] bit_array<1> fields{};

            if (*it == '}') {
               GLZ_SUB_LEVEL;
               if constexpr (Opts.error_on_missing_keys) {
                  ctx.error = error_code::missing_key;
               }
               return;
            }

            if constexpr (str_t<typename T::first_type>) {
               read<JSON>::op<Opts>(value.first, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            else {
               std::string_view key;
               read<JSON>::op<Opts>(key, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if constexpr (Opts.null_terminated) {
                  read<JSON>::op<Opts>(value.first, ctx, key.data(), key.data() + key.size());
               }
               else {
                  if (size_t(end - it) == key.size()) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  // For the non-null terminated case we just want one more character so that we don't parse
                  // until the end of the buffer and create an end_reached code (unless there is an error).
                  read<JSON>::op<Opts>(value.first, ctx, key.data(), key.data() + key.size() + 1);
               }
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }

            GLZ_PARSE_WS_COLON;

            read<JSON>::op<Opts>(value.second, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;

            GLZ_SKIP_WS();

            match<'}'>(ctx, it);
            GLZ_SUB_LEVEL;
            GLZ_VALID_END();
         }
      };

      template <class T, string_literal Tag>
      inline consteval bool contains_tag()
      {
         auto& keys = reflect<T>::keys;
         for (size_t i = 0; i < keys.size(); ++i) {
            if (Tag.sv() == keys[i]) {
               return true;
            }
         }
         return false;
      }

      template <class T>
         requires readable_map_t<T> || glaze_object_t<T> || reflectable<T>
      struct from<JSON, T>
      {
         template <auto Options, string_literal tag = "">
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            static constexpr auto num_members = reflect<T>::size;

            static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();
            if constexpr (!has_opening_handled(Options)) {
               if constexpr (!has_ws_handled(Options)) {
                  GLZ_SKIP_WS();
               }
               GLZ_MATCH_OPEN_BRACE;
               GLZ_INVALID_END();
               GLZ_ADD_LEVEL;
            }
            const auto ws_start = it;
            GLZ_SKIP_WS();
            const size_t ws_size = size_t(it - ws_start);

            if constexpr ((glaze_object_t<T> || reflectable<T>)&&num_members == 0 && Opts.error_on_unknown_keys) {
               if (*it == '}') [[likely]] {
                  GLZ_SUB_LEVEL;
                  ++it;
                  GLZ_VALID_END();
                  if constexpr (Opts.partial_read) {
                     ctx.error = error_code::partial_read_complete;
                  }
                  return;
               }
               ctx.error = error_code::unknown_key;
               return;
            }
            else {
               decltype(auto) fields = [&]() -> decltype(auto) {
                  if constexpr ((glaze_object_t<T> || reflectable<T>)&&(Opts.error_on_missing_keys ||
                                                                        Opts.partial_read)) {
                     return bit_array<num_members>{};
                  }
                  else {
                     return nullptr;
                  }
               }();

               size_t read_count{}; // for partial_read

               bool first = true;
               while (true) {
                  if constexpr ((glaze_object_t<T> || reflectable<T>)&&Opts.partial_read) {
                     static constexpr bit_array<num_members> all_fields = [] {
                        bit_array<num_members> arr{};
                        for (size_t i = 0; i < num_members; ++i) {
                           arr[i] = true;
                        }
                        return arr;
                     }();

                     if ((all_fields & fields) == all_fields) {
                        ctx.error = error_code::partial_read_complete;
                        return;
                     }
                  }

                  if (*it == '}') {
                     GLZ_SUB_LEVEL;
                     if constexpr ((glaze_object_t<T> ||
                                    reflectable<T>)&&(Opts.partial_read && Opts.error_on_missing_keys)) {
                        ctx.error = error_code::missing_key;
                        return;
                     }
                     else {
                        ++it;
                        if constexpr ((glaze_object_t<T> || reflectable<T>)&&Opts.error_on_missing_keys) {
                           constexpr auto req_fields = required_fields<T, Opts>();
                           if ((req_fields & fields) != req_fields) {
                              ctx.error = error_code::missing_key;
                              return;
                           }
                        }
                        GLZ_VALID_END();
                     }
                     return;
                  }
                  else if (first) {
                     first = false;
                  }
                  else {
                     GLZ_MATCH_COMMA;
                     GLZ_INVALID_END();

                     if constexpr ((not Opts.minified) && (num_members > 1 || not Opts.error_on_unknown_keys)) {
                        if (ws_size && ws_size < size_t(end - it)) {
                           skip_matching_ws(ws_start, it, ws_size);
                        }
                     }

                     GLZ_SKIP_WS();
                  }

                  constexpr auto reflection_type = glaze_object_t<T> || reflectable<T>;

                  if constexpr (reflection_type && (num_members == 0)) {
                     if constexpr (Opts.error_on_unknown_keys) {
                        static_assert(false_v<T>, "This should be unreachable");
                     }
                     else {
                        GLZ_MATCH_QUOTE;
                        GLZ_INVALID_END();

                        // parsing to an empty object, but at this point the JSON presents keys

                        // Unknown key handler does not unescape keys. Unknown escaped keys are
                        // handled by the user.

                        const auto start = it;
                        skip_string_view<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        const sv key{start, size_t(it - start)};
                        ++it;
                        GLZ_INVALID_END();

                        GLZ_PARSE_WS_COLON;

                        read<JSON>::handle_unknown<Opts>(key, value, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
                  else if constexpr (reflection_type) {
                     static_assert(bool(hash_info<T>.type));

                     if (*it != '"') [[unlikely]] {
                        ctx.error = error_code::expected_quote;
                        return;
                     }
                     ++it;
                     GLZ_INVALID_END();

                     if constexpr (not tag.sv().empty() && not contains_tag<T, tag>()) {
                        // For tagged variants we first check to see if the key matches the tag
                        // We only need to do this if the tag is not part of the keys

                        const auto start = it;
                        skip_string_view<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        const sv key{start, size_t(it - start)};
                        ++it;
                        GLZ_INVALID_END();

                        if (key == tag.sv()) {
                           GLZ_PARSE_WS_COLON;

                           read<JSON>::handle_unknown<Opts>(key, value, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;

                           GLZ_SKIP_WS();
                           continue;
                        }
                        else {
                           it = start; // reset the iterator
                        }
                     }

                     if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
                        size_t index = num_members;
                        parse_and_invoke<Opts, T, hash_info<T>>(value, ctx, it, end, index);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        if (index < num_members) {
                           fields[index] = true;
                        }
                     }
                     else {
                        parse_and_invoke<Opts, T, hash_info<T>>(value, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
                  else {
                     // For types like std::map, std::unordered_map

                     auto reading = [&](auto&& key) {
                        if constexpr (Opts.partial_read) {
                           if (auto element = value.find(key); element != value.end()) {
                              ++read_count;
                              read<JSON>::op<ws_handled<Opts>()>(element->second, ctx, it, end);
                           }
                           else {
                              skip_value<JSON>::op<Opts>(ctx, it, end);
                           }
                        }
                        else {
                           read<JSON>::op<ws_handled<Opts>()>(value[key], ctx, it, end);
                        }
                     };

                     // using Key = std::conditional_t<heterogeneous_map<T>, sv, typename T::key_type>;
                     using Key = typename T::key_type;
                     if constexpr (std::is_same_v<Key, std::string>) {
                        static thread_local Key key;
                        read<JSON>::op<Opts>(key, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        GLZ_PARSE_WS_COLON;

                        reading(key);
                        if constexpr (Opts.partial_read) {
                           if (read_count == value.size()) {
                              return;
                           }
                        }
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else if constexpr (str_t<Key>) {
                        Key key;
                        read<JSON>::op<Opts>(key, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        GLZ_PARSE_WS_COLON;

                        reading(key);
                        if constexpr (Opts.partial_read) {
                           if (read_count == value.size()) {
                              return;
                           }
                        }
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else {
                        Key key_value{};
                        if constexpr (glaze_enum_t<Key>) {
                           read<JSON>::op<Opts>(key_value, ctx, it, end);
                        }
                        else if constexpr (std::is_arithmetic_v<Key>) {
                           // prefer over quoted_t below to avoid double parsing of quoted_t
                           read<JSON>::op<opt_true<Opts, &opts::quoted_num>>(key_value, ctx, it, end);
                        }
                        else {
                           read<JSON>::op<opt_false<Opts, &opts::raw_string>>(quoted_t<Key>{key_value}, ctx, it, end);
                        }
                        if (bool(ctx.error)) [[unlikely]]
                           return;

                        GLZ_PARSE_WS_COLON;

                        reading(key_value);
                        if constexpr (Opts.partial_read) {
                           if (read_count == value.size()) {
                              return;
                           }
                        }
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
                  GLZ_SKIP_WS();
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
            objects += (writable_map_t<V> || readable_map_t<V> || is_memory_object<V>);
            objects += glaze_object_t<V>;
            meta_objects += glaze_object_t<V> || reflectable<V> || is_memory_object<V>;
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
         using string_types = decltype(tuplet::tuple_cat( // glaze_enum_t remove_meta_wrapper_t supports constexpr
                                                          // types while the other supports non const
            std::conditional_t < str_t<remove_meta_wrapper_t<Ts>> || glaze_enum_t<remove_meta_wrapper_t<Ts>> ||
               glaze_enum_t<Ts>,
            tuplet::tuple<Ts>, tuplet::tuple < >> {}...));
         using object_types =
            decltype(tuplet::tuple_cat(std::conditional_t<json_object<Ts>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
         using array_types =
            decltype(tuplet::tuple_cat(std::conditional_t < array_t<remove_meta_wrapper_t<Ts>> || glaze_array_t<Ts>,
                                       tuplet::tuple<Ts>, tuplet::tuple < >> {}...));
         using nullable_types =
            decltype(tuplet::tuple_cat(std::conditional_t<null_t<Ts>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
         using nullable_objects = decltype(tuplet::tuple_cat(
            std::conditional_t<is_memory_object<Ts>, tuplet::tuple<Ts>, tuplet::tuple<>>{}...));
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
         static constexpr auto n_bool = glz::tuple_size_v<typename V::bool_types>;
         static constexpr auto n_number = glz::tuple_size_v<typename V::number_types>;
         static constexpr auto n_string = glz::tuple_size_v<typename V::string_types>;
         static constexpr auto n_nullable_object = glz::tuple_size_v<typename V::nullable_objects>;
         static constexpr auto n_object = glz::tuple_size_v<typename V::object_types> + n_nullable_object;
         static constexpr auto n_array = glz::tuple_size_v<typename V::array_types>;
         static constexpr auto n_null = glz::tuple_size_v<typename V::nullable_types>;
      };

      template <class Tuple>
      struct process_arithmetic_boolean_string_or_array
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (glz::tuple_size_v<Tuple> < 1) {
               ctx.error = error_code::no_matching_variant_type;
            }
            else {
               using const_glaze_types = typename tuple_types<Tuple>::glaze_const_types;
               bool found_match{};
               for_each<glz::tuple_size_v<const_glaze_types>>([&]([[maybe_unused]] auto I) {
                  if (found_match) {
                     return;
                  }
                  using V = glz::tuple_element_t<I, const_glaze_types>;
                  // run time substitute to compare to const value
                  std::remove_const_t<std::remove_pointer_t<std::remove_const_t<meta_wrapper_t<V>>>> substitute{};
                  auto copy_it{it};
                  read<JSON>::op<ws_handled<Options>()>(substitute, ctx, it, end);
                  static constexpr auto const_value{*meta_wrapper_v<V>};
                  if (substitute == const_value) {
                     found_match = true;
                     if (!std::holds_alternative<V>(value)) {
                        value = V{};
                     }
                  }
                  else {
                     if constexpr (not Options.null_terminated) {
                        if (ctx.error == error_code::end_reached) {
                           // reset the context for next attempt
                           ctx.error = error_code::none;
                        }
                     }
                     it = copy_it;
                  }
               });
               if (found_match) {
                  return;
               }

               using non_const_types = typename tuple_types<Tuple>::glaze_non_const_types;
               if constexpr (glz::tuple_size_v < non_const_types >> 0) {
                  using V = glz::tuple_element_t<0, non_const_types>;
                  if (!std::holds_alternative<V>(value)) value = V{};
                  read<JSON>::op<ws_handled<Options>()>(std::get<V>(value), ctx, it, end);
               }
               else {
                  ctx.error = error_code::no_matching_variant_type;
               }
            }
         }
      };

      template <is_variant T>
      struct from<JSON, T>
      {
         // Note that items in the variant are required to be default constructable for us to switch types
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            constexpr auto Opts = ws_handled_off<Options>();
            if constexpr (variant_is_auto_deducible<T>()) {
               if constexpr (not has_ws_handled(Options)) {
                  GLZ_SKIP_WS();
               }

               switch (*it) {
               case '\0':
                  ctx.error = error_code::unexpected_end;
                  return;
               case '{':
                  if (ctx.indentation_level >= max_recursive_depth_limit) {
                     ctx.error = error_code::exceeded_max_recursive_depth;
                     return;
                  }
                  // In the null terminated case this guards for stack overflow
                  // Depth counting is done at the object level when not null terminated
                  ++ctx.indentation_level;

                  ++it;
                  GLZ_INVALID_END();
                  using type_counts = variant_type_count<T>;
                  using object_types = typename variant_types<T>::object_types;
                  if constexpr ((type_counts::n_object < 1) //
                                && (type_counts::n_nullable_object < 1)) {
                     ctx.error = error_code::no_matching_variant_type;
                     return;
                  }
                  else if constexpr ((type_counts::n_object + type_counts::n_nullable_object) == 1 &&
                                     tag_v<T>.empty()) {
                     using V = glz::tuple_element_t<0, object_types>;
                     if (!std::holds_alternative<V>(value)) value = V{};
                     read<JSON>::op<opening_handled<Opts>()>(std::get<V>(value), ctx, it, end);
                     if constexpr (Opts.null_terminated) {
                        // In the null terminated case this guards for stack overflow
                        // Depth counting is done at the object level when not null terminated
                        --ctx.indentation_level;
                     }
                     return;
                  }
                  else {
                     auto possible_types = bit_array<std::variant_size_v<T>>{}.flip();
                     static constexpr auto deduction_map = detail::make_variant_deduction_map<T>();
                     static constexpr auto tag_literal = string_literal_from_view<tag_v<T>.size()>(tag_v<T>);
                     GLZ_SKIP_WS();
                     auto start = it;
                     while (*it != '}') {
                        if (it != start) {
                           GLZ_MATCH_COMMA;
                        }

                        GLZ_SKIP_WS();
                        GLZ_MATCH_QUOTE;
                        GLZ_INVALID_END();

                        auto* key_start = it;
                        skip_string_view<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        const sv key = {key_start, size_t(it - key_start)};

                        GLZ_MATCH_QUOTE;
                        GLZ_INVALID_END();

                        if constexpr (deduction_map.size()) {
                           // We first check if a tag is defined and see if the key matches the tag
                           if constexpr (not tag_v<T>.empty()) {
                              if (key == tag_v<T>) {
                                 GLZ_PARSE_WS_COLON;
                                 sv type_id{};
                                 from<JSON, sv>::template op<ws_handled<Opts>()>(type_id, ctx, it, end);
                                 if (bool(ctx.error)) [[unlikely]]
                                    return;
                                 GLZ_SKIP_WS();
                                 if (!(*it == ',' || *it == '}')) [[unlikely]] {
                                    ctx.error = error_code::syntax_error;
                                    return;
                                 }

                                 static constexpr auto id_map = make_variant_id_map<T>();
                                 auto id_it = id_map.find(type_id);
                                 if (id_it != id_map.end()) [[likely]] {
                                    it = start; // we restart our object parsing now that we know the target type
                                    const auto type_index = id_it->second;
                                    if (value.index() != type_index) value = runtime_variant_map<T>()[type_index];
                                    std::visit(
                                       [&](auto&& v) {
                                          using V = std::decay_t<decltype(v)>;
                                          constexpr bool is_object = glaze_object_t<V> || reflectable<V>;
                                          if constexpr (is_object) {
                                             from<JSON, V>::template op<opening_handled<Opts>(), tag_literal>(v, ctx,
                                                                                                              it, end);
                                          }
                                          else if constexpr (is_memory_object<V>) {
                                             if (!v) {
                                                if constexpr (is_specialization_v<V, std::optional>) {
                                                   if constexpr (requires { v.emplace(); }) {
                                                      v.emplace();
                                                   }
                                                   else {
                                                      v = typename V::value_type{};
                                                   }
                                                }
                                                else if constexpr (is_specialization_v<V, std::unique_ptr>)
                                                   v = std::make_unique<typename V::element_type>();
                                                else if constexpr (is_specialization_v<V, std::shared_ptr>)
                                                   v = std::make_shared<typename V::element_type>();
                                                else if constexpr (constructible<V>) {
                                                   v = meta_construct_v<V>();
                                                }
                                                else {
                                                   ctx.error = error_code::invalid_nullable_read;
                                                   return;
                                                   // Cannot read into unset nullable that is not std::optional,
                                                   // std::unique_ptr, or std::shared_ptr
                                                }
                                             }
                                             from<JSON, memory_type<V>>::template op<opening_handled<Opts>(),
                                                                                     tag_literal>(*v, ctx, it, end);
                                          }
                                       },
                                       value);

                                    if constexpr (Opts.null_terminated) {
                                       // In the null terminated case this guards for stack overflow
                                       // Depth counting is done at the object level when not null terminated
                                       --ctx.indentation_level;
                                    }
                                    return; // we've decoded our target type
                                 }
                                 else [[unlikely]] {
                                    ctx.error = error_code::no_matching_variant_type;
                                    return;
                                 }
                              }
                           }

                           auto deduction_it = deduction_map.find(key);
                           if (deduction_it != deduction_map.end()) [[likely]] {
                              possible_types &= deduction_it->second;
                           }
                           else if constexpr (Opts.error_on_unknown_keys) {
                              ctx.error = error_code::unknown_key;
                              return;
                           }
                        }
                        else if constexpr (not tag_v<T>.empty()) {
                           // empty object case for variant, if there are no normal elements
                           if (key == tag_v<T>) {
                              GLZ_PARSE_WS_COLON;

                              std::string_view type_id{};
                              read<JSON>::op<ws_handled<Opts>()>(type_id, ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                              GLZ_SKIP_WS();

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
                                    from<JSON, V>::template op<opening_handled<Opts>(), tag_literal>(v, ctx, it, end);
                                 }
                                 else if constexpr (is_memory_object<V>) {
                                    if (!v) {
                                       if constexpr (is_specialization_v<V, std::optional>) {
                                          if constexpr (requires { v.emplace(); }) {
                                             v.emplace();
                                          }
                                          else {
                                             v = typename V::value_type{};
                                          }
                                       }
                                       else if constexpr (is_specialization_v<V, std::unique_ptr>)
                                          v = std::make_unique<typename V::element_type>();
                                       else if constexpr (is_specialization_v<V, std::shared_ptr>)
                                          v = std::make_shared<typename V::element_type>();
                                       else if constexpr (constructible<V>) {
                                          v = meta_construct_v<V>();
                                       }
                                       else {
                                          ctx.error = error_code::invalid_nullable_read;
                                          return;
                                          // Cannot read into unset nullable that is not std::optional,
                                          // std::unique_ptr, or std::shared_ptr
                                       }
                                    }
                                    from<JSON, memory_type<V>>::template op<opening_handled<Opts>(), tag_literal>(
                                       *v, ctx, it, end);
                                 }
                              },
                              value);

                           if constexpr (Opts.null_terminated) {
                              // In the null terminated case this guards for stack overflow
                              // Depth counting is done at the object level when not null terminated
                              --ctx.indentation_level;
                           }
                           return; // we've decoded our target type
                        }
                        GLZ_PARSE_WS_COLON;

                        skip_value<JSON>::op<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        GLZ_SKIP_WS();
                     }
                     ctx.error = error_code::no_matching_variant_type;
                  }
                  break;
               case '[':
                  using array_types = typename variant_types<T>::array_types;

                  if (ctx.indentation_level >= max_recursive_depth_limit) {
                     ctx.error = error_code::exceeded_max_recursive_depth;
                     return;
                  }
                  if constexpr (Opts.null_terminated) {
                     // In the null terminated case this guards for stack overflow
                     // Depth counting is done at the object level when not null terminated
                     ++ctx.indentation_level;
                  }
                  process_arithmetic_boolean_string_or_array<array_types>::template op<Opts>(value, ctx, it, end);
                  if constexpr (Opts.null_terminated) {
                     --ctx.indentation_level;
                  }
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
                  if constexpr (glz::tuple_size_v<nullable_types> < 1) {
                     ctx.error = error_code::no_matching_variant_type;
                  }
                  else {
                     using V = glz::tuple_element_t<0, nullable_types>;
                     if (!std::holds_alternative<V>(value)) value = V{};
                     match<"null", Opts>(ctx, it, end);
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
               std::visit([&](auto&& v) { read<JSON>::op<Options>(v, ctx, it, end); }, value);
            }
         }
      };

      template <class T>
      struct from<JSON, array_variant_wrapper<T>>
      {
         template <auto Options>
         static void op(auto&& wrapper, is_context auto&& ctx, auto&& it, auto&& end)
         {
            auto& value = wrapper.value;

            constexpr auto Opts = ws_handled_off<Options>();
            if constexpr (!has_ws_handled(Options)) {
               GLZ_SKIP_WS();
            }

            GLZ_MATCH_OPEN_BRACKET;
            GLZ_ADD_LEVEL;
            GLZ_SKIP_WS();

            // TODO Use key parsing for compiletime known keys
            GLZ_MATCH_QUOTE;
            GLZ_INVALID_END();
            auto start = it;
            skip_string_view<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
            sv type_id = {start, size_t(it - start)};
            GLZ_MATCH_QUOTE;
            GLZ_VALID_END();

            static constexpr auto id_map = make_variant_id_map<T>();
            auto id_it = id_map.find(type_id);
            if (id_it != id_map.end()) [[likely]] {
               GLZ_SKIP_WS();
               GLZ_MATCH_COMMA;
               const auto type_index = id_it->second;
               if (value.index() != type_index) value = runtime_variant_map<T>()[type_index];
               std::visit([&](auto&& v) { read<JSON>::op<Opts>(v, ctx, it, end); }, value);
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            else {
               ctx.error = error_code::no_matching_variant_type;
               return;
            }

            GLZ_SKIP_WS();
            match<']'>(ctx, it);
            GLZ_SUB_LEVEL;
         }
      };

      template <is_expected T>
      struct from<JSON, T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (!has_ws_handled(Opts)) {
               GLZ_SKIP_WS();
            }

            if (*it == '{') {
               GLZ_ADD_LEVEL;
               auto start = it;
               ++it;
               GLZ_INVALID_END();
               GLZ_SKIP_WS();
               if (*it == '}') {
                  it = start;
                  // empty object
                  if (value) {
                     read<JSON>::op<Opts>(*value, ctx, it, end);
                  }
                  else {
                     value.emplace();
                     read<JSON>::op<Opts>(*value, ctx, it, end);
                  }
               }
               else {
                  // either we have an unexpected value or we are decoding an object
                  auto& key = string_buffer();
                  read<JSON>::op<Opts>(key, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  if (key == "unexpected") {
                     GLZ_SKIP_WS();
                     GLZ_MATCH_COLON();
                     // read in unexpected value
                     if (!value) {
                        read<JSON>::op<Opts>(value.error(), ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else {
                        // set value to unexpected
                        using error_ctx = typename std::decay_t<decltype(value)>::error_type;
                        std::decay_t<error_ctx> error{};
                        read<JSON>::op<Opts>(error, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        value = glz::unexpected(error);
                     }
                     GLZ_SKIP_WS();
                     GLZ_MATCH_CLOSE_BRACE;
                     GLZ_SUB_LEVEL;
                  }
                  else {
                     it = start;
                     if (value) {
                        read<JSON>::op<Opts>(*value, ctx, it, end);
                     }
                     else {
                        value.emplace();
                        read<JSON>::op<Opts>(*value, ctx, it, end);
                     }
                  }
               }
            }
            else {
               // this is not an object and therefore cannot be an unexpected value
               if (value) {
                  read<JSON>::op<Opts>(*value, ctx, it, end);
               }
               else {
                  value.emplace();
                  read<JSON>::op<Opts>(*value, ctx, it, end);
               }
            }
         }
      };

      template <nullable_t T>
         requires(std::is_array_v<T>)
      struct from<JSON, T>
      {
         template <auto Opts, class V, size_t N>
         GLZ_ALWAYS_INLINE static void op(V (&value)[N], is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            read<JSON>::op<Opts>(std::span{value, N}, ctx, it, end);
         }
      };

      template <class T>
         requires((nullable_t<T> || nullable_value_t<T>) && not is_expected<T> && not std::is_array_v<T> &&
                  not custom_read<T>)
      struct from<JSON, T>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            constexpr auto Opts = ws_handled_off<Options>();
            if constexpr (!has_ws_handled(Options)) {
               GLZ_SKIP_WS();
            }

            if (*it == 'n') {
               ++it;
               GLZ_INVALID_END();
               match<"ull", Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if constexpr (requires { value.reset(); }) {
                  value.reset();
               }
            }
            else {
               if constexpr (nullable_value_t<T>) {
                  if (not value.has_value()) {
                     if constexpr (constructible<T>) {
                        value = meta_construct_v<T>();
                     }
                     else if constexpr (requires { value.emplace(); }) {
                        value.emplace();
                     }
                     else {
                        static_assert(false_v<T>,
                                      "Your nullable type must have `emplace()` or be glz::meta constructible, or "
                                      "create a custom glz::detail::from specialization");
                     }
                  }
                  read<JSON>::op<Opts>(value.value(), ctx, it, end);
               }
               else {
                  if (!value) {
                     if constexpr (optional_like<T>) {
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
                        // Cannot read into a null raw pointer
                        ctx.error = error_code::invalid_nullable_read;
                        return;
                     }
                  }
                  read<JSON>::op<Opts>(*value, ctx, it, end);
               }
            }
         }
      };

      template <filesystem_path T>
      struct from<JSON, T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            std::string& buffer = string_buffer();
            read<JSON>::op<Opts>(buffer, ctx, it, end);
            if constexpr (Opts.null_terminated) {
               if (bool(ctx.error)) [[unlikely]]
                  return;
            }
            else {
               if (size_t(ctx.error) > size_t(error_code::end_reached)) [[unlikely]] {
                  return;
               }
            }
            value = buffer;
         }
      };
   } // namespace detail

   template <is_buffer Buffer>
   [[nodiscard]] error_ctx validate_json(Buffer&& buffer) noexcept
   {
      context ctx{};
      glz::skip skip_value{};
      return read<opts{.validate_skipped = true, .validate_trailing_whitespace = true}>(
         skip_value, std::forward<Buffer>(buffer), ctx);
   }

   template <is_buffer Buffer>
   [[nodiscard]] error_ctx validate_jsonc(Buffer&& buffer) noexcept
   {
      context ctx{};
      glz::skip skip_value{};
      return read<opts{.comments = true, .validate_skipped = true, .validate_trailing_whitespace = true}>(
         skip_value, std::forward<Buffer>(buffer), ctx);
   }

   template <read_json_supported T, is_buffer Buffer>
   [[nodiscard]] error_ctx read_json(T& value, Buffer&& buffer)
   {
      context ctx{};
      return read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
   }

   template <read_json_supported T, is_buffer Buffer>
   [[nodiscard]] expected<T, error_ctx> read_json(Buffer&& buffer)
   {
      T value{};
      context ctx{};
      const error_ctx ec = read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return value;
   }

   template <read_json_supported T, is_buffer Buffer>
   [[nodiscard]] error_ctx read_jsonc(T& value, Buffer&& buffer)
   {
      context ctx{};
      return read<opts{.comments = true}>(value, std::forward<Buffer>(buffer), ctx);
   }

   template <read_json_supported T, is_buffer Buffer>
   [[nodiscard]] expected<T, error_ctx> read_jsonc(Buffer&& buffer)
   {
      T value{};
      context ctx{};
      const error_ctx ec = read<opts{.comments = true}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return value;
   }

   template <auto Opts = opts{}, read_json_supported T, is_buffer Buffer>
   [[nodiscard]] error_ctx read_file_json(T& value, const sv file_name, Buffer&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {ec};
      }

      return read<set_json<Opts>()>(value, buffer, ctx);
   }

   template <auto Opts = opts{}, read_json_supported T, is_buffer Buffer>
   [[nodiscard]] error_ctx read_file_jsonc(T& value, const sv file_name, Buffer&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {ec};
      }

      constexpr auto Options = opt_true<set_json<Opts>(), &opts::comments>;
      return read<Options>(value, buffer, ctx);
   }
}

#ifdef _MSC_VER
// restore disabled warnings
#pragma warning(pop)
#endif
