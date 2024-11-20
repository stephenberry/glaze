// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <iterator>
#include <ostream>
#include <variant>

#if defined(__APPLE__)
#elif defined(_MSC_VER)
#include <intrin.h>
#elif defined(GLZ_USE_AVX2)
#include <immintrin.h>
#endif

#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
#include "glaze/json/ptr.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/itoa.hpp"

namespace glz
{
   namespace detail
   {
      template <>
      struct write<JSON>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            to<JSON, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <class T>
         requires(glaze_value_t<T> && !custom_write<T>)
      struct to<JSON, T>
      {
         template <auto Opts, class Value, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            using V = std::remove_cvref_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
            to<JSON, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                           std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <class T>
      concept optional_like = nullable_t<T> && (!is_expected<T> && !std::is_array_v<T>);

      template <class T>
      concept supports_unchecked_write = boolean_like<T> || num_t<T> || optional_like<T> || always_null_t<T>;

      template <class T>
      inline constexpr std::optional<size_t> required_padding()
      {
         if constexpr (boolean_like<T>) {
            return 8;
         }
         else if constexpr (num_t<T>) {
            return 64;
         }
         else if constexpr (optional_like<T> &&
                            (
                               requires { requires supports_unchecked_write<typename T::value_type>; } ||
                               requires { requires supports_unchecked_write<typename T::element_type>; })) {
            if constexpr (requires { requires supports_unchecked_write<typename T::value_type>; }) {
               return required_padding<typename T::value_type>();
            }
            else if constexpr (requires { requires supports_unchecked_write<typename T::element_type>; }) {
               return required_padding<typename T::element_type>();
            }
         }
         else if constexpr (always_null_t<T>) {
            return 8;
         }
         else {
            return {};
         }
      }

      template <is_bitset T>
      struct to<JSON, T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&&, auto&& b, auto&& ix)
         {
            dump<'"'>(b, ix);
            for (size_t i = value.size(); i > 0; --i) {
               value[i - 1] ? dump<'1'>(b, ix) : dump<'0'>(b, ix);
            }
            dump<'"'>(b, ix);
         }
      };

