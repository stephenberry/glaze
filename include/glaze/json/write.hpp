// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "fmt/format.h"
#include "fmt/compile.h"

#include <charconv>
#include <iterator>
#include <ostream>

#include "glaze/core/format.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/json/from_ptr.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct to_json {};
      
      template <>
      struct write<json>
      {
         template <auto Opts, class T, class B>
         static void op(T&& value, B&& b) {
            to_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<B>(b));
         }
         
         template <auto Opts, class T, class B, class IX>
         static void op(T&& value, B&& b, IX&& ix) {
            to_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<B>(b), std::forward<IX>(ix));
         }
      };
      
      template <class T>
      requires (std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(const bool value, Args&&... args) noexcept
         {
            if (value) {
               dump<"true">(std::forward<Args>(args)...);
            }
            else {
               dump<"false">(std::forward<Args>(args)...);
            }
         }
      };
      
      template <num_t T>
      struct to_json<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, B&& b) noexcept
         {
            if constexpr (std::same_as<std::decay_t<B>, char*>) {
               b = fmt::format_to(std::forward<B>(b), FMT_COMPILE("{}"), value);
            }
            else {
               fmt::format_to(std::back_inserter(b), FMT_COMPILE("{}"), value);
            }
         }
         
         template <auto Opts, class B>
         static void op(auto&& value, B&& b, auto&& ix) noexcept
         {
            /*if constexpr (std::same_as<std::decay_t<B>, std::string>) {
               // more efficient strings in C++23:
             https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite
             }*/
            
            // https://stackoverflow.com/questions/1701055/what-is-the-maximum-length-in-chars-needed-to-represent-any-double-value
            // maximum length for a double should be 24 chars, we use 64 to be sufficient
            if (ix + 64 > b.size()) [[unlikely]] {
               b.resize(std::max(b.size() * 2, ix + 64));
            }
            
            auto start = b.data() + ix;
            auto end = fmt::format_to(start, FMT_COMPILE("{}"), value);
            ix += std::distance(start, end);
         }
      };

      template <class T>
      requires str_t<T> || char_t<T>
      struct to_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'"'>(b);
            const auto write_char = [&](auto&& c) {
               switch (c) {
               case '\\':
               case '"':
                  dump<'\\'>(b);
                  break;
               }
               dump(c, b);
            };
            if constexpr (char_t<T>) {
               write_char(value);
            }
            else {
               const sv str = value;
               for (auto&& c : str) {
                  write_char(c);
               }
            }
            dump<'"'>(b);
         }
         
         template <auto Opts>
         static void op(auto&& value, auto&& b, auto&& ix) noexcept
         {
            if constexpr (char_t<T>) {
               dump<'"'>(b, ix);
               switch (value) {
               case '\\':
               case '"':
                  dump<'\\'>(b, ix);
                  break;
               }
               dump(value, b, ix);
               dump<'"'>(b, ix);
            }
            else {
               const sv str = value;
               const auto n = str.size();
               
               // we use 4 * n to handle potential escape characters and quoted bounds (excessive safety)
               if ((ix + 4 * n) >= b.size()) [[unlikely]] {
                  b.resize(std::max(b.size() * 2, ix + 4 * n));
               }
               
               dump_unchecked<'"'>(b, ix);
               
               // now we don't have to check writing
               for (auto&& c : str) {
                  switch (c) {
                  case '\\':
                  case '"':
                     b[ix] = '\\';
                     ++ix;
                     break;
                  }
                  b[ix] = c;
                  ++ix;
               }
               dump_unchecked<'"'>(b, ix);
            }
         }
      };

      template <glaze_enum_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args) noexcept
         {
            using key_t = std::underlying_type_t<T>;
            static constexpr auto frozen_map =
               detail::make_enum_to_string_map<T>();
            const auto& member_it = frozen_map.find(static_cast<key_t>(value));
            if (member_it != frozen_map.end()) {
               const sv str = {member_it->second.data(),
                                       member_it->second.size()};
               // Note: Assumes people dont use strings with chars that need to
               // be
               // escaped for their enum names
               // TODO: Could create a pre qouted map for better perf
               dump<'"'>(std::forward<Args>(args)...);
               dump(str, std::forward<Args>(args)...);
               dump<'"'>(std::forward<Args>(args)...);
            }
            else [[unlikely]] {
               // What do we want to happen if the value doesnt have a mapped
               // string
               write<json>::op<Opts>(
                  static_cast<std::underlying_type_t<T>>(value), std::forward<Args>(args)...);
            }
         }
      };

      template <func_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& /*value*/, Args&&...) noexcept
         {}
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct to_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& b) noexcept {
            dump(value.str, b);
         }
         
         template <auto Opts>
         static void op(auto&& value, auto&& b, auto&& ix) noexcept {
            dump(value.str, b, ix);
         }
      };
      
      template <array_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args) noexcept
         {
            dump<'['>(std::forward<Args>(args)...);
            const auto is_empty = [&]() -> bool {
               if constexpr (nano::ranges::sized_range<T>) {
                  return value.size() ? false : true;
               }
               else {
                  return value.empty();
               }
            }();
            
            if (!is_empty) {
               auto it = value.begin();
               write<json>::op<Opts>(*it, std::forward<Args>(args)...);
               ++it;
               const auto end = value.end();
               for (; it != end; ++it) {
                  dump<','>(std::forward<Args>(args)...);
                  write<json>::op<Opts>(*it, std::forward<Args>(args)...);
               }
            }
            dump<']'>(std::forward<Args>(args)...);
         }
      };

      template <map_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args) noexcept
         {
            dump<'{'>(std::forward<Args>(args)...);
            if (!value.empty()) {
               auto it = value.cbegin();
               auto write_pair = [&] {
                  using Key = decltype(it->first);
                  if constexpr (str_t<Key> || char_t<Key>) {
                     write<json>::op<Opts>(it->first, std::forward<Args>(args)...);
                  }
                  else {
                     dump<'"'>(std::forward<Args>(args)...);
                     write<json>::op<Opts>(it->first, std::forward<Args>(args)...);
                     dump<'"'>(std::forward<Args>(args)...);
                  }
                  dump<':'>(std::forward<Args>(args)...);
                  write<json>::op<Opts>(it->second, std::forward<Args>(args)...);
               };
               write_pair();
               ++it;
               
               const auto end = value.cend();
               for (; it != end; ++it) {
                  dump<','>(std::forward<Args>(args)...);
                  write_pair();
               }
            }
            dump<'}'>(std::forward<Args>(args)...);
         }
      };
      
      template <nullable_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args) noexcept
         {
            if (value)
               write<json>::op<Opts>(*value, std::forward<Args>(args)...);
            else {
               dump<"null">(std::forward<Args>(args)...);
            }
         }
      };

      template <class T>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args) noexcept
         {
            static constexpr auto N = []() constexpr
            {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return std::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return std::tuple_size_v<std::decay_t<T>>;
               }
            }
            ();
            
            dump<'['>(std::forward<Args>(args)...);
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(value.*std::get<I>(meta_v<V>), std::forward<Args>(args)...);
               }
               else {
                  write<json>::op<Opts>(std::get<I>(value), std::forward<Args>(args)...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  dump<','>(std::forward<Args>(args)...);
               }
            });
            dump<']'>(std::forward<Args>(args)...);
         }
      };

      template <const std::string_view& S>
      inline constexpr auto array_from_sv()
      {
         constexpr auto N = S.size();
         std::array<char, N> arr;
         std::copy_n(S.data(), N, arr.data());
         return arr;
      }
      
      template <const std::string_view& S>
      inline constexpr bool needs_escaping()
      {
         for (auto& c : S) {
            if (c == '"') {
               return true;
            }
         }
         return false;
      }
      
      template <class T>
      requires glaze_object_t<T>
      struct to_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump<'{'>(b);
            for_each<N>([&](auto I) {
               static constexpr auto item = std::get<I>(meta_v<V>);
               using Key =
                  typename std::decay_t<std::tuple_element_t<0, decltype(item)>>;
               if constexpr (str_t<Key> || char_t<Key>) {
                  write<json>::op<Opts>(std::get<0>(item), b);
                  dump<':'>(b);
               }
               else {
                  static constexpr auto quoted =
                     concat_arrays(concat_arrays("\"", std::get<0>(item)), "\":");
                  write<json>::op<Opts>(quoted, b);
               }
               if constexpr (std::is_member_pointer_v<
                                std::tuple_element_t<1, decltype(item)>>) {
                  write<json>::op<Opts>(value.*std::get<1>(item), b);
               }
               else {
                  write<json>::op<Opts>(std::get<1>(item)(value), b);
               }
               constexpr auto S = std::tuple_size_v<decltype(item)>;
               if constexpr (Opts.comments && S > 2) {
                  constexpr sv comment = std::get<2>(item);
                  if constexpr (comment.size() > 0) {
                     dump<"/*">(b);
                     dump(comment, b);
                     dump<"*/">(b);
                  }
               }
               if constexpr (I < N - 1) {
                  dump<','>(b);
               }
            });
            dump<'}'>(b);
         }
         
         template <auto Opts>
         static void op(auto&& value, auto&& b, auto&& ix) noexcept
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump<'{'>(b, ix);
            for_each<N>([&](auto I) {
               static constexpr auto item = std::get<I>(meta_v<V>);
               using Key =
                  typename std::decay_t<std::tuple_element_t<0, decltype(item)>>;
               if constexpr (str_t<Key> || char_t<Key>) {
                  static constexpr sv key = std::get<0>(item);
                  // TODO: gain this performance when MSVC fixes their bug
                  #if _MSC_VER
                  write<json>::op<Opts>(key, b, ix);
                  dump<':'>(b, ix);
                  #else
                  if constexpr (needs_escaping<key>()) {
                     write<json>::op<Opts>(key, b, ix);
                     dump<':'>(b, ix);
                  }
                  else {
                     static constexpr auto quoted = join_v<chars<"\"">, key, chars<"\":">>;
                     dump<quoted>(b, ix);
                  }
                  #endif
               }
               else {
                  static constexpr auto quoted =
                     concat_arrays(concat_arrays("\"", std::get<0>(item)), "\":");
                  write<json>::op<Opts>(quoted, b, ix);
               }
               if constexpr (std::is_member_pointer_v<
                                std::tuple_element_t<1, decltype(item)>>) {
                  write<json>::op<Opts>(value.*std::get<1>(item), b, ix);
               }
               else {
                  write<json>::op<Opts>(std::get<1>(item)(value), b, ix);
               }
               constexpr auto S = std::tuple_size_v<decltype(item)>;
               if constexpr (Opts.comments && S > 2) {
                  constexpr sv comment = std::get<2>(item);
                  if constexpr (comment.size() > 0) {
                     dump<"/*">(b, ix);
                     dump(comment, b, ix);
                     dump<"*/">(b, ix);
                  }
               }
               if constexpr (I < N - 1) {
                  dump<','>(b, ix);
               }
            });
            dump<'}'>(b, ix);
         }
      };
   }  // namespace detail
   
   template <class T, class Buffer>
   inline auto write_json(T&& value, Buffer&& buffer) {
      return write<opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   template <class T>
   inline auto write_json(T&& value) {
      std::string buffer{};
      write<opts{}>(std::forward<T>(value), buffer);
      return buffer;
   }
   
   template <class T, class Buffer>
   inline void write_jsonc(T&& value, Buffer&& buffer) {
      write<opts{.comments = true}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   template <class T>
   inline auto write_jsonc(T&& value) {
      std::string buffer{};
      write<opts{.comments = true}>(std::forward<T>(value), buffer);
      return buffer;
   }
}  // namespace glaze
