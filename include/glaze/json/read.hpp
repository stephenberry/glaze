// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <iterator>
#include <ranges>
#include <charconv>
#include <sstream>

#include "glaze/core/read.hpp"
#include "glaze/core/format.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/parse.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/strod.hpp"
#include "glaze/json/json_t.hpp"

namespace glz
{
   namespace detail
   {
      // Unless we can mutate the input buffer we need somewhere to store escaped strings for key lookup and such
      // Could put this in the context but tls overhead isnt that bad. Will need to figure out when heap allocations are not allowed or restricted
      GLZ_ALWAYS_INLINE std::string& string_buffer() noexcept {
         static thread_local std::string buffer(128, ' ');
         return buffer;
      }

      template <class T = void>
      struct from_json {};
      
      template <>
      struct read<json>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept {
            from_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <glaze_value_t T>
      struct from_json<T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept {
            using V = std::decay_t<decltype(get_member(std::declval<T>(), meta_wrapper_v<T>))>;
            from_json<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <is_member_function_pointer T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
         {
            ctx.error = error_code::attempt_member_func_read;
         }
      };
      
      template <>
      struct from_json<skip>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
         {
            // skip_ws called by skip_value
            skip_value<Opts>(ctx, args...);
         }
      };
      
      template <is_reference_wrapper T>
      struct from_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args) noexcept
         {
            using V = std::decay_t<decltype(value.get())>;
            from_json<V>::template op<Opts>(value.get(), std::forward<Args>(args)...);
         }
      };
      
      template <>
      struct from_json<hidden>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
         {
            ctx.error = error_code::attempt_read_hidden;
         }
      };
      
      template <>
      struct from_json<std::monostate>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, auto&&... args) noexcept
         {
            skip_ws(args...);
            match<R"("std::monostate")">(args...);
         }
      };
      
      template <bool_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(bool_t auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            
            switch (*it) {
               case 't': {
                  ++it;
                  match<"rue">(ctx, it, end);
                  value = true;
                  break;
               }
               case 'f': {
                  ++it;
                  match<"alse">(ctx, it, end);
                  value = false;
                  break;
               }
               [[unlikely]] default
               :  {
                  ctx.error = error_code::expected_true_or_false;
                  return;
            }
         }
         }
      };
      
      template <num_t T>
      struct from_json<T>
      {
         template <auto Options, class It>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end) noexcept
         {
            if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
            
            skip_ws<Options>(ctx, it, end);
            
            // TODO: fix this
            using X = std::conditional_t<std::is_const_v<std::remove_pointer_t<std::remove_reference_t<decltype(it)>>>, const uint8_t*, uint8_t*>;
            auto cur = reinterpret_cast<X>(it);
            auto s = parse_number<std::decay_t<T>, Options.force_conformance>(value, cur);
            if (!s) [[unlikely]] {
               ctx.error = error_code::parse_number_failure;
               return;
            }
            it = reinterpret_cast<std::remove_reference_t<decltype(it)>>(cur);
         }
      };

      template <str_t T>
      struct from_json<T>
      {
         template <auto Opts, class It, class End>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, It&& it, End&& end) noexcept
         {
            if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
            
            if constexpr (!Opts.opening_handled) {
               skip_ws<Opts>(ctx, it, end);
               match<'"'>(ctx, it);
            }
            
            // overwrite portion

            auto handle_escaped = [&]() {
               switch (*it) {
               case '"':
               case '\\':
               case '/':
                  value.push_back(*it);
                  ++it;
                  break;
               case 'b':
                  value.push_back('\b');
                  ++it;
                  break;
               case 'f':
                  value.push_back('\f');
                  ++it;
                  break;
               case 'n':
                  value.push_back('\n');
                  ++it;
                  break;
               case 'r':
                  value.push_back('\r');
                  ++it;
                  break;
               case 't':
                  value.push_back('\t');
                  ++it;
                  break;
               case 'u': {
                  // TODO: this is slow
                  ++it;
                  if (std::distance(it, end) < 4) [[unlikely]] {
                     ctx.error = error_code::u_requires_hex_digits; // \\u should be followed by 4 hex digits.
                     return;
                  }
                  uint32_t codepoint_integer;
                  std::stringstream ss;
                  ss << std::hex << sv{ it, 4 };
                  ss >> codepoint_integer;
                  
                  //auto [ptr, ec] = std::from_chars(it, it + 4, codepoint_double, std::chars_format::hex);
                  //if (ec != std::errc() || ptr - it != 4) {
                  //   ctx.error = error_code::u_requires_hex_digits;
                  //   return;
                  //}
                  
                  char32_t codepoint = codepoint_integer;
                  char8_t buffer[4];
                  auto& f = std::use_facet<std::codecvt<char32_t, char8_t, mbstate_t>>(std::locale());
                  std::mbstate_t mb{};
                  const char32_t* from_next;
                  char8_t* to_next;
                  const auto result = f.out(mb, &codepoint, &codepoint + 1, from_next, buffer, buffer + 4, to_next);
                  if (result == std::codecvt_base::noconv) {
                     std::memcpy(buffer, &codepoint, 4);
                  }
                  else if (result != std::codecvt_base::ok) {
                     ctx.error = error_code::unicode_escape_conversion_failure;
                     return;
                  }
                  value.append(reinterpret_cast<char*>(buffer), to_next - buffer);
                  std::advance(it, 4);
                  break;
               }
                  default: {
                     ctx.error = error_code::invalid_escape;
                     return;
               }
               }
            };
            
            // growth portion
            value.clear(); // Single append on unescaped strings so overwrite opt isnt as important
            auto start = it;
            while (it < end) {
               if constexpr (!Opts.force_conformance) {
                  skip_till_escape_or_quote(ctx, it, end);
                  if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
               
                  if (*it == '"') {
                     value.append(start, static_cast<size_t>(it - start));
                     ++it;
                     return;
                  }
                  else {
                     value.append(start, static_cast<size_t>(it - start));
                     ++it;
                     handle_escaped();
                     start = it;
                  }
               }
               else {
                  switch (*it) {
                     case '"': {
                        value.append(start, static_cast<size_t>(it - start));
                        ++it;
                        return;
                     }
                     case '\b':
                     case '\f':
                     case '\n':
                     case '\r':
                     case '\t': {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     case '\0': {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                     case '\\': {
                        value.append(start, static_cast<size_t>(it - start));
                        ++it;
                        handle_escaped();
                        start = it;
                        break;
                     }
                     default:
                        ++it;
                  }
               }
            }
         }
      };
      
      template <char_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
            
            skip_ws<Opts>(ctx, it, end);
            
            match<'"'>(ctx, it);
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
               case 'n' :
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
               case 'u':
                  {
                     //TODO: this is slow
                     ++it;
                     if (std::distance(it, end) < 4) [[unlikely]] {
                        ctx.error = error_code::u_requires_hex_digits;
                        return;
                     }
                     //double codepoint_double;
                     //auto [ptr, ec] = from_chars(it, it + 4, codepoint_double, std::chars_format::hex);
                     //if (ec != std::errc() || ptr - it != 4) {
                     //   ctx.error = error_code::u_requires_hex_digits;
                     //   return;
                     //}
                     //char32_t codepoint = static_cast<uint32_t>(codepoint_double);
                     
                     uint32_t codepoint_integer;
                     std::stringstream ss;
                     ss << std::hex << sv{ it, 4 };
                     ss >> codepoint_integer;
                     
                     char32_t codepoint = codepoint_integer;
                     
                     if constexpr (std::is_same_v<T, char32_t>) {
                        value = codepoint;
                     }
                     else {
                        char8_t buffer[4];
                        auto& f = std::use_facet<std::codecvt<char32_t, char8_t, mbstate_t>>(std::locale());
                        std::mbstate_t mb{};
                        const char32_t* from_next;
                        char8_t* to_next;
                        auto result = f.out(mb, &codepoint, &codepoint + 1, from_next, buffer, buffer + 4, to_next);
                        if (result != std::codecvt_base::ok) {
                           ctx.error = error_code::unicode_escape_conversion_failure;
                           return;
                        }
                        
                        const auto n = to_next - buffer;
                        
                        if constexpr (sizeof(T) == 1) {
                           std::memcpy(&value, buffer, 1);
                        }
                        else {
                           using buffer_type = std::conditional_t<std::is_same_v<T, wchar_t>, char, char8_t>;
                           auto& facet = std::use_facet<std::codecvt<T, buffer_type, mbstate_t>>(std::locale());
                           const buffer_type* from_next_final;
                           T* to_next_final;
                           auto* rbuf = reinterpret_cast<buffer_type*>(buffer);
                           result =
                              facet.in(mb, rbuf, rbuf + n, from_next_final, &value, &value + 1, to_next_final);
                           if (result != std::codecvt_base::ok) {
                              ctx.error = error_code::unicode_escape_conversion_failure;
                              return;
                           }
                        }
                     }
                     
                     std::advance(it, 4);
                     break;
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
            match<'"'>(ctx, it);
         }
      };

      template <glaze_enum_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {            
            skip_ws<Opts>(ctx, it, end);
            const auto key = parse_key(ctx, it, end);
            
            if (key) {
            static constexpr auto frozen_map = detail::make_string_to_enum_map<T>();
               const auto& member_it = frozen_map.find(frozen::string(*key));
            if (member_it != frozen_map.end()) {
               value = member_it->second;
            }
            else [[unlikely]] {
                  ctx.error = error_code::unexpected_enum;
            }
         }
         }
      };

      template <func_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto& /*value*/, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            match<'"'>(ctx, it);
            skip_till_quote(ctx, it, end);
            match<'"'>(ctx, it);
         }
      };
      
      template <>
      struct from_json<raw_json>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(raw_json& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            auto it_start = it;
            skip_value<Opts>(ctx, it, end);
            value.str.clear();
            value.str.insert(value.str.begin(), it_start, it);
         }
      };
      
      // for set types
      template <class T> requires (array_t<T> && !emplace_backable<T> && !resizeable<T> && emplaceable<T>)
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            match<'['>(ctx, it);
            skip_ws<Opts>(ctx, it, end);
            
            value.clear();
            
            while (true) {
               using V = typename T::value_type;
               if constexpr (sizeof(V) > 8) {
                  static thread_local V v;
                  read<json>::op<Opts>(v, ctx, it, end);
                  value.emplace(v);
               }
               else {
                  V v;
                  read<json>::op<Opts>(v, ctx, it, end);
                  value.emplace(std::move(v));
               }
               skip_ws<Opts>(ctx, it, end);
               if (*it == ']') {
                  ++it;
                  return;
               }
               match<','>(ctx, it);
            }
         }
      };

      template <class T> requires (array_t<T> &&
      (emplace_backable<T> ||
       !resizeable<T>) && !emplaceable<T>)
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            match<'['>(ctx, it);
            skip_ws<Opts>(ctx, it, end);
            if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
            
            if (*it == ']') [[unlikely]] {
               ++it;
               if constexpr (resizeable<T>) {
                  value.clear();
                  
                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }
               return;
            }
            
            const auto n = value.size();
            
            auto value_it = value.begin();
            
            for (size_t i = 0; i < n; ++i) {
               read<json>::op<Opts>(*value_it++, ctx, it, end);
               skip_ws<Opts>(ctx, it, end);
               if (*it == ',') [[likely]] {
                  ++it;
                  skip_ws<Opts>(ctx, it, end);
               }
               else if (*it == ']') {
                  ++it;
                  if constexpr (resizeable<T>) {
                     value.resize(i + 1);
                     
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
            
            // growing
            if constexpr (emplace_backable<T>) {
               while (it < end) {
                  read<json>::op<Opts>(value.emplace_back(), ctx, it, end);
                  skip_ws<Opts>(ctx, it, end);
                  if (*it == ',') [[likely]] {
                     ++it;
                     skip_ws<Opts>(ctx, it, end);
                  }
                  else if (*it == ']') {
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
      };
      
      // counts the number of JSON array elements
      // needed for classes that are resizable, but do not have an emplace_back
      // it is copied so that it does not actually progress the iterator
      // expects the opening brace ([) to have already been consumed
      template <auto Opts>
      [[nodiscard]] GLZ_ALWAYS_INLINE expected<size_t, error_code> number_of_array_elements(is_context auto&& ctx, auto it, auto&& end) noexcept
      {
         skip_ws<Opts>(ctx, it, end);
         if (static_cast<bool>(ctx.error)) [[unlikely]] { return unexpected(ctx.error); }
         
         if (*it == ']') [[unlikely]] {
            return 0;
         }
         size_t count = 1;
         while (true) {
            switch (*it)
            {
               case ',': {
                  ++count;
                  ++it;
                  break;
               }
               case '/': {
                  skip_ws<Opts>(ctx, it, end);
                  break;
               }
               case '"': {
                  skip_string<Opts>(ctx, it, end);
                  break;
               }
               case ']': {
                  return count;
               }
               case '\0': {
                  return unexpected(error_code::unexpected_end);
               }
               default:
                  ++it;
            }
         }
         return unexpected(error_code::syntax_error); // should never be reached
      }
      
      template <class T> requires array_t<T> &&
      (!emplace_backable<T> &&
       resizeable<T>)
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            match<'['>(ctx, it);
            
            const auto n = number_of_array_elements<Opts>(ctx, it, end);
            if (n) {
               value.resize(*n);
            size_t i = 0;
            for (auto& x : value) {
               read<json>::op<Opts>(x, ctx, it, end);
                  skip_ws<Opts>(ctx, it, end);
                  if (i < *n - 1) {
                     match<','>(ctx, it);
               }
               ++i;
            }
            match<']'>(ctx, it);
         }
         }
      };

      template <class T>
      requires glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
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
            
            skip_ws<Opts>(ctx, it, end);
            match<'['>(ctx, it);
            skip_ws<Opts>(ctx, it, end);
            
            for_each<N>([&](auto I) {
               if (*it == ']') {
                  return;
               }
               if constexpr (I != 0) {
                  match<','>(ctx, it);
                  skip_ws<Opts>(ctx, it, end);
               }
               if constexpr (is_std_tuple<T>) {
                  read<json>::op<Opts>(std::get<I>(value), ctx, it, end);
               }
               else if constexpr (glaze_array_t<T>) {
                  read<json>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<T>)), ctx, it, end);
               }
               else {
                  read<json>::op<Opts>(glz::tuplet::get<I>(value), ctx, it, end);
               }
               skip_ws<Opts>(ctx, it, end);
            });
            
            match<']'>(ctx, it);
         }
      };
      
      template <glaze_flags_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            match<'['>(ctx, it);
            
            std::string& s = string_buffer();
            
            static constexpr auto flag_map = make_map<T>();
            
            while (true) {
               read<json>::op<Opts>(s, ctx, it, end);
               
               auto itr = flag_map.find(s);
               if (itr != flag_map.end()) {
                  std::visit([&](auto&& x) {
                     get_member(value, x) = true;
                  }, itr->second);
               }
               else {
                  ctx.error = error_code::invalid_flag_input;
                  return;
               }
               
               skip_ws<Opts>(ctx, it, end);
               if (*it == ']') {
                  ++it;
                  return;
               }
               match<','>(ctx, it);
            }
         }
      };
      
      template <class T>
      struct from_json<includer<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            std::string& path = string_buffer();
            read<json>::op<Opts>(path, ctx, it, end);
            
            const auto file_path = relativize_if_not_absolute(std::filesystem::path(ctx.current_file).parent_path(), std::filesystem::path{ path });
               
            std::string& buffer = string_buffer();
            std::string string_file_path = file_path.string();
            const auto ec = file_to_buffer(buffer, string_file_path);
                  
            if (static_cast<bool>(ec)) {
               ctx.error = ec;
               return;
            }
            
            const auto current_file = ctx.current_file;
            ctx.current_file = file_path.string();
                  
            std::ignore = glz::read<Opts>(value.value, buffer, ctx);
                  
            ctx.current_file = current_file;
         }
      };
      
      template <glaze_object_t T>
      GLZ_ALWAYS_INLINE constexpr bool keys_may_contain_escape() noexcept
      {
         auto is_unicode = [](const auto c) {
            return (static_cast<uint8_t>(c) >> 7) > 0;
         };
         
         bool may_escape = false;
         constexpr auto N = std::tuple_size_v<meta_t<T>>;
         for_each<N>([&](auto I) {
            constexpr auto s = [] {
               return glz::tuplet::get<0>(glz::tuplet::get<decltype(I)::value>(meta_v<T>));
            }(); // MSVC internal compiler error workaround
            for (auto& c : s) {
               if (c == '\\' || c == '"' || is_unicode(c)) {
                  may_escape = true;
                  return;
               }
            }
         });
         
         return may_escape;
      }

      template <is_variant T>
      GLZ_ALWAYS_INLINE constexpr bool keys_may_contain_escape() noexcept
      {
         bool may_escape = false;
         constexpr auto N = std::variant_size_v<T>;
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            if constexpr (glaze_object_t<V>) {
               if constexpr (keys_may_contain_escape<V>()) {
                  may_escape = true;
                  return;
               }
            }
         });
         return may_escape;
      }
      
      struct key_stats_t
      {
         uint32_t min_length = (std::numeric_limits<uint32_t>::max)();
         uint32_t max_length{};
         uint32_t length_range{};
      };
      
      // only use this if the keys cannot contain escape characters
      template <glaze_object_t T, string_literal tag = "">
      GLZ_ALWAYS_INLINE constexpr auto key_stats() noexcept
      {
         key_stats_t stats{};
         if constexpr (!tag.sv().empty()) {
            constexpr auto tag_size = tag.sv().size();
            stats.max_length = tag_size;
            stats.min_length = tag_size;
         }
         
         constexpr auto N = std::tuple_size_v<meta_t<T>>;
         for_each<N>([&](auto I) {
            constexpr auto s = [] {
               return glz::tuplet::get<0>(glz::tuplet::get<decltype(I)::value>(meta_v<T>));
            }(); // MSVC internal compiler error workaround
            const auto n = s.size();
            if (n < stats.min_length) {
               stats.min_length = n;
            }
            if (n > stats.max_length) {
               stats.max_length = n;
            }
         });
         
         stats.length_range = stats.max_length - stats.min_length;
         
         return stats;
      }

      template <is_variant T, string_literal tag = "">
      GLZ_ALWAYS_INLINE constexpr auto key_stats() noexcept
      {
         key_stats_t stats{};
         if constexpr (!tag.sv().empty()) {
            constexpr auto tag_size = tag.sv().size();
            stats.max_length = tag_size;
            stats.min_length = tag_size;
         }

         constexpr auto N = std::variant_size_v<T>;
         for_each<N>([&](auto I) {
            using V = std::decay_t<std::variant_alternative_t<I, T>>;
            if constexpr (glaze_object_t<V>) {
               constexpr auto substats = key_stats<V>();
               if (substats.min_length < stats.min_length) {
                  stats.min_length = substats.min_length;
               }
               if (substats.max_length > stats.max_length) {
                  stats.max_length = substats.max_length;
               }
            }
         });

         stats.length_range = stats.max_length - stats.min_length;

         return stats;
      }

      // Key parsing for meta objects or variants of meta objects.
      // TODO We could expand this to compiletime known strings in general like enums
      template <class T, auto Opts, string_literal tag = "">
      GLZ_ALWAYS_INLINE std::string_view parse_object_key(is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         if (static_cast<bool>(ctx.error)) [[unlikely]] {
            return {};
         }

         // skip white space and escape characters and find the string
         skip_ws<Opts>(ctx, it, end);
         match<'"'>(ctx, it);

         if constexpr (keys_may_contain_escape<T>()) {
            auto start = it;
            
            skip_till_escape_or_quote(ctx, it, end);
            if (static_cast<bool>(ctx.error)) [[unlikely]] {
               return {};
            }
            if (*it == '\\') [[unlikely]] {
               // we dont' optimize this currently because it would increase binary size significantly with the
               // complexity of generating escaped compile time versions of keys
               it = start;
               std::string& static_key = string_buffer();
               read<json>::op<opening_handled<Opts>()>(static_key, ctx, it, end);
               return static_key;
            }
            else [[likely]] {
               const sv key{start, static_cast<size_t>(it - start)};
               ++it;
               return key;
            }
         }
         else {
            static constexpr auto stats = key_stats<T, tag>();
            if constexpr (stats.length_range < 8 && Opts.error_on_unknown_keys) {
               if ((it + stats.max_length) < end) [[likely]] {
                  if constexpr (stats.length_range == 0) {
                     const sv key{it, stats.max_length};
                     it += stats.max_length;
                     match<'"'>(ctx, it);
                     return key;
                  }
                  else if constexpr (stats.length_range < 4) {
                     auto start = it;
                     it += stats.min_length;
                     for (uint32_t i = 0; i <= stats.length_range; ++it, ++i) {
                        if (*it == '"') {
                           const sv key{start, static_cast<size_t>(it - start)};
                           ++it;
                           return key;
                        }
                     }
                     ctx.error = error_code::key_not_found;
                     return {};
                  }
                  else {
                     return parse_key_cx<stats.min_length, stats.length_range>(ctx, it);
                  }
               }
               else [[unlikely]] {
                  return parse_unescaped_key(ctx, it, end);
               }
            }
            else {
               return parse_unescaped_key(ctx, it, end);
            }
         }
      }
      
      template <class T>
      requires map_t<T> || glaze_object_t<T>
      struct from_json<T>
      {
         template <auto Options, string_literal tag = "">
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (!Options.opening_handled) {
               skip_ws<Options>(ctx, it, end);
               match<'{'>(ctx, it);
            }
            
            skip_ws<Options>(ctx, it, end);
            
            static constexpr auto Opts = opening_handled_off<Options>();
            
            bool first = true;
            while (true) {
               if (*it == '}') [[unlikely]] {
                  ++it;
                  return;
               }
               else if (first) [[unlikely]]
                  first = false;
               else [[likely]] {
                  match<','>(ctx, it);
                  skip_ws<Opts>(ctx, it, end);
               }
               
               if constexpr (glaze_object_t<T>) {
                  const sv key = parse_object_key<T, Opts, tag>(ctx, it, end);
                  
                  skip_ws<Opts>(ctx, it, end);
                  match<':'>(ctx, it);
                  
                  if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
                  
                  static constexpr auto frozen_map = detail::make_map<T, Opts.allow_hash_check>();
                  const auto& member_it = frozen_map.find(key);
                  if (member_it != frozen_map.end()) [[likely]] {
                     std::visit(
                        [&](auto&& member_ptr) {
                           read<json>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
                        },
                        member_it->second);
                  }
                  else [[unlikely]] {
                     if constexpr (Opts.error_on_unknown_keys) {
                        if constexpr (tag.sv().empty()) {
                           ctx.error = error_code::unknown_key;
                           return;
                        }
                        else if (key != tag.sv()) {
                           ctx.error = error_code::unknown_key;
                           return;
                        }
                        else {
                           skip_value<Opts>(ctx, it, end);
                        }
                     }
                     else {
                        skip_value<Opts>(ctx, it, end);
                     }
                  }
               }
               else {
                  std::string& key = string_buffer();
                  read<json>::op<Opts>(key, ctx, it, end);
                  
                  skip_ws<Opts>(ctx, it, end);
                  match<':'>(ctx, it);
                  
                  if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
                  
                  if constexpr (std::is_same_v<typename T::key_type,
                                               std::string>) {
                     read<json>::op<Opts>(value[key], ctx, it, end);
                  }
                  else {
                     static thread_local typename T::key_type key_value{};
                     read<json>::op<Opts>(key_value, ctx, key.data(), key.data() + key.size());
                     read<json>::op<Opts>(value[key_value], ctx, it, end);
                  }
               }
               skip_ws<Opts>(ctx, it, end);
            }
            
            if (static_cast<bool>(ctx.error)) [[unlikely]] { return; }
            ctx.error = error_code::expected_bracket;
         }
      };

       template <is_variant T>
       GLZ_ALWAYS_INLINE constexpr auto variant_is_auto_deducible() noexcept
       {
          //Contains at most one each of the basic json types bool, numeric, string, object, array
          //If all objects are meta objects then we can attemt to deduce them as well either through a type tag or unique combinations of keys
          int bools{}, numbers{}, strings{}, objects{}, meta_objects{}, arrays{};
          constexpr auto N = std::variant_size_v<T>;
          for_each<N>([&](auto I) {
             using V = std::decay_t<std::variant_alternative_t<I, T>>;
             if constexpr (bool_t<V>) ++bools;
             else if constexpr (num_t<V>) ++numbers;
             else if constexpr (str_t<V> || glaze_enum_t<V>) ++strings;
             else if constexpr (map_t<V>) ++objects;
             else if constexpr (map_t<V>) {
                ++objects;
                ++meta_objects;
             }
             else if constexpr (array_t<V> || glaze_array_t<V>) ++arrays;
          });
          return bools < 2 && numbers < 2 && strings < 2 && (objects < 2 || meta_objects == objects) && arrays < 2;
       }

       template <typename>
       struct variant_types;

       template <typename... Ts>
       struct variant_types<std::variant<Ts...>>
       {
          //TODO this way of filtering types is compile time intensive.
          using bool_types = decltype(std::tuple_cat(std::conditional_t<bool_t<Ts>,std::tuple<Ts>,std::tuple<>>{}...));
          using number_types = decltype(std::tuple_cat(std::conditional_t<num_t<Ts>,std::tuple<Ts>,std::tuple<>>{}...));
          using string_types = decltype(std::tuple_cat(std::conditional_t<str_t<Ts> || glaze_enum_t<Ts>, std::tuple<Ts>, std::tuple<>>{}...));
          using object_types = decltype(std::tuple_cat(std::conditional_t<map_t<Ts> || glaze_object_t<Ts>, std::tuple<Ts>, std::tuple<>>{}...));
          using array_types = decltype(std::tuple_cat(std::conditional_t<array_t<Ts> || glaze_array_t<Ts>, std::tuple<Ts>, std::tuple<>>{}...));
          using nullable_types = decltype(std::tuple_cat(std::conditional_t<nullable_t<Ts>, std::tuple<Ts>, std::tuple<>>{}...));
       };
       
       template <is_variant T>
       struct from_json<T>
       {
          // Note that items in the variant are required to be default constructible for us to switch types
          template <auto Opts>
          GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
          {
             if constexpr (variant_is_auto_deducible<T>()) {
                skip_ws<Opts>(ctx, it, end);
                switch (*it) {
                   case '\0':
                      ctx.error = error_code::unexpected_end;
                      return;
                   case '{':
                      ++it;
                      using object_types = typename variant_types<T>::object_types;
                      if constexpr (std::tuple_size_v<object_types> < 1) {
                         ctx.error = error_code::no_matching_variant_type;
                         return;
                      }
                      else if constexpr (std::tuple_size_v<object_types> == 1) {
                         using V = std::tuple_element_t<0, object_types>;
                         if (!std::holds_alternative<V>(value)) value = V{};
                         read<json>::op<opening_handled<Opts>()>(std::get<V>(value), ctx, it, end);
                      }
                      else {
                         auto possible_types = bit_array<std::variant_size_v<T>>{}.flip();
                         static constexpr auto deduction_map = glz::detail::make_variant_deduction_map<T>();
                         static constexpr auto tag_literal = string_literal_from_view<tag_v<T>.size()>(tag_v<T>);
                         skip_ws<Opts>(ctx, it, end);
                         auto start = it;
                         while (*it != '}') {
                            if (it != start) {
                               match<','>(ctx, it);
                            }
                            std::string_view key = parse_object_key<T, Opts, tag_literal>(ctx, it, end);
                            auto deduction_it = deduction_map.find(key);
                            if (deduction_it != deduction_map.end()) [[likely]] {
                               possible_types &= deduction_it->second;
                            }
                            else [[unlikely]] {
                               if constexpr (!tag_v<T>.empty()) {
                                  if (key == tag_v<T>) {
                                     skip_ws<Opts>(ctx, it, end);
                                     match<':'>(ctx, it);

                                     std::string& type_id = string_buffer();
                                     read<json>::op<Opts>(type_id, ctx, it, end);
                                     skip_ws<Opts>(ctx, it, end);
                                     match<','>(ctx, it);

                                     static constexpr auto id_map = make_variant_id_map<T>();
                                     auto id_it = id_map.find(std::string_view{type_id});
                                     if (id_it != id_map.end()) [[likely]] {
                                        it = start;
                                        const auto type_index = id_it->second;
                                        if (value.index() != type_index) value = runtime_variant_map<T>()[type_index];
                                        std::visit(
                                           [&](auto&& v) {
                                              using V = std::decay_t<decltype(v)>;
                                              if constexpr (glaze_object_t<V>) {
                                                  from_json<V>::template op<opening_handled<Opts>(), tag_literal>(v, ctx, it, end);
                                              }
                                           },
                                           value);
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
                            }

                            auto matching_types = possible_types.popcount();
                            if (matching_types == 0) {
                               ctx.error = error_code::no_matching_variant_type;
                               return;
                            }
                            else if (matching_types == 1) {
                               it = start;
                               const auto type_index = possible_types.countr_zero();
                               if (value.index() != static_cast<size_t>(type_index)) value = runtime_variant_map<T>()[type_index];
                               std::visit(
                                  [&](auto&& v) {
                                     using V = std::decay_t<decltype(v)>;
                                     if constexpr (glaze_object_t<V>) {
                                        from_json<V>::template op<opening_handled<Opts>(), tag_literal>(v, ctx, it, end);
                                     }
                                  },
                                  value);
                               return;
                            }
                            skip_ws<Opts>(ctx, it, end);
                            match<':'>(ctx, it);
                            skip_ws<Opts>(ctx, it, end);
                            skip_value<Opts>(ctx, it, end);
                            skip_ws<Opts>(ctx, it, end);
                         }
                         ctx.error = error_code::no_matching_variant_type;
                         return;
                      }
                      break;
                   case '[':
                      using array_types = typename variant_types<T>::array_types;
                      if constexpr (std::tuple_size_v<array_types> < 1) {
                         ctx.error = error_code::no_matching_variant_type;
                         return;
                      }
                      else {
                         using V = std::tuple_element_t<0, array_types>;
                         if (!std::holds_alternative<V>(value)) value = V{};
                         read<json>::op<Opts>(std::get<V>(value), ctx, it, end);
                      }
                      break;
                   case '"': {
                      using string_types = typename variant_types<T>::string_types;
                      if constexpr (std::tuple_size_v<string_types> < 1) {
                         ctx.error = error_code::no_matching_variant_type;
                         return;
                      }
                      else {
                         using V = std::tuple_element_t<0, string_types>;
                         if (!std::holds_alternative<V>(value)) value = V{};
                         read<json>::op<Opts>(std::get<V>(value), ctx, it, end);
                      }
                      break;
                   }
                   case 't':
                   case 'f': {
                      using bool_types = typename variant_types<T>::bool_types;
                      if constexpr (std::tuple_size_v<bool_types> < 1) {
                         ctx.error = error_code::no_matching_variant_type;
                         return;
                      }
                      else {
                         using V = std::tuple_element_t<0, bool_types>;
                         if (!std::holds_alternative<V>(value)) value = V{};
                         read<json>::op<Opts>(std::get<V>(value), ctx, it, end);
                      }
                      break;
                   }
                   case 'n':
                      using nullable_types = typename variant_types<T>::nullable_types;
                      if constexpr (std::tuple_size_v<nullable_types> < 1) {
                         ctx.error = error_code::no_matching_variant_type;
                         return;
                      }
                      else {
                         using V = std::tuple_element_t<0, nullable_types>;
                         if (!std::holds_alternative<V>(value)) value = V{};
                         match<"null">(ctx, it, end);
                      }
                      break;
                   default: {
                      // Not bool, string, object, or array so must be number or null
                      using number_types = typename variant_types<T>::number_types;
                      if constexpr (std::tuple_size_v<number_types> < 1) {
                         ctx.error = error_code::no_matching_variant_type;
                         return;
                      }
                      else {
                         using V = std::tuple_element_t<0, number_types>;
                         if (!std::holds_alternative<V>(value)) value = V{};
                         read<json>::op<Opts>(std::get<V>(value), ctx, it, end);
                      }
                   }
                }
             }
             else {
                std::visit(
                   [&](auto&& v) {
                      read<json>::op<Opts>(v, ctx, it, end);
                   },
                   value);
             }
          }
       };

      template <nullable_t T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            skip_ws<Opts>(ctx, it, end);
            
            if (*it == 'n') {
               ++it;
               match<"ull">(ctx, it, end);
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
                  else if constexpr (constructible<T>) {
                     value = meta_construct_v<T>();
                  }
                  else {
                     ctx.error = error_code::invalid_nullable_read;
                     // Cannot read into unset nullable that is not std::optional, std::unique_ptr, or std::shared_ptr
                  }
               }
               read<json>::op<Opts>(*value, ctx, it, end);
            }
         }
      };
   }  // namespace detail

   template <class Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE parse_error validate_json(Buffer&& buffer) noexcept {
      context ctx{};
      glz::skip skip_value{};
      return read<opts{.force_conformance = true}>(skip_value, std::forward<Buffer>(buffer), ctx);
   }

   template <class T, class Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE parse_error read_json(T& value, Buffer&& buffer) noexcept {
      context ctx{};
      return read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
   }
   
   template <class T, class Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE expected<T, parse_error> read_json(Buffer&& buffer) noexcept {
      T value{};
      context ctx{};
      const auto ec = read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec) {
         return unexpected(ec);
      }
      return value;
   }
   
   template <auto Opts = opts{}, class T>
   GLZ_ALWAYS_INLINE parse_error read_file_json(T& value, const sv file_name) {
      
      context ctx{};
      ctx.current_file = file_name;
      
      std::string buffer;
      
      const auto ec = file_to_buffer(buffer, ctx.current_file);
      
      if (static_cast<bool>(ec)) {
         return { ec };
   }
      
      return read<Opts>(value, buffer, ctx);
   }
}
