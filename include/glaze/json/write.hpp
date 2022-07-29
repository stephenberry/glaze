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

#include "glaze/common.hpp"
#include "glaze/format.hpp"

namespace glaze
{
   namespace detail
   {
      inline void dump(char c, std::string& b) noexcept {
         b.push_back(c);
      }
      
      template <char c>
      inline void dump(std::string& b) noexcept {
         b.push_back(c);
      }

      template <char c>
      inline void dump(std::output_iterator<char> auto&&it) noexcept
      {
         *it = c;
         ++it;
      }

      template <string_literal str>
      inline void dump(std::output_iterator<char> auto&& it) noexcept {
         std::copy(str.value, str.value + str.size, it);
      }

      template <string_literal str>
      inline void dump(std::string& b) noexcept {
         b.append(str.value, str.size);
      }

      inline void dump(const std::string_view str, std::output_iterator<char> auto&& it) noexcept {
         std::copy(str.data(), str.data() + str.size(), it);
      }

      inline void dump(const std::string_view str, std::string& b) noexcept {
         b.append(str.data(), str.size());
      }
      
      template <>
      struct write<json>
      {
         template <bool C, class T, class B>
         static void op(T&& value, B&& b) {
            to_json<std::decay_t<T>>::template op<C>(std::forward<T>(value), std::forward<B>(b));
         }
      };
      
      template <class T>
      requires (std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct to_json<T>
      {
         template <bool C>
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
         template <bool C>
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
      requires str_t<T> || char_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <bool C>
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
            if constexpr (char_t<std::decay_t<T>>) {
               write_char(std::forward<T>(value));
            }
            else {
               std::string_view str = value;
               for (auto&& c : str) {
                  write_char(c);
               }
            }
            dump<'"'>(b);
         }
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct to_json<T>
      {
         template <bool C>
         static void op(T&& value, auto&& b) noexcept {
            dump(value.str, b);
         }
      };
      
      template <array_t T>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'['>(b);
            if (!value.empty()) {
               write<json>::op<C>(*value.begin(), b);
               auto it = value.cbegin();
               ++it;
               for (; it != value.cend(); ++it) {
                  dump<','>(b);
                  write<json>::op<C>(*it, b);
               }
            }
            dump<']'>(b);
         }
      };

      template <class T>
      requires map_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <bool C>
         static void op(auto&& value, auto&& b) noexcept
         {
            dump<'{'>(b);
            bool first = true;
            for (auto&& item : std::forward<T>(value)) {
               if (first)
                  first = false;
               else
                  dump<','>(b);
               if constexpr (str_t<decltype(item.first)> ||
                             char_t<std::decay_t<decltype(item.first)>>) {
                  write<json>::op<C>(item.first, b);
               }
               else {
                  dump<'"'>(b);
                  write<json>::op<C>(item.first, b);
                  dump<'"'>(b);
               }
               dump<':'>(b);
               write<json>::op<C>(item.second, b);
            }
            dump<'}'>(b);
         }
      };
      
      template <nullable_t T>
      struct to_json<T>
      {
         template <bool C, class B>
         static void op(auto&& value, B&& b) noexcept
         {
            if (value)
               write<json>::op<C>(*value, std::forward<B>(b));
            else {
               dump<"null">(b);
            }
         }
      };

      template <class T>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <bool C, size_t I = 0>
         static void op(auto&& value, auto&& b) noexcept
         {
            constexpr auto n = []() constexpr
            {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return std::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return std::tuple_size_v<std::decay_t<T>>;
               }
            }
            ();

            if constexpr (I == 0) {
               dump<'['>(b);
            }
            using value_t = std::decay_t<T>;
            if constexpr (I < n) {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  write<json>::op<C>(value.*std::get<I>(meta_v<value_t>), b);
               }
               else {
                  write<json>::op<C>(std::get<I>(value), b);
               }
               if constexpr (I < n - 1) {
                  dump<','>(b);
                  op<C, I + 1>(value, b);
               }
            }
            if constexpr (I == 0) {
               dump<']'>(b);
            }
         }
      };
      
      template <class T>
      requires glaze_object_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <bool C, size_t I = 0>
         static void op(auto&& value, auto&& b) noexcept
         {
            if constexpr (I == 0) {
               dump<'{'>(b);
            }
            using value_t = std::decay_t<T>;
            if constexpr (I < std::tuple_size_v<meta_t<value_t>>) {
               static constexpr auto item = std::get<I>(meta_v<value_t>);
               using key_t =
                  typename std::decay_t<std::tuple_element_t<0, decltype(item)>>;
               if constexpr (str_t<key_t> || char_t<key_t>) {
                  write<json>::op<C>(std::get<0>(item), b);
                  dump<':'>(b);
               }
               else {
                  static constexpr auto quoted =
                     concat_arrays(concat_arrays("\"", std::get<0>(item)), "\":");
                  write<json>::op<C>(quoted, b);
               }
               write<json>::op<C>(value.*std::get<1>(item), b);
               if constexpr (C && std::tuple_size_v < decltype(item) >> 2) {
                  static_assert(
                     std::is_same_v<std::decay_t<decltype(std::get<2>(item))>,
                                    comment_t>);
                  constexpr auto comment = std::get<2>(item).str;
                  if constexpr (comment.size() > 0) {
                     dump<"/*">(b);
                     dump(comment, b);
                     dump<"*/">(b);
                  }
               }
               if constexpr (I < std::tuple_size_v<meta_t<value_t>> - 1) {
                  dump<','>(b);
                  op<C, I + 1>(value, b);
               }
            }
            if constexpr (I == 0) {
               dump<'}'>(b);
            }
         }
      };
   }  // namespace detail

   // For writing json to a std::string, std::vector<char>, std::deque<char> and
   // the like
   template <uint32_t Format, bool C, class T, class Buffer>
   requires nano::ranges::input_range<Buffer> &&
      std::same_as<char, nano::ranges::range_value_t<Buffer>>
   inline void write(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (std::same_as<Buffer, std::string>) {
         detail::write<Format>::template op<C>(std::forward<T>(value), buffer);
      }
      else {
         detail::write<Format>::template op<C>(std::forward<T>(value), std::back_inserter(buffer));
      }
   }

   // For writing json to std::ofstream, std::cout, or other streams
   template <uint32_t Format, bool C, class T>
   inline void write(T&& value, std::ostream& os) noexcept
   {
      detail::write<Format>::template op<C>(std::forward<T>(value),
                        std::ostreambuf_iterator<char>(os));
   }
   
   template <class T, class Buffer>
   inline void write_json(T&& value, Buffer&& buffer) {
      write<json, false>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   template <class T, class Buffer>
   inline void write_jsonc(T&& value, Buffer&& buffer) {
      write<json, true>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}  // namespace glaze
