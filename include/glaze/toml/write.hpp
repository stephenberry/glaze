// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
#include "glaze/core/wrappers.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/itoa.hpp"

namespace glz
{
   namespace detail
   {
      template <>
      struct write<TOML>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            to<TOML, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <class T>
         requires(glaze_value_t<T> && !custom_write<T>)
      struct to<TOML, T>
      {
         template <auto Opts, class Value, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            using V = std::remove_cvref_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
            to<TOML, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                           std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <>
      struct to<TOML, hidden>
      {
         template <auto Opts>
         static void op(auto&& value, auto&&...) noexcept
         {
            static_assert(false_v<decltype(value)>, "hidden type should not be written");
         }
      };

      template <>
      struct to<TOML, skip>
      {
         template <auto Opts>
         static void op(auto&& value, auto&&...) noexcept
         {
            static_assert(false_v<decltype(value)>, "skip type should not be written");
         }
      };

      template <boolean_like T>
      struct to<TOML, T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&&, B&& b, auto&& ix)
         {
            static constexpr auto checked = not has_write_unchecked(Opts);
            if constexpr (checked && vector_like<B>) {
               if (const auto n = ix + 8; n > b.size()) [[unlikely]] {
                  b.resize(2 * n);
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
               if (value) {
                  std::memcpy(&b[ix], "true", 4);
                  ix += 4;
               }
               else {
                  std::memcpy(&b[ix], "false", 5);
                  ix += 5;
               }
            }
         }
      };

      template <num_t T>
      struct to<TOML, T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix)
         {
            if constexpr (Opts.quoted_num) {
               std::memcpy(&b[ix], "\"", 1);
               ++ix;
               write_chars::op<Opts>(value, ctx, b, ix);
               std::memcpy(&b[ix], "\"", 1);
               ++ix;
            }
            else {
               write_chars::op<Opts>(value, ctx, b, ix);
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
      struct to<TOML, T>
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
                     const auto k = ix + 8; // 4 characters is enough for quotes and escaped character
                     if (k > b.size()) [[unlikely]] {
                        b.resize(2 * k);
                     }
                  }

                  std::memcpy(&b[ix], "\"", 1);
                  ++ix;
                  if (const auto escaped = char_escape_table[uint8_t(value)]; escaped) {
                     std::memcpy(&b[ix], &escaped, 2);
                     ix += 2;
                  }
                  else if (value == '\0') {
                     // null character treated as empty string
                  }
                  else {
                     std::memcpy(&b[ix], &value, 1);
                     ++ix;
                  }
                  std::memcpy(&b[ix], "\"", 1);
                  ++ix;
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
                  // Use +8 for extra buffer
                  if constexpr (resizable<B>) {
                     const auto n = str.size();
                     const auto k = ix + 8 + n;
                     if (k > b.size()) [[unlikely]] {
                        b.resize(2 * k);
                     }
                  }
                  // now we don't have to check writing

                  std::memcpy(&b[ix], "\"", 1);
                  ++ix;
                  if (str.size()) [[likely]] {
                     const auto n = str.size();
                     std::memcpy(&b[ix], str.data(), n);
                     ix += n;
                  }
                  std::memcpy(&b[ix], "\"", 1);
                  ++ix;
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
                     if (k > b.size()) [[unlikely]] {
                        b.resize(2 * k);
                     }
                  }
                  // now we don't have to check writing

