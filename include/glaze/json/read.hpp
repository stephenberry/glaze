// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <iterator>
#include <ranges>
#include <charconv>

#include "fast_float/fast_float.h"
#include "glaze/core/read.hpp"
#include "glaze/core/format.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/for_each.hpp"

namespace glaze
{
   namespace detail
   {
      inline void skip_object_value(auto&& it, auto&& end)
      {
         skip_ws(it, end);
         if (it == end) [[unlikely]]
            return;
         switch (*it) {
         case '{':
            skip_until_closed<'{', '}'>(it, end);
            break;
         case '[':
            skip_until_closed<'[', ']'>(it, end);
            break;
         case '"':
            skip_string(it, end);
            skip_ws(it, end);
            break;
         default: {
            while (it < end) {
               switch (*it) {
               case '/':
                  skip_comment(it, end);
                  continue;
               case '"':
                  skip_string(it, end);
                  continue;
               case ',':
               case '}':
                  break;
               default:
                  ++it;
                  continue;
               }
               break;
            }
         }
         }
      }
      
      template <>
      struct read<json>
      {
         template <class T, class It0, class It1>
         static void op(T&& value, It0&& it, It1&& end) {
            from_json<std::decay_t<T>>::op(value, std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <bool_t T>
      struct from_json<T>
      {
         static void op(bool_t auto&& value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            if (it < end) [[likely]] {
               switch (*it) {
               case 't': {
                  ++it;
                  match<"rue">(it, end);
                  value = true;
                  break;
               }
               case 'f': {
                  ++it;
                  match<"alse">(it, end);
                  value = false;
                  break;
               }
                  [[unlikely]] default
                     : throw std::runtime_error("Expected true or false");
               }
            }
            else [[unlikely]] {
               throw std::runtime_error("Expected true or false");
            }
         }
      };
      
      template <num_t T>
      struct from_json<T>
      {
         template <class It>
         static void op(T& value, It&& it, auto&& end)
         {
            skip_ws(it, end);
            if (it == end) [[unlikely]]
               throw std::runtime_error("Unexpected end of buffer");
            
            if constexpr (std::contiguous_iterator<It>)
            {
               if constexpr (std::is_floating_point_v<T>)
               {
                  const auto size = std::distance(it, end);
                  const auto start = &*it;
                  auto [p, ec] = fast_float::from_chars(start, start + size, value);
                  if (ec != std::errc{}) [[unlikely]]
                     throw std::runtime_error("Failed to parse number");
                  it += (p - &*it);
               }
               else
               {
                  const auto size = std::distance(it, end);
                  const auto start = &*it;
                  auto [p, ec] = std::from_chars(start, start + size, value);
                  if (ec != std::errc{}) [[unlikely]]
                     throw std::runtime_error("Failed to parse number");
                  it += (p - &*it);
               }
            }
            else {
               double num;
               char buffer[256];
               size_t i{};
               while (it != end && is_numeric(*it)) {
                  if (i > 254) [[unlikely]]
                     throw std::runtime_error("Number is too long");
                  buffer[i] = *it++;
                  ++i;
               }
               auto [p, ec] = fast_float::from_chars(buffer, buffer + i, num);
               if (ec != std::errc{}) [[unlikely]]
                  throw std::runtime_error("Failed to parse number");
               value = static_cast<T>(num);
            }
         }
      };

      template <str_t T>
      struct from_json<T>
      {
         static void op(T& value, auto&& it, auto&& end)
         {
            // TODO: this does not handle control chars like \t and \n
            
            skip_ws(it, end);
            match<'"'>(it, end);
            const auto cend = value.cend();
            for (auto c = value.begin(); c < cend; ++c, ++it)
            {
               switch (*it) {
                  [[unlikely]] case '\\':
                  {
                     if (++it == end) [[unlikely]]
                        throw std::runtime_error("Expected \"");
                     else [[likely]] {
                        *c = *it;
                     }
                     break;
                  }
                  [[unlikely]] case '"':
                  {
                     ++it;
                     value.resize(std::distance(value.begin(), c));
                     return;
                  }
                  [[likely]] default : *c = *it;
               }
            }
            
            while (it < end) {
               switch (*it) {
                  [[unlikely]] case '\\':
                  {
                     if (++it == end) [[unlikely]]
                        throw std::runtime_error("Expected \"");
                     else [[likely]] {
                        value.push_back(*it);
                     }
                     break;
                  }
                  [[unlikely]] case '"':
                  {
                     ++it;
                     return;
                  }
                  [[likely]] default : value.push_back(*it);
               }
               ++it;
            }
            throw std::runtime_error("Expected \"");
         }
      };
      
      template <char_t T>
      struct from_json<T>
      {
         static void op(T& value, auto&& it, auto&& end)
         {
            // TODO: this does not handle escaped chars
            match<'"'>(it, end);
            if (it == end) [[unlikely]]
               throw std::runtime_error("Unxpected end of buffer");
            if (*it == '\\') [[unlikely]]
               if (++it == end) [[unlikely]]
                  throw std::runtime_error("Unxpected end of buffer");
            value = *it++;
            match<'"'>(it, end);
         }
      };
      
      template <>
      struct from_json<raw_json>
      {
         static void op(raw_json& value, auto&& it, auto&& end)
         {
            // TODO this will not work for streams where we cant move backward
            auto it_start = it;
            skip_object_value(it, end);
            value.str.clear();
            value.str.insert(value.str.begin(), it_start, it);
         }
      };

      template <class T> requires array_t<T> &&
      (emplace_backable<T> ||
       !resizeable<T>)
      struct from_json<T>
      {
         static void op(T& value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            auto value_it = value.begin();
            match<'['>(it, end);
            skip_ws(it, end);
            for (size_t i = 0; it < end; ++i) {
               if (*it == ']') [[unlikely]] {
                  ++it;
                  if constexpr (resizeable<T>) value.resize(i);
                  return;
               }
               if (i > 0) [[likely]] {
                  match<','>(it, end);
               }
               if (i < static_cast<size_t>(value.size())) {
                  read<json>::op(*value_it++, it, end);
               }
               else {
                  if constexpr (emplace_backable<T>) {
                     read<json>::op(value.emplace_back(), it, end);
                  }
                  else {
                     throw std::runtime_error("Exceeded static array size.");
                  }
               }
               skip_ws(it, end);
            }
            throw std::runtime_error("Expected ]");
         }
      };
      
      template <class T> requires array_t<T> &&
      (!emplace_backable<T> &&
       resizeable<T>)
      struct from_json<T>
      {
         static void op(T& value, auto&& it, auto&& end)
         {
            using value_t = nano::ranges::range_value_t<T>;
            static thread_local std::vector<value_t> buffer{};
            buffer.clear();

            skip_ws(it, end);
            match<'['>(it, end);
            skip_ws(it, end);
            for (size_t i = 0; it < end; ++i) {
               if (*it == ']') [[unlikely]] {
                  ++it;
                  value.resize(i);
                  auto value_it = std::ranges::begin(value);
                  for (size_t j = 0; j < i; ++j) {
                     *value_it++ = buffer[j];
                  }
                  return;
               }
               if (i > 0) [[likely]] {
                  match<','>(it, end);
               }
               read<json>::op(buffer.emplace_back(), it, end);
               skip_ws(it, end);
            }
            throw std::runtime_error("Expected ]");
         }
      };

      template <class T> requires glaze_array_t<T> || tuple_t<T>
      struct from_json<T>
      {
         static void op(T& value, auto&& it, auto&& end)
         {
            static constexpr auto N = []() constexpr
            {
               if constexpr (glaze_array_t<T>) {
                  return std::tuple_size_v<meta_t<T>>;
               }
               else {
                  return std::tuple_size_v<T>;
               }
            }
            ();
            
            skip_ws(it, end);
            match<'['>(it, end);
            skip_ws(it, end);
            
            for_each<N>([&](auto I) {
               if constexpr (I != 0) {
                  match<','>(it, end);
               }
               if constexpr (glaze_array_t<T>) {
                  read<json>::op(value.*std::get<I>(meta_v<T>), it, end);
               }
               else {
                  read<json>::op(std::get<I>(value), it, end);
               }
               skip_ws(it, end);
            });
            
            if constexpr (N == 0) {
               skip_ws(it, end);
            }
            match<']'>(it, end);
         }
      };
      
      template <class T>
      requires map_t<T> || glaze_object_t<T>
      struct from_json<T>
      {
         static void op(T& value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            match<'{'>(it, end);
            skip_ws(it, end);
            bool first = true;
            while (it < end) {
               if (*it == '}') [[unlikely]] {
                  ++it;
                  return;
               }
               else if (first) [[unlikely]]
                  first = false;
               else [[likely]] {
                  match<','>(it, end);
               }
               static thread_local std::string key{};
               read<json>::op(key, it, end);
               skip_ws(it, end);
               match<':'>(it, end);
               if constexpr (glaze_object_t<T>) {
                  static constexpr auto frozen_map = detail::make_map<T>();
                  const auto& member_it = frozen_map.find(frozen::string(key));
                  if (member_it != frozen_map.end()) {
                     std::visit(
                        [&](auto&& member_ptr) {
                           read<json>::op(value.*member_ptr, it, end);
                        },
                        member_it->second);
                  }
                  else [[unlikely]] {
                     skip_object_value(it, end);
                  }
               }
               else {
                  if constexpr (std::is_same_v<typename T::key_type,
                                               std::string>) {
                     read<json>::op(value[key], it, end);
                  }
                  else {
                     static thread_local typename T::key_type key_value{};
                     read<json>::op(key_value, key.begin(), key.end());
                     read<json>::op(value[key_value], it, end);
                  }
               }
               skip_ws(it, end);
            }
            throw std::runtime_error("Expected }");
         }
      };
      
      template <nullable_t T>
      struct from_json<T>
      {
         static void op(T& value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            if (it == end) {
               throw std::runtime_error("Unexexpected eof");
            }
            if (*it == 'n') {
               ++it;
               match<"ull">(it, end);
               if constexpr (!std::is_pointer_v<T>) {
                  value.reset();
               }
            }
            else {
               if (!value) {
                  if constexpr (is_specialization_v<T, std::optional>)
                     value = std::make_optional<typename T::value_type>();
                  else if constexpr (is_specialization_v<T, std::unique_ptr>)
                     value = std::make_unique<typename T::element_type>();
                  else if constexpr (is_specialization_v<T, std::shared_ptr>)
                     value = std::make_shared<typename T::element_type>();
                  else
                     throw std::runtime_error(
                        "Cannot read into unset nullable that is not "
                        "std::optional, std::unique_ptr, or std::shared_ptr");
               }
               read<json>::op(*value, it, end);
            }
         }
      };
   }  // namespace detail
   
   template <class T, class Buffer>
   inline void read_json(T& value, Buffer&& buffer) {
      read<json>(value, std::forward<Buffer>(buffer));
   }
}  // namespace glaze
