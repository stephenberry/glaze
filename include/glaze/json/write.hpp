// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif  // !FMT_HEADER_ONLY
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
         template <auto& Opts, class T, class B>
         static void op(T&& value, B&& b) {
            to_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<B>(b));
         }
      };
      
      template <class T>
      requires (std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(const bool value, auto&& b) noexcept
         {
            if (value) {
               dump<"true">(b);
            }
            else {
               dump<"false">(b);
            }
         }
      };
      
      template <num_t T>
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            /*if constexpr (std::same_as<std::decay_t<B>, std::string>) {
               // more efficient strings in C++23:
             https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite
             }*/
            fmt::format_to(std::back_inserter(b), FMT_COMPILE("{}"), value);
         }
      };

      template <class T>
      requires str_t<T> || char_t<T>
      struct to_json<T>
      {
         template <auto& Opts>
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
               const std::string_view str = value;
               for (auto&& c : str) {
                  write_char(c);
               }
            }
            dump<'"'>(b);
         }
      };

      template <glaze_enum_t T>
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            using key_t = std::underlying_type_t<T>;
            static constexpr auto frozen_map =
               detail::make_enum_to_string_map<T>();
            const auto& member_it = frozen_map.find(static_cast<key_t>(value));
            if (member_it != frozen_map.end()) {
               std::string_view str = {member_it->second.data(),
                                       member_it->second.size()};
               // Note: Assumes people dont use strings with chars that need to
               // be
               // escaped for their enum names
               // TODO: Could create a pre qouted map for better perf
               dump<'"'>(b);
               dump(str, b);
               dump<'"'>(b);
            }
            else [[unlikely]] {
               // What do we want to happen if the value doesnt have a mapped
               // string
               write<json>::op<Opts>(
                  static_cast<std::underlying_type_t<T>>(value), b);
            }
         }
      };

      template <func_t T>
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(auto&& /*value*/, auto&& /*b*/) noexcept
         {}
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept {
            dump(value.str, b);
         }
      };
      
      template <array_t T>
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'['>(b);
            if (!value.empty()) {
               auto it = value.begin();
               write<json>::op<Opts>(*it, b);
               ++it;
               const auto end = value.end();
               for (; it != end; ++it) {
                  dump<','>(b);
                  write<json>::op<Opts>(*it, b);
               }
            }
            dump<']'>(b);
         }
      };

      template <map_t T>
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'{'>(b);
            if (!value.empty()) {
               auto it = value.cbegin();
               auto write_pair = [&] {
                  using Key = decltype(it->first);
                  if constexpr (str_t<Key> || char_t<Key>) {
                     write<json>::op<Opts>(it->first, b);
                  }
                  else {
                     dump<'"'>(b);
                     write<json>::op<Opts>(it->first, b);
                     dump<'"'>(b);
                  }
                  dump<':'>(b);
                  write<json>::op<Opts>(it->second, b);
               };
               write_pair();
               ++it;
               
               const auto end = value.cend();
               for (; it != end; ++it) {
                  dump<','>(b);
                  write_pair();
               }
            }
            dump<'}'>(b);
         }
      };
      
      template <nullable_t T>
      struct to_json<T>
      {
         template <auto& Opts, class B>
         static void op(auto&& value, B&& b) noexcept
         {
            if (value)
               write<json>::op<Opts>(*value, std::forward<B>(b));
            else {
               dump<"null">(b);
            }
         }
      };

      template <class T>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
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
            
            dump<'['>(b);
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(value.*std::get<I>(meta_v<V>), b);
               }
               else {
                  write<json>::op<Opts>(std::get<I>(value), b);
               }
               if constexpr (I < N - 1) {
                  dump<','>(b);
               }
            });
            dump<']'>(b);
         }
      };
      
      template <class T>
      requires glaze_object_t<T>
      struct to_json<T>
      {
         template <auto& Opts>
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
      };
   }  // namespace detail
   
   template <class T, class Buffer>
   inline void write_json(T&& value, Buffer&& buffer) {
      write<opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   template <class T, class Buffer>
   inline void write_jsonc(T&& value, Buffer&& buffer) {
      write<opts{.comments = true}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}  // namespace glaze