      template <glaze_flags_t T>
      struct to<JSON, T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix)
         {
            static constexpr auto N = reflect<T>::size;

            dump<'['>(b, ix);

            invoke_table<N>([&]<size_t I>() {
               if (get_member(value, get<I>(reflect<T>::values))) {
                  dump<'"'>(b, ix);
                  dump_maybe_empty(reflect<T>::keys[I], b, ix);
                  dump<"\",">(b, ix);
               }
            });

            if (b[ix - 1] == ',') {
               b[ix - 1] = ']';
            }
            else {
               dump<']'>(b, ix);
            }
         }
      };

      template <>
      struct to<JSON, hidden>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&... args)
         {
            dump(R"("hidden type should not have been written")", args...);
         }
      };

      template <>
      struct to<JSON, skip>
      {
         template <auto Opts>
         static void op(auto&& value, auto&&...) noexcept
         {
            static_assert(false_v<decltype(value)>, "skip type should not be written");
         }
      };

      template <is_member_function_pointer T>
      struct to<JSON, T>
      {
         template <auto Opts>
         static void op(auto&&, is_context auto&&, auto&&...) noexcept
         {}
      };

      template <is_reference_wrapper T>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args)
         {
            using V = std::remove_cvref_t<decltype(value.get())>;
            to<JSON, V>::template op<Opts>(value.get(), std::forward<Args>(args)...);
         }
      };

      template <complex_t T>
      struct to<JSON, T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix)
         {
            dump<'['>(b, ix);
            write<JSON>::op<Opts>(value.real(), ctx, b, ix);
            dump<','>(b, ix);
            write<JSON>::op<Opts>(value.imag(), ctx, b, ix);
            dump<']'>(b, ix);
         }
      };

      template <boolean_like T>
      struct to<JSON, T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&&, B&& b, auto&& ix)
         {
            static constexpr auto checked = not has_write_unchecked(Opts);
            if constexpr (checked && vector_like<B>) {
               if ((ix + 8) > b.size()) [[unlikely]] {
                  b.resize((std::max)(b.size() * 2, ix + 8));
               }
            }

            if constexpr (Opts.bools_as_numbers) {
               if (value) {
                  std::memcpy(&b[ix], "1", 1);
               }
               else {
                  std::memcpy(&b[ix], "0", 1);
               }
               ++ix;
            }
            else {
               // branchless dumping of `true` or `false`
               static constexpr uint64_t false_v = 435728179558;
               static constexpr uint64_t if_true_v = 434025983730;

               const uint64_t state = false_v - (value * if_true_v);
               std::memcpy(&b[ix], &state, 8);
               ix += 5 - value;
            }
         }
      };

      template <num_t T>
      struct to<JSON, T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix)
         {
            constexpr auto checked = not has_write_unchecked(Opts);
            if constexpr (Opts.quoted_num) {
               dump<'"', checked>(b, ix);
            }
            write_chars::op<Opts>(value, ctx, b, ix);
            if constexpr (Opts.quoted_num) {
               dump<'"', checked>(b, ix);
            }
         }
      };

      constexpr std::array<uint16_t, 256> char_escape_table = [] {
         auto combine = [](const char chars[2]) -> uint16_t { return uint16_t(chars[0]) | (uint16_t(chars[1]) << 8); };

         std::array<uint16_t, 256> t{};
         t['\b'] = combine(R"(\b)");
         t['\t'] = combine(R"(\t)");
         t['\n'] = combine(R"(\n)");
         t['\f'] = combine(R"(\f)");
         t['\r'] = combine(R"(\r)");
         t['\"'] = combine(R"(\")");
         t['\\'] = combine(R"(\\)");
         return t;
      }();

      template <class T>
         requires str_t<T> || char_t<T>
      struct to<JSON, T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&&, B&& b, auto&& ix)
         {
            if constexpr (Opts.number) {
               dump_maybe_empty(value, b, ix);
            }
            else if constexpr (char_t<T>) {
               if constexpr (Opts.raw) {
                  dump(value, b, ix);
               }
               else {
                  if constexpr (resizable<B>) {
                     const auto k = ix + 4; // 4 characters is enough for quotes and escaped character
                     if (k >= b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, k));
                     }
                  }

                  dump<'"', false>(b, ix);
                  if (const auto escaped = char_escape_table[uint8_t(value)]; escaped) {
                     std::memcpy(&b[ix], &escaped, 2);
                     ix += 2;
                  }
                  else if (value == '\0') {
                     // null character treated as empty string
                  }
                  else {
                     dump<false>(value, b, ix);
                  }
                  dump<'"', false>(b, ix);
               }
            }
            else {
               if constexpr (Opts.raw_string) {
                  const sv str = [&]() -> const sv {
                     if constexpr (!char_array_t<T> && std::is_pointer_v<std::decay_t<T>>) {
                        return value ? value : "";
                     }
                     else {
                        return value;
                     }
                  }();

                  // We need space for quotes and the string length: 2 + n.
                  if constexpr (resizable<B>) {
                     const auto n = str.size();
                     const auto k = ix + 2 + n;
                     if (k >= b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, k));
                     }
                  }
                  // now we don't have to check writing

                  dump<'"', false>(b, ix);
                  if (str.size()) [[likely]] {
                     dump<false>(str, b, ix);
                  }
                  dump<'"', false>(b, ix);
               }
               else {
                  const sv str = [&]() -> const sv {
                     if constexpr (!char_array_t<T> && std::is_pointer_v<std::decay_t<T>>) {
                        return value ? value : "";
                     }
                     else if constexpr (array_char_t<T>) {
                        return *value.data() ? sv{value.data()} : "";
                     }
                     else {
                        return value;
                     }
                  }();
                  const auto n = str.size();

                  // In the case n == 0 we need two characters for quotes.
                  // For each individual character we need room for two characters to handle escapes.
                  // So, we need 2 + 2 * n characters to handle all cases.
                  // We add another 8 characters to support SWAR
                  if constexpr (resizable<B>) {
                     const auto k = ix + 10 + 2 * n;
                     if (k >= b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, k));
                     }
                  }
                  // now we don't have to check writing

                  if constexpr (Opts.raw) {
                     if (str.size()) [[likely]] {
                        dump<false>(str, b, ix);
                     }
                  }
                  else {
                     dump<'"', false>(b, ix);

                     const auto* c = str.data();
                     const auto* const e = c + n;
                     const auto start = &b[ix];
                     auto data = start;

                     // We don't check for writing out invalid characters as this can be tested by the user if
                     // necessary. In the case of invalid JSON characters we write out null characters to
                     // showcase the error and make the JSON invalid. These would then be detected upon reading
                     // the JSON.

                     // This 128bit SWAR approach tends to be slower than SIMD approaches
                     /*for (const auto end_m15 = e - 15; c < end_m15;) {
                        std::memcpy(data, c, 16);
                        __uint128_t swar;
                        std::memcpy(&swar, c, 16);

                        constexpr __uint128_t lo7_mask = repeat_byte16(0b01111111);
                        const __uint128_t lo7 = swar & lo7_mask;
                        const __uint128_t quote = (lo7 ^ repeat_byte16('"')) + lo7_mask;
                        const __uint128_t backslash = (lo7 ^ repeat_byte16('\\')) + lo7_mask;
                        const __uint128_t less_32 = (swar & repeat_byte16(0b01100000)) + lo7_mask;
                        __uint128_t next = ~((quote & backslash & less_32) | swar);

                        next &= repeat_byte16(0b10000000);
                        if (next == 0) {
                           data += 16;
                           c += 16;
                           continue;
                        }

                        const auto length = (countr_zero(next) >> 3);
                        c += length;
                        data += length;

                        std::memcpy(data, &char_escape_table[uint8_t(*c)], 2);
                        data += 2;
                        ++c;
                     }*/

#if defined(__APPLE__)
                     // This approach is faster when strings don't contain many escapes
                     // But, this is not faster in the general case
                     /*if (n > 15) {
                        const uint8x16_t lo7_mask = vdupq_n_u8(0b01111111);
                        const uint8x16_t quote_char = vdupq_n_u8('"');
                        const uint8x16_t backslash_char = vdupq_n_u8('\\');
                        const uint8x16_t less_32_mask = vdupq_n_u8(0b01100000);
                        const uint8x16_t high_bit_mask = vdupq_n_u8(0b10000000);

                        for (const auto end_m15 = e - 15; c < end_m15;) {
                           uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(c));

                           vst1q_u8(reinterpret_cast<uint8_t*>(data), v);

                           const uint8x16_t lo7 = vandq_u8(v, lo7_mask);
                           const uint8x16_t quote = vaddq_u8(veorq_u8(lo7, quote_char), lo7_mask);
                           const uint8x16_t backslash = vaddq_u8(veorq_u8(lo7, backslash_char), lo7_mask);
                           const uint8x16_t less_32 = vaddq_u8(vandq_u8(v, less_32_mask), lo7_mask);

                           uint8x16_t temp = vandq_u8(quote, backslash);
                           temp = vandq_u8(temp, less_32);
                           temp = vorrq_u8(temp, v);
                           uint8x16_t next = vmvnq_u8(temp);
                           next = vandq_u8(next, high_bit_mask);

                           uint64x2_t next64 = vreinterpretq_u64_u8(next);
                           uint64_t next_low = vgetq_lane_u64(next64, 0);
                           uint64_t next_high = vgetq_lane_u64(next64, 1);

                           if (next_low == 0 && next_high == 0) {
                              data += 16;
                              c += 16;
                              continue;
                           }

                           uint32_t length;
                           if (next_low != 0) {
                              length = (__builtin_ctzll(next_low)) >> 3;
                           }
                           else {
                              length = (__builtin_ctzll(next_high) >> 3) + 8;
                           }

                           c += length;
                           data += length;

                           std::memcpy(data, &char_escape_table[uint8_t(*c)], 2);
                           data += 2;
                           ++c;
                        }
                     }*/
#elif defined(GLZ_USE_AVX2)
                     // Optimization for systems with AVX2 support
                     if (n > 31) {
                        const __m256i lo7_mask = _mm256_set1_epi8(0b01111111);
                        const __m256i quote_char = _mm256_set1_epi8('"');
                        const __m256i backslash_char = _mm256_set1_epi8('\\');
                        const __m256i less_32_mask = _mm256_set1_epi8(0b01100000);
                        const __m256i high_bit_mask = _mm256_set1_epi8(static_cast<int8_t>(0b10000000));

                        for (const char* end_m31 = e - 31; c < end_m31;) {
                           __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(c));

                           _mm256_storeu_si256(reinterpret_cast<__m256i*>(data), v);

                           const __m256i lo7 = _mm256_and_si256(v, lo7_mask);
                           const __m256i quote = _mm256_add_epi8(_mm256_xor_si256(lo7, quote_char), lo7_mask);
                           const __m256i backslash = _mm256_add_epi8(_mm256_xor_si256(lo7, backslash_char), lo7_mask);
                           const __m256i less_32 = _mm256_add_epi8(_mm256_and_si256(v, less_32_mask), lo7_mask);

                           __m256i temp = _mm256_and_si256(quote, backslash);
                           temp = _mm256_and_si256(temp, less_32);
                           temp = _mm256_or_si256(temp, v);
                           __m256i next = _mm256_andnot_si256(temp, _mm256_set1_epi8(-1)); // Equivalent to ~temp
                           next = _mm256_and_si256(next, high_bit_mask);

                           uint32_t mask = _mm256_movemask_epi8(next);

                           if (mask == 0) {
                              data += 32;
                              c += 32;
                              continue;
                           }

                           uint32_t length = countr_zero(mask);

                           c += length;
                           data += length;

                           std::memcpy(data, &char_escape_table[uint8_t(*c)], 2);
                           data += 2;
                           ++c;
                        }
                     }