                  if constexpr (Opts.raw) {
                     const auto n = str.size();
                     if (n) {
                        std::memcpy(&b[ix], str.data(), n);
                        ix += n;
                     }
                  }
                  else {
                     std::memcpy(&b[ix], "\"", 1);
                     ++ix;

                     const auto* c = str.data();
                     const auto* const e = c + n;
                     const auto start = &b[ix];
                     auto data = start;

                     // We don't check for writing out invalid characters as this can be tested by the user if
                     // necessary. In the case of invalid TOML characters we write out null characters to
                     // showcase the error and make the TOML invalid. These would then be detected upon reading
                     // the TOML.

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

                     std::memcpy(&b[ix], "\"", 1);
                     ++ix;
                  }
               }
            }
         }
      };

      template <filesystem_path T>
      struct to<TOML, T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args)
         {
            to<TOML, decltype(value.string())>::template op<Opts>(value.string(), ctx, std::forward<Args>(args)...);
         }
      };

      template <opts Opts, bool minified_check = true, class B>
         requires (Opts.format == TOML)
      GLZ_ALWAYS_INLINE void write_array_entry_separator(is_context auto&&, B&& b, auto&& ix)
      {
         if constexpr (vector_like<B>) {
            if constexpr (minified_check) {
               if (ix >= b.size()) [[unlikely]] {
                  b.resize(2 * ix);
               }
            }
         }
         std::memcpy(&b[ix], ",", 1);
         ++ix;
      }

      template <opts Opts, bool minified_check = true, class B>
         requires (Opts.format == TOML)
      GLZ_ALWAYS_INLINE void write_object_entry_separator(is_context auto&&, B&& b, auto&& ix)
      {
         if constexpr (vector_like<B>) {
            if constexpr (minified_check) {
               if (ix >= b.size()) [[unlikely]] {
                  b.resize(2 * ix);
               }
            }
         }
         std::memcpy(&b[ix], ",", 1);
         ++ix;
      }

      // "key":value pair output
      template <opts Opts, class Key, class Value, is_context Ctx, class B>
         requires (Opts.format == TOML)
      GLZ_ALWAYS_INLINE void write_pair_content(const Key& key, Value&& value, Ctx& ctx, B&& b, auto&& ix)
      {
         if constexpr (str_t<Key> || char_t<Key> || glaze_enum_t<Key> || Opts.quoted_num) {
            to<TOML, core_t<Key>>::template op<Opts>(key, ctx, b, ix);
         }
         else if constexpr (num_t<Key>) {
            write<TOML>::op<opt_true<Opts, &opts::quoted_num>>(key, ctx, b, ix);
         }
         else {
            write<TOML>::op<opt_false<Opts, &opts::raw_string>>(quoted_t<const Key>{key}, ctx, b, ix);
         }
         dump<':'>(b, ix);

         using V = core_t<decltype(value)>;
         to<TOML, V>::template op<opening_and_closing_handled_off<Opts>()>(value, ctx, b, ix);
      }

      template <class T>
         requires(writable_array_t<T> || writable_map_t<T>)
      struct to<TOML, T>
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
               if constexpr (has_size<T>) {
                  const auto n = value.size();

                  if constexpr (vector_like<B>) {
                     static constexpr auto comma_padding = 1;
                     if (const auto k = ix + n * comma_padding + write_padding_bytes; k > b.size())
                        [[unlikely]] {
                        b.resize(2 * k);
                     }
                  }
                  std::memcpy(&b[ix], "[", 1);
                  ++ix;

                  auto it = std::begin(value);
                  using val_t = std::remove_cvref_t<decltype(*it)>;
                  to<TOML, val_t>::template op<Opts>(*it, ctx, b, ix);

                  ++it;
                  for (const auto fin = std::end(value); it != fin; ++it) {
                     std::memcpy(&b[ix], ",", 1);
                     ++ix;

                     to<TOML, val_t>::template op<Opts>(*it, ctx, b, ix);
                  }

                  std::memcpy(&b[ix], "]", 1);
                  ++ix;
               }
               else {
                  // we either can't get the size (std::forward_list) or we cannot compute the allocation size

                  if constexpr (vector_like<B>) {
                     if (const auto k = ix + write_padding_bytes; k > b.size()) [[unlikely]] {
                        b.resize(2 * k);
                     }
                  }
                  std::memcpy(&b[ix], "[", 1);
                  ++ix;

                  auto it = std::begin(value);
                  using val_t = std::remove_cvref_t<decltype(*it)>;
                  to<TOML, val_t>::template op<Opts>(*it, ctx, b, ix);

                  ++it;
                  for (const auto fin = std::end(value); it != fin; ++it) {
                     write_array_entry_separator<Opts>(ctx, b, ix);
                     to<TOML, val_t>::template op<Opts>(*it, ctx, b, ix);
                  }

                  dump<']'>(b, ix);
               }
            }
         }

         template <auto Opts, class B>
            requires(writable_map_t<T> || (map_like_array && Opts.concatenate == true))
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix)
         {
            if constexpr (not has_opening_handled(Opts)) {
               dump<'{'>(b, ix);
            }

            if (!empty_range(value)) {
               using val_t = iterator_second_type<T>; // the type of value in each [key, value] pair

               if constexpr (not always_skipped<val_t>) {
                  if constexpr (null_t<val_t>) {
                     auto write_first_entry = [&](auto&& it) {
                        auto&& [key, entry_val] = *it;
                        if (skip_member<Opts>(entry_val)) {
                           return true;
                        }
                        write_pair_content<Opts>(key, entry_val, ctx, b, ix);
                        return false;
                     };

                     auto it = std::begin(value);
                     bool first = write_first_entry(it);
                     ++it;
                     for (const auto end = std::end(value); it != end; ++it) {
                        auto&& [key, entry_val] = *it;
                        if (skip_member<Opts>(entry_val)) {
                           continue;
                        }

                        // *any* entry may be skipped, meaning separator dumping must be
                        // conditional for every entry.
                        // Alternatively, write separator after each entry except last but then branch is permanent
                        if (not first) {
                           write_object_entry_separator<Opts>(ctx, b, ix);
                        }

                        write_pair_content<Opts>(key, entry_val, ctx, b, ix);

                        first = false;
                     }
                  }
                  else {
                     auto write_first_entry = [&](auto&& it) {
                        auto&& [key, entry_val] = *it;
                        write_pair_content<Opts>(key, entry_val, ctx, b, ix);
                     };

                     auto it = std::begin(value);
                     write_first_entry(it);
                     ++it;
                     for (const auto end = std::end(value); it != end; ++it) {
                        auto&& [key, entry_val] = *it;
                        write_object_entry_separator<Opts>(ctx, b, ix);
                        write_pair_content<Opts>(key, entry_val, ctx, b, ix);
                     }
                  }
               }
            }

            if constexpr (!has_closing_handled(Opts)) {
               dump<'}'>(b, ix);
            }
         }
      };

      // for C style arrays
      template <nullable_t T>
         requires(std::is_array_v<T>)
      struct to<TOML, T>
      {
         template <auto Opts, class V, size_t N, class... Args>
         GLZ_ALWAYS_INLINE static void op(const V (&value)[N], is_context auto&& ctx, Args&&... args)
         {
            write<TOML>::op<Opts>(std::span{value, N}, ctx, std::forward<Args>(args)...);
         }
      };

      template <class T>
         requires glaze_array_t<T> || tuple_t<std::decay_t<T>> || is_std_tuple<T>
      struct to<TOML, T>
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
            using V = std::decay_t<T>;
            invoke_table<N>([&]<size_t I>() {
               if constexpr (glaze_array_t<V>) {
                  write<TOML>::op<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, args...);
               }
               else if constexpr (is_std_tuple<T>) {
                  using Value = core_t<decltype(std::get<I>(value))>;
                  to<TOML, Value>::template op<Opts>(std::get<I>(value), ctx, args...);
               }
               else {
                  using Value = core_t<decltype(glz::get<I>(value))>;
                  to<TOML, Value>::template op<Opts>(glz::get<I>(value), ctx, args...);
               }
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  write_array_entry_separator<Opts>(ctx, args...);
               }
            });
            dump<']'>(args...);
         }
      };

      template <is_includer T>
      struct to<TOML, T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, Args&&... args)
         {
            dump<R"("")">(args...); // dump an empty string
         }
      };

      template <class T>
         requires glaze_object_t<T> || reflectable<T>
      struct to<TOML, T>
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
                     write<TOML>::op<disable_write_unknown_on<Options>()>(std::move(merged), ctx, b, ix);
                  }
                  else {
                     write<TOML>::op<disable_write_unknown_on<Options>()>(value, ctx, b, ix);
                  }
               }
               else if constexpr (std::is_member_function_pointer_v<WriterType>) {
                  decltype(auto) unknown_writer = (value.*writer)();
                  if (unknown_writer.size() > 0) {
                     // TODO: This intermediate is added to get GCC 14 to build
                     decltype(auto) merged = glz::merge{value, unknown_writer};
                     write<TOML>::op<disable_write_unknown_on<Options>()>(std::move(merged), ctx, b, ix);
                  }
                  else {
                     write<TOML>::op<disable_write_unknown_on<Options>()>(value, ctx, b, ix);
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

               if constexpr (not has_opening_handled(Options)) {
                  dump<'{'>(b, ix);
               }

               static constexpr auto N = reflect<T>::size;

               decltype(auto) t = [&]() -> decltype(auto) {
                  if constexpr (reflectable<T>) {
                     return to_tie(value);
                  }
                  else {
                     return nullptr;
                  }
               }();

               static constexpr auto padding = round_up_to_nearest_16(maximum_key_size<T> + write_padding_bytes);
               if constexpr (maybe_skipped<Opts, T>) {
                  bool first = true;
                  invoke_table<N>([&]<size_t I>() {
                     using val_t = field_t<T, I>;

                     if constexpr (always_skipped<val_t>) {
                        return;
                     }
                     else {
                        if constexpr (null_t<val_t>) {
                           if constexpr (always_null_t<val_t>)
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
                                 else if constexpr (nullable_value_t<val_t>) {
                                    return !get_member(value, element()).has_value();
                                 }
                                 else {
                                    return !bool(get_member(value, element()));
                                 }
                              }();
                              if (is_null) return;
                           }
                        }

                        maybe_pad<padding>(b, ix);

                        if (first) {
                           first = false;
                        }
                        else {
                           // Null members may be skipped so we cant just write it out for all but the last member
                           std::memcpy(&b[ix], ",", 1);
                           ++ix;
                        }

                        // MSVC requires get<I> rather than keys[I]
                        static constexpr auto key = glz::get<I>(reflect<T>::keys); // GCC 14 requires auto here
                        static constexpr auto quoted_key = quoted_key_v<key, Opts.prettify>;
                        static constexpr auto n = quoted_key.size();
                        std::memcpy(&b[ix], quoted_key.data(), n);
                        ix += n;

                        static constexpr auto check_opts =
                           required_padding<val_t>() ? write_unchecked_on<Opts>() : Opts;
                        if constexpr (reflectable<T>) {
                           to<TOML, val_t>::template op<check_opts>(get_member(value, get<I>(t)), ctx, b, ix);
                        }
                        else {
                           to<TOML, val_t>::template op<check_opts>(get_member(value, get<I>(reflect<T>::values)), ctx,
                                                                    b, ix);
                        }
                     }
                  });
               }
               else {
                  invoke_table<N>([&]<size_t I>() {
                     maybe_pad<padding>(b, ix);

                     using val_t = field_t<T, I>;

                     // MSVC requires get<I> rather than keys[I]
                     static constexpr auto key = glz::get<I>(reflect<T>::keys); // GCC 14 requires auto here
                     if constexpr (always_null_t<val_t>) {
                        if constexpr (I == 0) {
                           static constexpr auto quoted_key = join_v<quoted_key_v<key, Opts.prettify>, chars<"null">>;
                           static constexpr auto n = quoted_key.size();
                           std::memcpy(&b[ix], quoted_key.data(), n);
                           ix += n;
                        }
                        else {
                           static constexpr auto quoted_key = join_v<chars<",">, quoted_key_v<key>, chars<"null">>;
                           static constexpr auto n = quoted_key.size();
                           std::memcpy(&b[ix], quoted_key.data(), n);
                           ix += n;
                        }
                     }
                     else {
                        if constexpr (I == 0) {
                           static constexpr auto quoted_key = quoted_key_v<key, Opts.prettify>;
                           static constexpr auto n = quoted_key.size();
                           std::memcpy(&b[ix], quoted_key.data(), n);
                           ix += n;
                        }
                        else {
                           static constexpr auto quoted_key = join_v<chars<",">, quoted_key_v<key>>;
                           static constexpr auto n = quoted_key.size();
                           std::memcpy(&b[ix], quoted_key.data(), n);
                           ix += n;
                        }

                        if constexpr (reflectable<T>) {
                           to<TOML, val_t>::template op<Opts>(get_member(value, get<I>(t)), ctx, b, ix);
                        }
                        else {
                           to<TOML, val_t>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx,
                                                                    b, ix);
                        }
                     }
                  });
               }

               // Options is required here, because it must be the top level
               if constexpr (not has_closing_handled(Options)) {
                  dump<'}'>(b, ix);
               }
            }
         }
      };
   } // namespace detail

   template <write_toml_supported T, output_buffer Buffer>
   [[nodiscard]] error_ctx write_toml(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = TOML}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_toml_supported T, raw_buffer Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> write_toml(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = TOML}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_toml_supported T>
   [[nodiscard]] glz::expected<std::string, error_ctx> write_toml(T&& value)
   {
      return write<opts{.format = TOML}>(std::forward<T>(value));
   }

   template <opts Opts = opts{.format = TOML}, write_json_supported T>
   [[nodiscard]] error_ctx write_file_toml(T&& value, const sv file_name, auto&& buffer)
   {
      const auto ec = write<set_toml<Opts>()>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec;
      }
      return {buffer_to_file(buffer, file_name)};
   }
}
