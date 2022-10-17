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

namespace glz
{
   namespace detail
   {
      inline void skip_object_value(auto&& it, auto&& end)
      {
         skip_ws(it, end);
         while (it != end) {
            switch (*it) {
               case '{':
                  skip_until_closed<'{', '}'>(it, end);
                  break;
               case '[':
                  skip_until_closed<'[', ']'>(it, end);
                  break;
               case '"':
                  skip_string(it, end);
                  break;
               case '/':
                  skip_comment(it, end);
                  continue;
               case ',':
               case '}':
               case ']':
                  break;
               default: {
                  ++it;
                  continue;
               }
            }
            
            break;
         }
      }
      
      template <class T = void>
      struct from_json {};
      
      template <>
      struct read<json>
      {
         template <auto Opts, class T, class It0, class It1>
         static void op(T&& value, It0&& it, It1&& end) {
            from_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <bool_t T>
      struct from_json<T>
      {
         template <auto Opts>
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
         template <auto Opts, class It>
         static void op(auto&& value, It&& it, auto&& end)
         {
            skip_ws(it, end);
            if (it == end) [[unlikely]]
               throw std::runtime_error("Unexpected end of buffer");
            
            if constexpr (std::contiguous_iterator<std::decay_t<It>>)
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
                  double temp;
                  const auto size = std::distance(it, end);
                  const auto start = &*it;
                  auto [p, ec] = fast_float::from_chars(start, start + size, temp);
                  if (ec != std::errc{}) [[unlikely]]
                     throw std::runtime_error("Failed to parse number");
                  it += (p - &*it);
                  value = static_cast<T>(temp);
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
         template <auto Opts, class It, class End>
         static void op(auto& value, It&& it, End&& end)
         {
            // TODO: this does not handle control chars like \t and \n
            
            if constexpr (!Opts.opening_handled) {
               skip_ws(it, end);
               match<'"'>(it, end);
            }
            
            // overwrite portion
            
            if constexpr (nano::contiguous_iterator<std::decay_t<It>>
                          && nano::contiguous_iterator<std::decay_t<End>>) {
               const auto n_buffer = static_cast<size_t>(std::distance(it, end));
               const auto n_value = value.size();
               
               // must use 2 * n_value to handle potential escape characters
               if (2 * n_value < n_buffer) {
                  // we don't have to check if we are reaching our end iterator
                  
                  for (size_t i = 0; i < n_value; ++i, ++it) {
                     switch (*it) {
                        [[unlikely]] case '\\':
                        {
                           ++it;
                           value[i] = *it;
                           break;
                        }
                        [[unlikely]] case '"':
                        {
                           ++it;
                           value.resize(i);
                           return;
                        }
                        [[likely]] default : value[i] = *it;
                     }
                  }
               }
               else {
                  for (size_t i = 0; i < n_value; ++i, ++it)
                  {
                     if (it == end) [[unlikely]]
                        throw std::runtime_error(R"(Expected ")");
                     switch (*it) {
                        [[unlikely]] case '\\':
                        {
                           if (++it == end) [[unlikely]]
                              throw std::runtime_error(R"(Expected ")");
                           else [[likely]] {
                              value[i] = *it;
                           }
                           break;
                        }
                        [[unlikely]] case '"':
                        {
                           ++it;
                           value.resize(i);
                           return;
                        }
                        [[likely]] default : value[i] = *it;
                     }
                  }
               }
            }
            else {
               const auto cend = value.cend();
               for (auto c = value.begin(); c < cend; ++c, ++it)
               {
                  if (it == end) [[unlikely]]
                     throw std::runtime_error(R"(Expected ")");
                  switch (*it) {
                     [[unlikely]] case '\\':
                     {
                        if (++it == end) [[unlikely]]
                           throw std::runtime_error(R"(Expected ")");
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
            }
            
            // growth portion
            if constexpr (nano::contiguous_iterator<std::decay_t<It>>
                          && nano::contiguous_iterator<std::decay_t<End>>) {
               auto start = it;

               while (it < end) {
                  switch (*it) {
                     case '\\': {
                        value.append(start, it);
                        ++it; // skip first escape
                        value.push_back(*it); // add the escaped character
                        ++it;
                        start = it;
                        break;
                     }
                     case '"': {
                        value.append(start, it);
                        ++it;
                        return;
                     }
                     default:
                        break;
                  }
                  
                  ++it;
               }
            }
            else {
               while (it < end) {
                  switch (*it) {
                     [[unlikely]] case '\\':
                     {
                        if (++it == end) [[unlikely]]
                           throw std::runtime_error(R"(Expected ")");
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
            }
         }
      };
      
      template <char_t T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& value, auto&& it, auto&& end)
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

      template <glaze_enum_t T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& value, auto&& it, auto&& end)
         {
            //Could do better key parsing for enums since we know we cant have escapes and we know the max size
            static thread_local std::string key{};
            read<json>::op<Opts>(key, it, end);

            static constexpr auto frozen_map = detail::make_string_to_enum_map<T>();
            const auto& member_it = frozen_map.find(frozen::string(key));
            if (member_it != frozen_map.end()) {
               value = member_it->second;
            }
            else [[unlikely]] {
               throw std::runtime_error("Enexpected enum value '" + key + "'");
            }
         }
      };

      template <func_t T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& /*value*/, auto&& /*it*/, auto&& /*end*/)
         {
         }
      };
      
      template <>
      struct from_json<raw_json>
      {
         template <auto Opts>
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
         template <auto Opts>
         static void op(auto& value, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            match<'['>(it, end);
            skip_ws(it, end);
            if (it == end) {
               throw std::runtime_error("Unexpected end");
            }
            
            if (*it == ']') [[unlikely]] {
               ++it;
               if constexpr (resizeable<T>) {
                  value.clear();
               }
               return;
            }
            
            const auto n = value.size();
            
            auto value_it = value.begin();
            
            for (size_t i = 0; i < n; ++i) {
               read<json>::op<Opts>(*value_it++, it, end);
               skip_ws(it, end);
               if (it == end) {
                  throw std::runtime_error("Unexpected end");
               }
               if (*it == ',') [[likely]] {
                  ++it;
               }
               else if (*it == ']') {
                  ++it;
                  if constexpr (resizeable<T>) {
                     value.resize(i + 1);
                  }
                  return;
               }
               else [[unlikely]] {
                  throw std::runtime_error("Expected ]");
               }
            }
            
            // growing
            if constexpr (emplace_backable<T>) {
               for (size_t i = 0; it < end; ++i) {
                  read<json>::op<Opts>(value.emplace_back(), it, end);
                  skip_ws(it, end);
                  if (*it == ',') [[likely]] {
                     ++it;
                  }
                  else if (*it == ']') {
                     ++it;
                     return;
                  }
                  else [[unlikely]] {
                     throw std::runtime_error("Expected ]");
                  }
               }
            }
            else {
               throw std::runtime_error("Exceeded static array size.");
            }
         }
      };
      
      template <class T> requires array_t<T> &&
      (!emplace_backable<T> &&
       resizeable<T>)
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& value, auto&& it, auto&& end)
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
               read<json>::op<Opts>(buffer.emplace_back(), it, end);
               skip_ws(it, end);
            }
            throw std::runtime_error("Expected ]");
         }
      };