#endif

                     if (n > 7) {
                        for (const auto end_m7 = e - 7; c < end_m7;) {
                           std::memcpy(data, c, 8);
                           uint64_t swar;
                           std::memcpy(&swar, c, 8);

                           constexpr uint64_t lo7_mask = repeat_byte8(0b01111111);
                           const uint64_t lo7 = swar & lo7_mask;
                           const uint64_t quote = (lo7 ^ repeat_byte8('"')) + lo7_mask;
                           const uint64_t backslash = (lo7 ^ repeat_byte8('\\')) + lo7_mask;
                           const uint64_t less_32 = (swar & repeat_byte8(0b01100000)) + lo7_mask;
                           uint64_t next = ~((quote & backslash & less_32) | swar);

                           next &= repeat_byte8(0b10000000);
                           if (next == 0) {
                              data += 8;
                              c += 8;
                              continue;
                           }

                           const auto length = (countr_zero(next) >> 3);
                           c += length;
                           data += length;

                           std::memcpy(data, &char_escape_table[uint8_t(*c)], 2);
                           data += 2;
                           ++c;
                        }
                     }

                     // Tail end of buffer. Uncommon for long strings.
                     for (; c < e; ++c) {
                        if (const auto escaped = char_escape_table[uint8_t(*c)]; escaped) {
                           std::memcpy(data, &escaped, 2);
                           data += 2;
                        }
                        else {
                           std::memcpy(data, c, 1);
                           ++data;
                        }
                     }

                     ix += size_t(data - start);

                     dump<'"', false>(b, ix);
                  }
               }
            }
         }
      };

      template <filesystem_path T>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            to<JSON, decltype(value.string())>::template op<Opts>(value.string(), ctx, args...);
         }
      };

      template <class T>
         requires((glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<std::decay_t<T>>)) && not custom_write<T>)
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            // TODO: Use new hashing approach for better performance
            // TODO: Check if sequenced and use the value as the index if so
            using key_t = std::underlying_type_t<T>;
            static constexpr auto frozen_map = detail::make_enum_to_string_map<T>();
            const auto& member_it = frozen_map.find(static_cast<key_t>(value));
            if (member_it != frozen_map.end()) {
               const sv str = {member_it->second.data(), member_it->second.size()};
               // TODO: Assumes people dont use strings with chars that need to be escaped for their enum names
               // TODO: Could create a pre quoted map for better performance
               if constexpr (not Opts.raw) {
                  dump<'"'>(args...);
               }
               dump_maybe_empty(str, args...);
               if constexpr (not Opts.raw) {
                  dump<'"'>(args...);
               }
            }
            else [[unlikely]] {
               // What do we want to happen if the value doesnt have a mapped string
               write<JSON>::op<Opts>(static_cast<std::underlying_type_t<T>>(value), ctx, std::forward<Args>(args)...);
            }
         }
      };

      template <class T>
         requires(!meta_keys<T> && std::is_enum_v<std::decay_t<T>> && !glaze_enum_t<T> && !custom_write<T>)
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            // serialize as underlying number
            write<JSON>::op<Opts>(static_cast<std::underlying_type_t<std::decay_t<T>>>(value), ctx,
                                  std::forward<Args>(args)...);
         }
      };

      template <func_t T>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args)
         {
            dump<'"'>(args...);
            dump_maybe_empty(name_v<std::decay_t<decltype(value)>>, args...);
            dump<'"'>(args...);
         }
      };

      template <class T>
      struct to<JSON, basic_raw_json<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix)
         {
            dump_maybe_empty(value.str, b, ix);
         }
      };

      template <class T>
      struct to<JSON, basic_text<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix)
         {
            dump_maybe_empty(value.str, b, ix);
         }
      };

      template <opts Opts, bool minified_check = true, class B>
      GLZ_ALWAYS_INLINE void write_array_entry_separator(is_context auto&& ctx, B&& b, auto&& ix)
      {
         if constexpr (Opts.prettify) {
            if constexpr (vector_like<B>) {
               if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size()) [[unlikely]] {
                  b.resize((std::max)(b.size() * 2, k));
               }
            }
            if constexpr (Opts.new_lines_in_arrays) {
               dump<",\n", false>(b, ix);
               dumpn_unchecked<Opts.indentation_char>(ctx.indentation_level, b, ix);
            }
            else {
               dump<", ", false>(b, ix);
            }
         }
         else {
            if constexpr (vector_like<B>) {
               if constexpr (minified_check) {
                  if (ix == b.size()) [[unlikely]] {
                     b.resize((std::max)(b.size() * 2, size_t(128)));
                  }
               }
            }
            std::memcpy(&b[ix], ",", 1);
            ++ix;
         }
      }

      template <opts Opts, bool minified_check = true, class B>
      GLZ_ALWAYS_INLINE void write_object_entry_separator(is_context auto&& ctx, B&& b, auto&& ix)
      {
         if constexpr (Opts.prettify) {
            if constexpr (vector_like<B>) {
               if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size()) [[unlikely]] {
                  b.resize((std::max)(b.size() * 2, k));
               }
            }
            dump<",\n", false>(b, ix);
            dumpn_unchecked<Opts.indentation_char>(ctx.indentation_level, b, ix);
         }
         else {
            if constexpr (vector_like<B>) {
               if constexpr (minified_check) {
                  if (ix == b.size()) [[unlikely]] {
                     b.resize((std::max)(b.size() * 2, size_t(128)));
                  }
               }
            }
            std::memcpy(&b[ix], ",", 1);
            ++ix;
         }
      }

      template <opts Opts, class Key, class Value, is_context Ctx>
      GLZ_ALWAYS_INLINE void write_pair_content(const Key& key, Value&& value, Ctx& ctx, auto&&... args)
      {
         if constexpr (str_t<Key> || char_t<Key> || glaze_enum_t<Key> || Opts.quoted_num) {
            write<JSON>::op<Opts>(key, ctx, args...);
         }
         else {
            write<JSON>::op<opt_false<Opts, &opts::raw_string>>(quoted_t<const Key>{key}, ctx, args...);
         }
         if constexpr (Opts.prettify) {
            dump<": ">(args...);
         }
         else {
            dump<':'>(args...);
         }

         write<JSON>::op<opening_and_closing_handled_off<Opts>()>(std::forward<Value>(value), ctx, args...);
      }

      template <class T>
         requires(writable_array_t<T> || writable_map_t<T>)
      struct to<JSON, T>
      {
         static constexpr bool map_like_array = writable_array_t<T> && pair_t<range_value_t<T>>;

         template <auto Opts, class B>
            requires(writable_array_t<T> && (map_like_array ? Opts.concatenate == false : true))
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix)
         {
            if (empty_range(value)) {
               dump<"[]">(b, ix);
            }
            else {
               if constexpr (Opts.prettify) {
                  if constexpr (Opts.new_lines_in_arrays) {
                     ctx.indentation_level += Opts.indentation_width;
                  }

                  if constexpr (vector_like<B>) {
                     if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, k));
                     }
                  }

                  if constexpr (Opts.new_lines_in_arrays) {
                     std::memcpy(&b[ix], "[\n", 2);
                     ix += 2;
                     std::memset(&b[ix], Opts.indentation_char, ctx.indentation_level);
                     ix += ctx.indentation_level;
                  }
                  else {
                     std::memcpy(&b[ix], "[", 1);
                     ++ix;
                  }
               }
               else {
                  if constexpr (vector_like<B>) {
                     if (const auto k = ix + write_padding_bytes; k > b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, k));
                     }
                  }
                  std::memcpy(&b[ix], "[", 1);
                  ++ix;
               }

               auto it = std::begin(value);
               using val_t = std::remove_cvref_t<decltype(*it)>;
               if constexpr (supports_unchecked_write<val_t>) {
                  to<JSON, val_t>::template op<write_unchecked_on<Opts>()>(*it, ctx, b, ix);
               }
               else {
                  to<JSON, val_t>::template op<Opts>(*it, ctx, b, ix);
               }

               ++it;
               for (const auto fin = std::end(value); it != fin; ++it) {
                  if constexpr (supports_unchecked_write<val_t>) {
                     if constexpr (vector_like<B>) {
                        if constexpr (Opts.prettify) {
                           if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size())
                              [[unlikely]] {
                              b.resize((std::max)(b.size() * 2, k));
                           }
                        }
                        else {
                           if (const auto k = ix + write_padding_bytes; k > b.size()) [[unlikely]] {
                              b.resize((std::max)(b.size() * 2, k));
                           }
                        }
                     }

                     if constexpr (Opts.prettify) {
                        if constexpr (Opts.new_lines_in_arrays) {
                           std::memcpy(&b[ix], ",\n", 2);
                           ix += 2;
                           dumpn_unchecked<Opts.indentation_char>(ctx.indentation_level, b, ix);
                        }
                        else {
                           std::memcpy(&b[ix], ", ", 2);
                           ix += 2;
                        }
                     }
                     else {
                        std::memcpy(&b[ix], ",", 1);
                        ++ix;
                     }

                     to<JSON, val_t>::template op<write_unchecked_on<Opts>()>(*it, ctx, b, ix);
                  }
                  else {
                     write_array_entry_separator<Opts>(ctx, b, ix);
                     to<JSON, val_t>::template op<Opts>(*it, ctx, b, ix);
                  }
               }
               if constexpr (Opts.prettify && Opts.new_lines_in_arrays) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, b, ix);
               }

               dump<']'>(b, ix);
            }
         }

         template <auto Opts>
            requires(writable_map_t<T> || (map_like_array && Opts.concatenate == true))
         static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            if constexpr (!has_opening_handled(Opts)) {
               dump<'{'>(args...);
            }

            if (!empty_range(value)) {
               if constexpr (!has_opening_handled(Opts)) {
                  if constexpr (Opts.prettify) {
                     ctx.indentation_level += Opts.indentation_width;
                     dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
                  }
               }

               auto write_first_entry = [&ctx, &args...](auto&& it) {
                  if constexpr (requires {
                                   it->first;
                                   it->second;
                                }) {
                     // Allow non-const access, unlike ranges
                     if (skip_member<Opts>(it->second)) {
                        return true;
                     }
                     write_pair_content<Opts>(it->first, it->second, ctx, args...);
                     return false;
                  }
                  else {
                     const auto& [key, entry_val] = *it;
                     if (skip_member<Opts>(entry_val)) {
                        return true;
                     }
                     write_pair_content<Opts>(key, entry_val, ctx, args...);
                     return false;
                  }
               };

               auto it = std::begin(value);
               [[maybe_unused]] bool starting = write_first_entry(it);
               for (++it; it != std::end(value); ++it) {
                  // I couldn't find an easy way around this code duplication
                  // Ranges need to be decomposed with const auto& [...],
                  // but we don't want to const qualify our maps for the sake of reflection writing
                  // we need to be able to populate the tuple of pointers when writing with reflection
                  if constexpr (requires {
                                   it->first;
                                   it->second;
                                }) {
                     if (skip_member<Opts>(it->second)) {
                        continue;
                     }

                     // When Opts.skip_null_members, *any* entry may be skipped, meaning separator dumping must be
                     // conditional for every entry. Avoid this branch when not skipping null members.
                     // Alternatively, write separator after each entry except last but then branch is permanent
                     if constexpr (Opts.skip_null_members) {
                        if (!starting) {
                           write_object_entry_separator<Opts>(ctx, args...);
                        }
                     }
                     else {
                        write_object_entry_separator<Opts>(ctx, args...);
                     }

                     write_pair_content<Opts>(it->first, it->second, ctx, args...);
                  }
                  else {
                     const auto& [key, entry_val] = *it;
                     if (skip_member<Opts>(entry_val)) {
                        continue;
                     }

                     // When Opts.skip_null_members, *any* entry may be skipped, meaning separator dumping must be
                     // conditional for every entry. Avoid this branch when not skipping null members.
                     // Alternatively, write separator after each entry except last but then branch is permanent
                     if constexpr (Opts.skip_null_members) {
                        if (!starting) {
                           write_object_entry_separator<Opts>(ctx, args...);
                        }
                     }
                     else {
                        write_object_entry_separator<Opts>(ctx, args...);
                     }

                     write_pair_content<Opts>(key, entry_val, ctx, args...);
                  }

                  starting = false;
               }

               if constexpr (!has_closing_handled(Opts)) {
                  if constexpr (Opts.prettify) {
                     ctx.indentation_level -= Opts.indentation_width;
                     dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
                  }
               }
            }

            if constexpr (!has_closing_handled(Opts)) {
               dump<'}'>(args...);
            }
         }
      };

      template <pair_t T>
      struct to<JSON, T>
      {
         template <glz::opts Opts, class B, class Ix>
         static void op(const T& value, is_context auto&& ctx, B&& b, Ix&& ix)
         {
            const auto& [key, val] = value;
            if (skip_member<Opts>(val)) {
               return dump<"{}">(b, ix);
            }

            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               if constexpr (vector_like<B>) {
                  if (const auto k = ix + ctx.indentation_level + 2; k > b.size()) [[unlikely]] {
                     b.resize((std::max)(b.size() * 2, k));
                  }
               }
               dump<"{\n", false>(b, ix);
               dumpn_unchecked<Opts.indentation_char>(ctx.indentation_level, b, ix);
            }
            else {
               dump<'{'>(b, ix);
            }

            write_pair_content<Opts>(key, val, ctx, b, ix);

            if constexpr (Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, b, ix);
               dump<'}', false>(b, ix);
            }
            else {
               dump<'}'>(b, ix);
            }
         }
      };

      template <is_expected T>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            if (value) {
               write<JSON>::op<Opts>(*value, ctx, std::forward<Args>(args)...);
            }
            else {
               write<JSON>::op<Opts>(unexpected_wrapper{&value.error()}, ctx, std::forward<Args>(args)...);
            }
         }
      };

      // for C style arrays
      template <nullable_t T>
         requires(std::is_array_v<T>)
      struct to<JSON, T>
      {
         template <auto Opts, class V, size_t N, class... Args>
         GLZ_ALWAYS_INLINE static void op(const V (&value)[N], is_context auto&& ctx, Args&&... args)
         {
            write<JSON>::op<Opts>(std::span{value, N}, ctx, std::forward<Args>(args)...);
         }
      };

      template <optional_like T>
      struct to<JSON, T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix)
         {
            if (value) {
               if constexpr (
                  requires { requires supports_unchecked_write<typename T::value_type>; } ||
                  requires { requires supports_unchecked_write<typename T::element_type>; }) {
                  write<JSON>::op<Opts>(*value, ctx, b, ix);
               }
               else {
                  write<JSON>::op<write_unchecked_off<Opts>()>(*value, ctx, b, ix);
               }
            }
            else {
               dump<"null", not has_write_unchecked(Opts)>(b, ix);
            }
         }
      };

      template <always_null_t T>
      struct to<JSON, T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, B&& b, auto&& ix)
         {
            if constexpr (not has_write_unchecked(Opts)) {
               if (ix + 4 > b.size()) [[unlikely]] {
                  b.resize(b.size() * 2);
               }
            }
            static constexpr uint32_t null_v = 1819047278;
            std::memcpy(&b[ix], &null_v, 4);
            ix += 4;
         }
      };

      template <is_variant T>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            std::visit(
               [&](auto&& val) {
                  using V = std::decay_t<decltype(val)>;

                  if constexpr (Opts.write_type_info && !tag_v<T>.empty() && glaze_object_t<V>) {
                     constexpr auto num_members = reflect<V>::size;

                     // must first write out type
                     if constexpr (Opts.prettify) {
                        dump<"{\n">(args...);
                        ctx.indentation_level += Opts.indentation_width;
                        dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                        dump<'"'>(args...);
                        dump_maybe_empty(tag_v<T>, args...);
                        dump<"\": \"">(args...);
                        dump_maybe_empty(ids_v<T>[value.index()], args...);
                        if constexpr (num_members == 0) {
                           dump<"\"\n">(args...);
                        }
                        else {
                           dump<"\",\n">(args...);
                        }
                        dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                     }
                     else {
                        dump<"{\"">(args...);
                        dump_maybe_empty(tag_v<T>, args...);
                        dump<"\":\"">(args...);
                        dump_maybe_empty(ids_v<T>[value.index()], args...);
                        if constexpr (num_members == 0) {
                           dump<R"(")">(args...);
                        }
                        else {
                           dump<R"(",)">(args...);
                        }
                     }
                     to<JSON, V>::template op<opening_handled<Opts>()>(val, ctx, args...);
                  }
                  else {
                     to<JSON, V>::template op<Opts>(val, ctx, args...);
                  }
               },
               value);
         }
      };

      template <class T>
      struct to<JSON, array_variant_wrapper<T>>
      {
         template <auto Opts, class... Args>
         static void op(auto&& wrapper, is_context auto&& ctx, Args&&... args)
         {
            auto& value = wrapper.value;
            dump<'['>(args...);
            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<'"'>(args...);
            dump_maybe_empty(ids_v<T>[value.index()], args...);
            dump<"\",">(args...);
            if constexpr (Opts.prettify) {
               dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            std::visit([&](auto&& v) { write<JSON>::op<Opts>(v, ctx, args...); }, value);
            if constexpr (Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<']'>(args...);
         }
      };

      template <class T>
         requires is_specialization_v<T, arr>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = glz::tuple_size_v<V>;

            dump<'['>(args...);
            if constexpr (N > 0 && Opts.prettify) {
               if constexpr (Opts.new_lines_in_arrays) {
                  ctx.indentation_level += Opts.indentation_width;
                  dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
               }
            }
            invoke_table<N>([&]<size_t I>() {
               if constexpr (glaze_array_t<V>) {
                  write<JSON>::op<Opts>(get_member(value.value, glz::get<I>(meta_v<T>)), ctx, args...);
               }
               else {
                  write<JSON>::op<Opts>(glz::get<I>(value.value), ctx, args...);
               }
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  write_array_entry_separator<Opts>(ctx, args...);
               }
            });
            if constexpr (N > 0 && Opts.prettify) {
               if constexpr (Opts.new_lines_in_arrays) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
               }
            }
            dump<']'>(args...);
         }
      };

      template <class T>
         requires glaze_array_t<T> || tuple_t<std::decay_t<T>> || is_std_tuple<T>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            static constexpr auto N = []() constexpr {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return glz::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return glz::tuple_size_v<std::decay_t<T>>;
               }
            }();

            dump<'['>(args...);
            if constexpr (N > 0 && Opts.prettify) {
               if constexpr (Opts.new_lines_in_arrays) {
                  ctx.indentation_level += Opts.indentation_width;
                  dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
               }
            }
            using V = std::decay_t<T>;
            invoke_table<N>([&]<size_t I>() {
               if constexpr (glaze_array_t<V>) {
                  write<JSON>::op<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, args...);
               }
               else if constexpr (is_std_tuple<T>) {
                  write<JSON>::op<Opts>(std::get<I>(value), ctx, args...);
               }
               else {
                  write<JSON>::op<Opts>(glz::get<I>(value), ctx, args...);
               }
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  write_array_entry_separator<Opts>(ctx, args...);
               }
            });
            if constexpr (N > 0 && Opts.prettify) {
               if constexpr (Opts.new_lines_in_arrays) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump_newline_indent<Opts.indentation_char>(ctx.indentation_level, args...);
               }
            }
            dump<']'>(args...);
         }
      };

      template <is_includer T>
      struct to<JSON, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&... args)
         {
            dump<R"("")">(args...); // dump an empty string
         }
      };

      template <const std::string_view& S>
      GLZ_ALWAYS_INLINE constexpr auto array_from_sv() noexcept
      {
         constexpr auto N = S.size();
         std::array<char, N> arr;
         std::copy_n(S.data(), N, arr.data());
         return arr;
      }

      template <class T>
         requires is_specialization_v<T, glz::obj> || is_specialization_v<T, glz::obj_copy>
      struct to<JSON, T>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix)
         {
            if constexpr (!has_opening_handled(Options)) {
               dump<'{'>(b, ix);
               if constexpr (Options.prettify) {
                  ctx.indentation_level += Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
            }

            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = glz::tuple_size_v<V> / 2;

            bool first = true;
            invoke_table<N>([&]<size_t I>() {
               constexpr auto Opts = opening_and_closing_handled_off<ws_handled_off<Options>()>();
               decltype(auto) item = glz::get<2 * I + 1>(value.value);
               using val_t = std::decay_t<decltype(item)>;

               if (skip_member<Opts>(item)) {
                  return;
               }

               // skip
               if constexpr (is_includer<val_t> || std::is_same_v<val_t, hidden> || std::same_as<val_t, skip>) {
                  return;
               }
               else {
                  if (first) {
                     first = false;
                  }
                  else {
                     // Null members may be skipped so we cant just write it out for all but the last member unless
                     // trailing commas are allowed
                     write_object_entry_separator<Opts>(ctx, b, ix);
                  }

                  using Key = typename std::decay_t<glz::tuple_element_t<2 * I, V>>;

                  if constexpr (str_t<Key> || char_t<Key>) {
                     const sv key = glz::get<2 * I>(value.value);
                     to<JSON, decltype(key)>::template op<Opts>(key, ctx, b, ix);
                     dump<':'>(b, ix);
                     if constexpr (Opts.prettify) {
                        dump<' '>(b, ix);
                     }
                  }
                  else {
                     dump<'"'>(b, ix);
                     to<JSON, val_t>::template op<Opts>(item, ctx, b, ix);
                     dump_not_empty(Opts.prettify ? "\": " : "\":", b, ix);
                  }

                  to<JSON, val_t>::template op<Opts>(item, ctx, b, ix);
               }
            });

            if constexpr (!has_closing_handled(Options)) {
               if constexpr (Options.prettify) {
                  ctx.indentation_level -= Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
               dump<'}'>(b, ix);
            }
         }
      };

      template <class T>
         requires is_specialization_v<T, glz::merge>
      struct to<JSON, T>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix)
         {
            if constexpr (!has_opening_handled(Options)) {
               dump<'{'>(b, ix);
               if constexpr (Options.prettify) {
                  ctx.indentation_level += Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
            }

            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = glz::tuple_size_v<V>;

            invoke_table<N>([&]<size_t I>() {
               if constexpr (Options.skip_null_members) {
                  // It is possible that all fields were skipped if skip_null_members is true
                  // In this case we don't want to dump a comma
                  const auto ix_start = ix;
                  write<JSON>::op<opening_and_closing_handled<Options>()>(glz::get<I>(value.value), ctx, b, ix);
                  if constexpr (I < N - 1) {
                     if (ix > ix_start) // we wrote something
                     {
                        dump<','>(b, ix);
                     }
                  }
               }
               else {
                  write<JSON>::op<opening_and_closing_handled<Options>()>(glz::get<I>(value.value), ctx, b, ix);
                  if constexpr (I < N - 1) {
                     dump<','>(b, ix);
                  }
               }
            });

            if constexpr (Options.prettify) {
               ctx.indentation_level -= Options.indentation_width;
               dump<'\n'>(b, ix);
               dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
            }
            dump<'}'>(b, ix);
         }
      };

      template <class T>
      inline constexpr size_t maximum_key_size = [] {
         constexpr auto N = reflect<T>::size;
         size_t maximum{};
         for (size_t i = 0; i < N; ++i) {
            if (reflect<T>::keys[i].size() > maximum) {
               maximum = reflect<T>::keys[i].size();
            }
         }
         return maximum + 2; // add quotes
      }();

      inline constexpr uint64_t round_up_to_nearest_16(const uint64_t value) noexcept { return (value + 15) & ~15ull; }

      // Only use this if you are not prettifying
      template <class T>
      inline constexpr std::optional<size_t> fixed_padding = [] {
         constexpr auto N = reflect<T>::size;
         std::optional<size_t> fixed = 2 + 16; // {} + extra padding
         for_each_short_circuit<N>([&](auto I) -> bool {
            using val_t = std::remove_cvref_t<refl_t<T, I>>;
            if constexpr (supports_unchecked_write<val_t> && required_padding<val_t>().has_value()) {
               fixed.value() += required_padding<val_t>().value();
               fixed.value() += reflect<T>::keys[I].size() + 2; // key length
               fixed.value() += 2; // colon and comma
               return false; // continue
            }
            else {
               fixed = {};
               return true; // break
            }
         });
         if (fixed) {
            fixed = round_up_to_nearest_16(fixed.value());
         }
         return fixed;
      }();

      template <class T>
         requires glaze_object_t<T> || reflectable<T>
      struct to<JSON, T>
      {
         template <auto Options, class V, class B>
            requires(not std::is_pointer_v<std::remove_cvref_t<V>>)
         static void op(V&& value, is_context auto&& ctx, B&& b, auto&& ix)
         {
            using ValueType = std::decay_t<V>;
            if constexpr (detail::has_unknown_writer<ValueType> && not has_disable_write_unknown(Options)) {
               constexpr auto& writer = meta_unknown_write_v<ValueType>;

               using WriterType = meta_unknown_write_t<ValueType>;
               if constexpr (std::is_member_object_pointer_v<WriterType>) {
                  decltype(auto) unknown_writer = value.*writer;
                  if (unknown_writer.size() > 0) {
                     // TODO: This intermediate is added to get GCC 14 to build
                     decltype(auto) merged = glz::merge{value, unknown_writer};
                     write<JSON>::op<disable_write_unknown_on<Options>()>(std::move(merged), ctx, b, ix);
                  }
                  else {
                     write<JSON>::op<disable_write_unknown_on<Options>()>(value, ctx, b, ix);
                  }
               }
               else if constexpr (std::is_member_function_pointer_v<WriterType>) {
                  decltype(auto) unknown_writer = (value.*writer)();
                  if (unknown_writer.size() > 0) {
                     // TODO: This intermediate is added to get GCC 14 to build
                     decltype(auto) merged = glz::merge{value, unknown_writer};
                     write<JSON>::op<disable_write_unknown_on<Options>()>(std::move(merged), ctx, b, ix);
                  }
                  else {
                     write<JSON>::op<disable_write_unknown_on<Options>()>(value, ctx, b, ix);
                  }
               }
               else {
                  static_assert(false_v<T>, "unknown_write type not handled");
               }
            }
            else {
               // handles glaze_object_t without extra unknown fields
               static constexpr auto Opts =
                  disable_write_unknown_off<opening_and_closing_handled_off<ws_handled_off<Options>()>()>();

               if constexpr (!has_opening_handled(Options)) {
                  if constexpr (Options.prettify) {
                     ctx.indentation_level += Options.indentation_width;
                     if constexpr (vector_like<B>) {
                        if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size())
                           [[unlikely]] {
                           b.resize((std::max)(b.size() * 2, k));
                        }
                     }
                     std::memcpy(&b[ix], "{\n", 2);
                     ix += 2;
                     dumpn_unchecked<Options.indentation_char>(ctx.indentation_level, b, ix);
                  }
                  else {
                     dump<'{'>(b, ix);
                  }
               }

               static constexpr auto N = reflect<T>::size;

               decltype(auto) t = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return to_tuple(value);
                  }
                  else {
                     return nullptr;
                  }
               }();

               static constexpr auto padding = round_up_to_nearest_16(maximum_key_size<T> + write_padding_bytes);
               if constexpr (object_info<Opts, T>::maybe_skipped) {
                  bool first = true;
                  static constexpr auto first_is_written = object_info<Opts, T>::first_will_be_written;
                  invoke_table<N>([&]<size_t I>() {
                     using val_t = std::remove_cvref_t<refl_t<T, I>>;

                     if constexpr (is_includer<val_t> || std::same_as<val_t, hidden> || std::same_as<val_t, skip>) {
                        return;
                     }
                     else {
                        if constexpr (null_t<val_t>) {
                           if constexpr (always_null_t<T>)
                              return;
                           else {
                              const auto is_null = [&]() {
                                 decltype(auto) element = [&]() -> decltype(auto) {
                                    if constexpr (reflectable<T>) {
                                       return get<I>(t);
                                    }
                                    else {
                                       return get<I>(reflect<T>::values);
                                    }
                                 };

                                 if constexpr (nullable_wrapper<val_t>) {
                                    return !bool(element()(value).val);
                                 }
                                 else {
                                    return !bool(get_member(value, element()));
                                 }
                              }();
                              if (is_null) return;
                           }
                        }

                        maybe_pad<padding>(b, ix);

                        if constexpr (first_is_written && I > 0) {
                           // write_object_entry_separator<Opts, not supports_unchecked_write<val_t>>(ctx, b, ix);
                           if constexpr (Opts.prettify) {
                              if constexpr (vector_like<B>) {
                                 if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size())
                                    [[unlikely]] {
                                    b.resize((std::max)(b.size() * 2, k));
                                 }
                              }
                              std::memcpy(&b[ix], ",\n", 2);
                              ix += 2;
                              dumpn_unchecked<Opts.indentation_char>(ctx.indentation_level, b, ix);
                           }
                           else {
                              if constexpr (vector_like<B>) {
                                 static_assert(vector_like<B>);
                                 if constexpr (not supports_unchecked_write<val_t>) {
                                    if (ix == b.size()) [[unlikely]] {
                                       b.resize(b.size() * 2);
                                    }
                                 }
                              }
                              std::memcpy(&b[ix], ",", 1);
                              ++ix;
                           }
                        }
                        else {
                           if (first) {
                              first = false;
                           }
                           else {
                              // Null members may be skipped so we cant just write it out for all but the last member
                              // write_object_entry_separator<Opts, not supports_unchecked_write<val_t>>(ctx, b, ix);
                              if constexpr (Opts.prettify) {
                                 if constexpr (vector_like<B>) {
                                    if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size())
                                       [[unlikely]] {
                                       b.resize((std::max)(b.size() * 2, k));
                                    }
                                 }
                                 std::memcpy(&b[ix], ",\n", 2);
                                 ix += 2;
                                 dumpn_unchecked<Opts.indentation_char>(ctx.indentation_level, b, ix);
                              }
                              else {
                                 if constexpr (vector_like<B>) {
                                    static_assert(vector_like<B>);
                                    if constexpr (not supports_unchecked_write<val_t>) {
                                       if (ix == b.size()) [[unlikely]] {
                                          b.resize(b.size() * 2);
                                       }
                                    }
                                 }
                                 std::memcpy(&b[ix], ",", 1);
                                 ++ix;
                              }
                           }
                        }

                        // MSVC requires get<I> rather than keys[I]
                        static constexpr auto key = glz::get<I>(reflect<T>::keys); // GCC 14 requires auto here
                        static constexpr auto quoted_key = join_v < chars<"\"">, key,
                                              Opts.prettify ? chars<"\": "> : chars < "\":" >>
                           ;

                        static constexpr auto n = quoted_key.size();
                        std::memcpy(&b[ix], quoted_key.data(), n);
                        ix += n;

                        static constexpr auto check_opts =
                           supports_unchecked_write<val_t> ? write_unchecked_on<Opts>() : Opts;
                        if constexpr (reflectable<T>) {
                           to<JSON, val_t>::template op<check_opts>(get_member(value, get<I>(t)), ctx, b, ix);
                        }
                        else {
                           to<JSON, val_t>::template op<check_opts>(get_member(value, get<I>(reflect<T>::values)), ctx,
                                                                    b, ix);
                        }
                     }
                  });
               }
               else {
                  static constexpr std::optional<size_t> fixed_max_size = fixed_padding<T>;
                  if constexpr (fixed_max_size) {
                     maybe_pad<fixed_max_size.value()>(b, ix);
                  }

                  invoke_table<N>([&]<size_t I>() {
                     if constexpr (not fixed_max_size) {
                        maybe_pad<padding>(b, ix);
                     }

                     // MSVC requires get<I> rather than keys[I]
                     static constexpr auto key = glz::get<I>(reflect<T>::keys); // GCC 14 requires auto here
                     static constexpr auto quoted_key = join_v < chars<"\"">, key,
                                           Opts.prettify ? chars<"\": "> : chars < "\":" >>
                        ;

                     static constexpr auto n = quoted_key.size();
                     std::memcpy(&b[ix], quoted_key.data(), n);
                     ix += n;

                     using val_t = std::remove_cvref_t<refl_t<T, I>>;

                     static constexpr auto check_opts =
                        supports_unchecked_write<val_t> ? write_unchecked_on<Opts>() : Opts;
                     if constexpr (reflectable<T>) {
                        to<JSON, val_t>::template op<check_opts>(get_member(value, get<I>(t)), ctx, b, ix);
                     }
                     else {
                        to<JSON, val_t>::template op<check_opts>(get_member(value, get<I>(reflect<T>::values)), ctx, b,
                                                                 ix);
                     }

                     if constexpr (I != (N - 1)) {
                        if constexpr (Opts.prettify) {
                           if constexpr (vector_like<B>) {
                              if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size())
                                 [[unlikely]] {
                                 b.resize((std::max)(b.size() * 2, k));
                              }
                           }
                           std::memcpy(&b[ix], ",\n", 2);
                           ix += 2;
                           dumpn_unchecked<Opts.indentation_char>(ctx.indentation_level, b, ix);
                        }
                        else {
                           if constexpr (vector_like<B>) {
                              static_assert(vector_like<B>);
                              if constexpr (not supports_unchecked_write<val_t>) {
                                 if (ix == b.size()) [[unlikely]] {
                                    b.resize(b.size() * 2);
                                 }
                              }
                           }
                           std::memcpy(&b[ix], ",", 1);
                           ++ix;
                        }
                     }
                  });
               }

               // Options is required here, because it must be the top level
               if constexpr (!has_closing_handled(Options)) {
                  if constexpr (Options.prettify) {
                     ctx.indentation_level -= Options.indentation_width;
                     if constexpr (vector_like<B>) {
                        if (const auto k = ix + ctx.indentation_level + write_padding_bytes; k > b.size())
                           [[unlikely]] {
                           b.resize((std::max)(b.size() * 2, k));
                        }
                     }
                     std::memcpy(&b[ix], "\n", 1);
                     ++ix;
                     dumpn_unchecked<Options.indentation_char>(ctx.indentation_level, b, ix);
                     std::memcpy(&b[ix], "}", 1);
                     ++ix;
                  }
                  else {
                     dump<'}'>(b, ix);
                  }
               }
            }
         }
      };

      template <auto& Partial, auto Opts, class T, class Ctx, class B, class IX>
      concept write_json_partial_invocable = requires(T&& value, Ctx&& ctx, B&& b, IX&& ix) {
         to_partial<JSON, std::remove_cvref_t<T>>::template op<Partial, Opts>(
            std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
      };

      template <>
      struct write_partial<JSON>
      {
         template <auto& Partial, auto Opts, class T, is_context Ctx, class B, class IX>
         static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            if constexpr (std::count(Partial.begin(), Partial.end(), "") > 0) {
               detail::write<JSON>::op<Opts>(value, ctx, b, ix);
            }
            else if constexpr (write_json_partial_invocable<Partial, Opts, T, Ctx, B, IX>) {
               to_partial<JSON, std::remove_cvref_t<T>>::template op<Partial, Opts>(
                  std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
            }
            else {
               static_assert(false_v<T>, "Glaze metadata is probably needed for your type");
            }
         }
      };

      template <class T>
      consteval size_t key_index(const std::string_view key)
      {
         const auto n = reflect<T>::keys.size();
         for (size_t i = 0; i < n; ++i) {
            if (key == reflect<T>::keys[i]) {
               return i;
            }
         }
         return n;
      }

      // Only object types are supported for partial
      template <class T>
         requires(glaze_object_t<T> || writable_map_t<T> || reflectable<T>)
      struct to_partial<JSON, T> final
      {
         template <auto& Partial, auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix)
         {
            if constexpr (!has_opening_handled(Opts)) {
               dump<'{'>(b, ix);
               if constexpr (Opts.prettify) {
                  ctx.indentation_level += Opts.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, b, ix);
               }
            }

            static constexpr auto sorted = sort_json_ptrs(Partial);
            static constexpr auto groups = glz::group_json_ptrs<sorted>();
            static constexpr auto N = glz::tuple_size_v<std::decay_t<decltype(groups)>>;

            static constexpr auto num_members = reflect<T>::size;

            if constexpr ((num_members > 0) && (glaze_object_t<T> || reflectable<T>)) {
               invoke_table<N>([&]<size_t I>() {
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  static constexpr auto group = glz::get<I>(groups);

                  static constexpr auto key = get<0>(group);
                  static constexpr auto quoted_key = join_v < chars<"\"">, key,
                                        Opts.prettify ? chars<"\": "> : chars < "\":" >>
                     ;
                  dump<quoted_key>(b, ix);

                  static constexpr auto sub_partial = get<1>(group);
                  static constexpr auto index = key_index<T>(key);
                  static_assert(index < num_members, "Invalid key passed to partial write");
                  if constexpr (glaze_object_t<T>) {
                     static constexpr auto member = get<index>(reflect<T>::values);

                     write_partial<JSON>::op<sub_partial, Opts>(get_member(value, member), ctx, b, ix);
                     if constexpr (I != N - 1) {
                        write_object_entry_separator<Opts>(ctx, b, ix);
                     }
                  }
                  else {
                     write_partial<JSON>::op<sub_partial, Opts>(get_member(value, get<index>(to_tuple(value))), ctx, b,
                                                                ix);
                     if constexpr (I != N - 1) {
                        write_object_entry_separator<Opts>(ctx, b, ix);
                     }
                  }
               });
            }
            else if constexpr (writable_map_t<T>) {
               invoke_table<N>([&]<size_t I>() {
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  static constexpr auto group = glz::get<I>(groups);

                  static constexpr auto key = std::get<0>(group);
                  static constexpr auto quoted_key = join_v < chars<"\"">, key,
                                        Opts.prettify ? chars<"\": "> : chars < "\":" >>
                     ;
                  dump<key>(b, ix);

                  static constexpr auto sub_partial = std::get<1>(group);
                  if constexpr (findable<std::decay_t<T>, decltype(key)>) {
                     auto it = value.find(key);
                     if (it != value.end()) {
                        write_partial<JSON>::op<sub_partial, Opts>(it->second, ctx, b, ix);
                     }
                     else {
                        ctx.error = error_code::invalid_partial_key;
                        return;
                     }
                  }
                  else {
                     static thread_local auto k = typename std::decay_t<T>::key_type(key);
                     auto it = value.find(k);
                     if (it != value.end()) {
                        write_partial<JSON>::op<sub_partial, Opts>(it->second, ctx, b, ix);
                     }
                     else {
                        ctx.error = error_code::invalid_partial_key;
                        return;
                     }
                  }
                  if constexpr (I != N - 1) {
                     write_object_entry_separator<Opts>(ctx, b, ix);
                  }
               });
            }

            if (not bool(ctx.error)) [[likely]] {
               dump<'}'>(b, ix);
            }
         }
      };
   } // namespace detail

   template <write_json_supported T, output_buffer Buffer>
   [[nodiscard]] error_ctx write_json(T&& value, Buffer&& buffer)
   {
      return write<opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_json_supported T, raw_buffer Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> write_json(T&& value, Buffer&& buffer)
   {
      return write<opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_json_supported T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_json(T&& value)
   {
      return write<opts{}>(std::forward<T>(value));
   }

   template <auto& Partial, write_json_supported T, output_buffer Buffer>
   [[nodiscard]] error_ctx write_json(T&& value, Buffer&& buffer)
   {
      return write<Partial, opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <auto& Partial, write_json_supported T, raw_buffer Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> write_json(T&& value, Buffer&& buffer)
   {
      return write<Partial, opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_json_supported T, class Buffer>
   [[nodiscard]] error_ctx write_jsonc(T&& value, Buffer&& buffer)
   {
      return write<opts{.comments = true}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_json_supported T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_jsonc(T&& value)
   {
      return write<opts{.comments = true}>(std::forward<T>(value));
   }

   template <opts Opts = opts{}, write_json_supported T>
   [[nodiscard]] error_ctx write_file_json(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = write<set_json<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec;
      }
      return {buffer_to_file(buffer, file_name)};
   }
}
