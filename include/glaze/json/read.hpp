// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <iterator>
#include <ranges>
#include <charconv>

#include "fast_float/fast_float.h"
#include "glaze/common.hpp"
#include "glaze/json/validate.hpp"
#include "glaze/format.hpp"

namespace glaze
{
   namespace detail
   {
      template <char c>
      inline void match(auto&& it, auto&& end)
      {
         if (it == end || *it != c) [[unlikely]] {
            static constexpr char b[] = {c, '\0'};
            static constexpr auto error = concat_arrays("Expected:", b);
            throw std::runtime_error(error.data());
         }
         else [[likely]] {
            ++it;
         }
      }

      template <string_literal str>
      inline void match(auto&& it, auto&& end)
      {
         const auto n = static_cast<size_t>(std::distance(it, end));
         if (n < str.size) [[unlikely]] {
            // TODO: compile time generate this message, currently borken with
            // MSVC
            static constexpr auto error = "Unexpected end of buffer. Expected:";
            throw std::runtime_error(error);
         }
         size_t i{};
         // clang and gcc will vectorize this loop
         for (auto* c = str.value; c < str.end(); ++it, ++c) {
            i += *it != *c;
         }
         if (i != 0) [[unlikely]] {
            // TODO: compile time generate this message, currently borken with
            // MSVC
            static constexpr auto error = "Expected: ";
            throw std::runtime_error(error);
         }
      }

      inline void skip_comment(auto&& it, auto&& end)
      {
         match<'/'>(it, end);
         if (it == end) [[unlikely]]
            throw std::runtime_error("Unexpected end, expected comment");
         else if (*it == '/') {
            while (++it != end && *it != '\n')
               ;
         }
         else if (*it == '*') {
            while (++it != end) {
               if (*it == '*') [[unlikely]] {
                  if (++it == end) [[unlikely]]
                     break;
                  else if (*it == '/') [[likely]] {
                     ++it;
                     break;
                  }
               }
            }
         }
         else [[unlikely]]
            throw std::runtime_error("Expected / or * after /");
      }

      inline void skip_ws(auto&& it, auto&& end)
      {
         while (it != end) {
            switch (*it) {
            case ' ':
            case '\f':
            case '\r':
            case '\t':
            case '\v':
            case '\n': {
               ++it;
               continue;
            }
            case '/': {
               skip_comment(it, end);
               continue;
            }
            default:
               break;
            }
            break;  // if not white space break
         }
      }

      inline void skip_string(auto&& it, auto&& end) noexcept
      {
         match<'"'>(it, end);
         while (it < end) {
            if (*it == '"') {
               ++it;
               break;
            }
            else if (*it == '\\')
               if (++it == end) [[unlikely]]
                  break;
            ++it;
         }
      }

      template <char open, char close>
      inline void skip_until_closed(auto&& it, auto&& end)
      {
         match<open>(it, end);
         size_t open_count = 1;
         size_t close_count = 0;
         while (it < end && open_count > close_count) {
            switch (*it) {
            case '/':
               skip_comment(it, end);
               break;
            case '"':
               skip_string(it, end);
               break;
            case open:
               ++open_count;
               ++it;
               break;
            case close:
               ++close_count;
               ++it;
               break;
            default:
               ++it;
            }
         }
      }

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

      inline constexpr bool is_numeric(const auto c) noexcept
      {
         switch (c) {
         case '0':
         case '1':
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
         case '8':
         case '9':
         case '.':
         case '+':
         case '-':
         case 'e':
         case 'E':
            return true;
         }
         return false;
      }
      
      void read_json(bool_t auto&& value, auto&& it, auto&& end)
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

      template <num_t T, class It>
      void read_json(T& value, It&& it, auto&& end)
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
      
      void read_json(str_t auto& value, auto&& it, auto&& end)
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

      void read_json(char_t auto& value, auto&& it, auto&& end)
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

      void read_json(raw_json& value, auto&& it, auto&& end)
      {
         // TODO this will not work for streams where we cant move backward
         auto it_start = it;
         skip_object_value(it, end);
         value.str.clear();
         value.str.insert(value.str.begin(), it_start, it);
      }

      template <class T>
      requires array_t<T> &&
         (emplace_backable<T> ||
          !resizeable<T>)void read_json(T& value, auto&& it, auto&& end);

      template <class T, class It>
      requires array_t<T> &&
         (!emplace_backable<T> &&
          resizeable<T>)void read_json(T& value, auto&& it, auto&& end);

      template <size_t I = 0, class T>
      requires glaze_array_t<T> || tuple_t<T>
      void read_json(T& value, auto&& it, auto&& end);

      template <class T>
      requires map_t<T> || glaze_object_t<T>
      void read_json(T& value, auto&& it, auto&& end);

      template <nullable_t T>
      void read_json(T& value, auto&& it, auto&& end);

      template <custom_t T>
      void read_json(T& value, auto&& it, auto&& end)
      {
         custom<T>::read_json(value, it, end);
      }