      template <class T> requires glaze_array_t<T> || tuple_t<T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& value, auto&& it, auto&& end)
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
                  read<json>::op<Opts>(value.*std::get<I>(meta_v<T>), it, end);
               }
               else {
                  read<json>::op<Opts>(std::get<I>(value), it, end);
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
         template <auto Opts, class It>
         static void op(auto& value, It&& it, auto&& end)
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
               
               if constexpr (glaze_object_t<T>) {
                  std::string_view key;
                  if constexpr (std::contiguous_iterator<std::decay_t<It>>)
                  {
                     // skip white space and escape characters and find the string
                     skip_ws(it, end);
                     match<'"'>(it, end);
                     auto start = it;
                     bool escaped = false;
                     while (true) {
                        if (it == end) [[unlikely]] {
                           throw std::runtime_error("Expected \"");
                        }
                        if (*it == '\\') [[unlikely]] {
                           escaped = true;
                           it = start;
                           break;
                        }
                        else if (*it == '"') {
                           break;
                        }
                        ++it;
                     }
                     
                     if (escaped) [[unlikely]] {
                        // we dont' optimize this currently because it would increase binary size significantly with the complexity of generating escaped compile time versions of keys
                        static thread_local std::string static_key{};
                        read<json>::op<opening_handled<Opts>()>(static_key, it, end);
                        key = static_key;
                     }
                     else [[likely]] {
                        key = sv{ &*start, static_cast<size_t>(std::distance(start, it)) };
                        ++it;
                     }
                  }
                  else {
                     static thread_local std::string static_key{};
                     read<json>::op(static_key, it, end);
                     key = static_key;
                  }
                  
                  skip_ws(it, end);
                  match<':'>(it, end);
                  
                  static constexpr auto frozen_map = detail::make_map<T>();
                  const auto& member_it = frozen_map.find(frozen::string(key));
                  if (member_it != frozen_map.end()) {
                     std::visit(
                        [&](auto&& member_ptr) {
                           using V = std::decay_t<decltype(member_ptr)>;
                           if constexpr (std::is_member_pointer_v<V>) {
                              read<json>::op<Opts>(value.*member_ptr, it, end);
                           }
                           else {
                              read<json>::op<Opts>(member_ptr(value), it, end);
                           }
                        },
                        member_it->second);
                  }
                  else [[unlikely]] {
                     if constexpr (Opts.error_on_unknown_keys) {
                        throw std::runtime_error("Unknown key: " + std::string(key));
                     }
                     else {
                        skip_object_value(it, end);
                     }
                  }
               }
               else {
                  static thread_local std::string key{};
                  read<json>::op<Opts>(key, it, end);
                  
                  skip_ws(it, end);
                  match<':'>(it, end);
                  
                  if constexpr (std::is_same_v<typename T::key_type,
                                               std::string>) {
                     read<json>::op<Opts>(value[key], it, end);
                  }
                  else {
                     static thread_local typename T::key_type key_value{};
                     read<json>::op<Opts>(key_value, key.begin(), key.end());
                     read<json>::op<Opts>(value[key_value], it, end);
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
         template <auto Opts>
         static void op(auto& value, auto&& it, auto&& end)
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
               read<json>::op<Opts>(*value, it, end);
            }
         }
      };
   }  // namespace detail
   
   template <class T, class Buffer>
   inline void read_json(T& value, Buffer&& buffer) {
      read<opts{}>(value, std::forward<Buffer>(buffer));
   }
   
   template <class T, class Buffer>
   inline auto read_json(Buffer&& buffer) {
      T value{};
      read<opts{}>(value, std::forward<Buffer>(buffer));
      return value;
   }
}  // namespace glaze
