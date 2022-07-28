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

namespace glaze
{
   namespace detail
   {
      inline void write(char c, std::string& b) noexcept {
         b.push_back(c);
      }
      
      template <char c>
      inline void write(std::string& b) noexcept {
         b.push_back(c);
      }

      template <char c>
      inline void write(std::output_iterator<char> auto&&it) noexcept
      {
         *it = c;
         ++it;
      }

      template <string_literal str>
      inline void write(std::output_iterator<char> auto&& it) noexcept {
         std::copy(str.value, str.value + str.size, it);
      }

      template <string_literal str>
      inline void write(std::string& b) noexcept {
         b.append(str.value, str.size);
      }

      inline void write(const std::string_view str, std::output_iterator<char> auto&& it) noexcept {
         std::copy(str.data(), str.data() + str.size(), it);
      }

      inline void write(const std::string_view str, std::string& b) noexcept {
         b.append(str.data(), str.size());
      }

      template <bool C = false, class B>
      inline void to_buffer(const bool value, B&& b) noexcept
      {
         if (value) {
            write<"true">(b);
         }
         else {
            write<"false">(b);
         }
      }

      template <bool C = false>
      inline void to_buffer(num_t auto&& value, auto&& b) noexcept
      {
         /*if constexpr (std::same_as<std::decay_t<B>, std::string>) {
            // more efficient strings in C++23:
          https://en.cppreference.com/w/cpp/string/basic_string/resize_and_overwrite
          }*/
         fmt::format_to(std::back_inserter(b), FMT_COMPILE("{}"), value);
      }

      template <bool C = false, class T, class B>
      requires str_t<T> || char_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept
      {
         write<'"'>(b);
         const auto write_char = [&](auto&& c) {
            switch (c) {
            case '\\':
            case '"':
               write<'\\'>(b);
               break;
            }
            write(c, b);
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
         write<'"'>(b);
      }

      template <bool C = false, class T, class B>
      requires std::same_as<std::decay_t<T>, raw_json>
      inline void to_buffer(T&& value, B&& b) noexcept { write(value.str, b); }

      // Container and Object Support
      template <bool C = false, class T, class B>
      requires array_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept;

      template <bool C = false, class T, class B>
      requires map_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept;

      template <bool C = false, class T, class B>
      requires nullable_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept;

      template <bool C = false, size_t I = 0, class T, class B>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept;

      template <bool C = false, size_t I = 0, class T, class B>
      requires glaze_object_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept;

      template <bool C = false, class T, class B>
      requires custom_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b)
      {
         custom<std::decay_t<T>>::template to_buffer<C>(std::forward<T>(value),
                                               std::forward<B>(b));
      }

      template <bool C, class T, class B>
      requires array_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept
      {
         write<'['>(b);
         if (!value.empty()) {
            to_buffer<C>(*value.begin(), b);
            auto it = value.cbegin();
            ++it;
            for (; it != value.cend(); ++it) {
               write<','>(b);
               to_buffer<C>(*it, b);
            }
         }
         write<']'>(b);
      }

      template <bool C, class T, class B>
      requires map_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept
      {
         write<'{'>(b);
         bool first = true;
         for (auto&& item : value) {
            if (first)
               first = false;
            else
               write<','>(b);
            if constexpr (str_t<decltype(item.first)> ||
                          char_t<std::decay_t<decltype(item.first)>>) {
               to_buffer(item.first, b);
            }
            else {
               write<'"'>(b);
               to_buffer(item.first, b);
               write<'"'>(b);
            }
            write<':'>(b);
            to_buffer<C>(item.second, b);
         }
         write<'}'>(b);
      }

      template <bool C, class T, class B>
      requires nullable_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept
      {
         if (value)
            to_buffer<C>(*value, std::forward<B>(b));
         else {
            write<"null">(b);
         }
      }

      template <bool C, size_t I, class T, class B>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept
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
            write<'['>(b);
         }
         using value_t = std::decay_t<T>;
         if constexpr (I < n) {
            if constexpr (glaze_array_t<std::decay_t<T>>) {
               to_buffer<C>(value.*std::get<I>(meta_v<value_t>), b);
            }
            else {
               to_buffer<C>(std::get<I>(value), b);
            }
            if constexpr (I < n - 1) {
               write<','>(b);
               to_buffer<C, I + 1>(std::forward<T>(value), b);
            }
         }
         if constexpr (I == 0) {
            write<']'>(b);
         }
      }

      template <bool C, size_t I, class T, class B>
      requires glaze_object_t<std::decay_t<T>>
      inline void to_buffer(T&& value, B&& b) noexcept
      {
         if constexpr (I == 0) {
            write<'{'>(b);
         }
         using value_t = std::decay_t<T>;
         if constexpr (I < std::tuple_size_v<meta_t<value_t>>) {
            static constexpr auto item = std::get<I>(meta_v<value_t>);
            using key_t =
               typename std::decay_t<std::tuple_element_t<0, decltype(item)>>;
            if constexpr (str_t<key_t> || char_t<key_t>) {
               to_buffer<C>(std::get<0>(item), b);
               write<':'>(b);
            }
            else {
               static constexpr auto quoted =
                  concat_arrays(concat_arrays("\"", std::get<0>(item)), "\":");
               to_buffer<C>(quoted, b);
            }
            to_buffer<C>(value.*std::get<1>(item), b);
            if constexpr (C && std::tuple_size_v < decltype(item) >> 2) {
               static_assert(
                  std::is_same_v<std::decay_t<decltype(std::get<2>(item))>,
                                 comment_t>);
               constexpr auto comment = std::get<2>(item).str;
               if constexpr (comment.size() > 0) {
                  write<"/*">(b);
                  write(comment, b);
                  write<"*/">(b);
               }
            }
            if constexpr (I < std::tuple_size_v<meta_t<value_t>> - 1) {
               write<','>(b);
               to_buffer<C, I + 1>(std::forward<T>(value), b);
            }
         }
         if constexpr (I == 0) {
            write<'}'>(b);
         }
      }
   }  // namespace detail

   // For writing json to a std::string, std::vector<char>, std::deque<char> and
   // the like
   template <class T, class Buffer>
   requires nano::ranges::input_range<Buffer> &&
      std::same_as<char, nano::ranges::range_value_t<Buffer>>
   inline void write_json(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (std::same_as<Buffer, std::string>) {
         detail::to_buffer(std::forward<T>(value), buffer);
      }
      else {
         detail::to_buffer(std::forward<T>(value), std::back_inserter(buffer));
      }
   }

   // For writing json to std::ofstream, std::cout, or other streams
   template <class T>
   inline void write_json(T&& value, std::ostream& os) noexcept
   {
      detail::to_buffer(std::forward<T>(value),
                        std::ostreambuf_iterator<char>(os));
   }

   // For writing json to a std::string, std::vector<char>, std::deque<char> and
   // the like
   template <class T, class Buffer>
   requires nano::ranges::input_range<Buffer> &&
      std::same_as<char, nano::ranges::range_value_t<Buffer>>
   inline void write_jsonc(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (std::same_as<Buffer, std::string>) {
         detail::to_buffer<true>(std::forward<T>(value), buffer);
      }
      else {
         detail::to_buffer<true>(std::forward<T>(value),
                                 std::back_inserter(buffer));
      }
   }

   // For writing json to std::ofstream, std::cout, or other streams
   template <class T>
   inline void write_jsonc(T&& value, std::ostream& os) noexcept
   {
      detail::to_buffer<true>(std::forward<T>(value),
                              std::ostreambuf_iterator<char>(os));
   }
}  // namespace glaze