      template <class T>
      requires array_t<T> &&
         (emplace_backable<T> ||
          !resizeable<T>)void read_json(T& value, auto&& it, auto&& end)
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
               read_json(*value_it++, it, end);
            }
            else {
               if constexpr (emplace_backable<T>) {
                  read_json(value.emplace_back(), it, end);
               }
               else {
                  throw std::runtime_error("Exceeded static array size.");
               }
            }
            skip_ws(it, end);
         }
         throw std::runtime_error("Expected ]");
      }

      template <class T>
      requires array_t<T> &&
         (!emplace_backable<T> &&
          resizeable<T>)void read_json(T& value, auto&& it, auto&& end)
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
            read_json(buffer.emplace_back(), it, end);
            skip_ws(it, end);
         }
         throw std::runtime_error("Expected ]");
      }

      template <size_t I, class T>
      requires glaze_array_t<T> || tuple_t<T>
      void read_json(T& value, auto&& it, auto&& end)
      {
         constexpr auto n = []() constexpr
         {
            if constexpr (glaze_array_t<T>) {
               return std::tuple_size_v<meta_t<T>>;
            }
            else {
               return std::tuple_size_v<T>;
            }
         }
         ();

         if constexpr (I == 0) {
            skip_ws(it, end);
            match<'['>(it, end);
            skip_ws(it, end);
         }
         if constexpr (I < n) {
            if constexpr (I != 0) {
               match<','>(it, end);
            }
            if constexpr (glaze_array_t<T>) {
               read_json(value.*std::get<I>(meta_v<T>), it, end);
            }
            else {
               read_json(std::get<I>(value), it, end);
            }
            skip_ws(it, end);
            read_json<I + 1>(value, it, end);
         }
         if constexpr (I == 0) {
            if constexpr (n == 0) {
               skip_ws(it, end);
            }
            match<']'>(it, end);
         }
      }

      template <class T>
      requires map_t<T> || glaze_object_t<T>
      void read_json(T& value, auto&& it, auto&& end)
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
            read_json(key, it, end);
            skip_ws(it, end);
            match<':'>(it, end);
            if constexpr (glaze_object_t<T>) {
               static constexpr auto frozen_map = detail::make_map<T>();
               const auto& member_it = frozen_map.find(frozen::string(key));
               if (member_it != frozen_map.end()) {
                  std::visit(
                     [&](auto&& member_ptr) {
                        read_json(value.*member_ptr, it, end);
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
                  read_json(value[key], it, end);
               }
               else {
                  static thread_local typename T::key_type key_value{};
                  read_json(key_value, key.begin(), key.end());
                  read_json(value[key_value], it, end);
               }
            }
            skip_ws(it, end);
         }
         throw std::runtime_error("Expected }");
      }

      template <nullable_t T>
      void read_json(T& value, auto&& it, auto&& end)
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
               if constexpr (is_specialization<T, std::optional>::value)
                  value = std::make_optional<typename T::value_type>();
               else if constexpr (is_specialization<T, std::unique_ptr>::value)
                  value = std::make_unique<typename T::element_type>();
               else if constexpr (is_specialization<T, std::shared_ptr>::value)
                  value = std::make_shared<typename T::element_type>();
               else
                  throw std::runtime_error(
                     "Cannot read into unset nullable that is not "
                     "std::optional, std::unique_ptr, or std::shared_ptr");
            }
            read_json(*value, it, end);
         }
      }
      
      template <>
      struct read<json>
      {
         template <class... Args>
         void operator()(Args&&... args) {
            read_json(std::forward<Args>(args)...);
         }
      };
   }  // namespace detail

   // For reading json from a std::vector<char>, std::deque<char> and the like
   template <uint32_t Format, class T, class Buffer>
   requires nano::ranges::input_range<std::decay_t<Buffer>> &&
      std::same_as<char, nano::ranges::range_value_t<std::decay_t<Buffer>>>
   inline void read(T& value, Buffer&& buffer)
   {
      auto b = std::ranges::begin(buffer);
      auto e = std::ranges::end(buffer);
      if (b == e) {
         throw std::runtime_error("No input provided to read_json");
      }
      try {
         detail::read<Format>{}(value, b, e);
      }
      catch (const std::exception& e) {
         auto index = std::distance(std::ranges::begin(buffer), b);
         auto info = detail::get_source_info(buffer, index);
         std::string error = e.what();
         if (info) error = detail::generate_error_string(error, *info);
         throw std::runtime_error(error);
      }
   }

   // For reading json from std::ofstream, std::cout, or other streams
   template <uint32_t Format, class T>
   inline void read(T& value, detail::stream_t auto& is)
   {
      std::istreambuf_iterator<char> b{is}, e{};
      if (b == e) {
         throw std::runtime_error("No input provided to read_json");
      }
      detail::read<Format>{}(value, b, e);
   }

   // For reading json from stuff convertable to a std::string_view
   template <uint32_t Format, class T, class Buffer>
   requires(std::convertible_to<std::decay_t<Buffer>, std::string_view> &&
            !nano::ranges::input_range<
               std::decay_t<Buffer>>) inline void read(T& value,
                                                            Buffer&& buffer)
   {
      const auto str = std::string_view{std::forward<Buffer>(buffer)};
      if (str.empty()) {
         throw std::runtime_error("No input provided to read_json");
      }
      read<Format>(value, str);
   }
   
   template <class T, class Buffer>
   inline void read_json(T&& value, Buffer&& buffer) {
      read<json>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

}  // namespace glaze
