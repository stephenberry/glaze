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
#include "glaze/file/file_ops.hpp"
#include "glaze/util/strod.hpp"

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
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) {
            from_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <is_variant T>
      constexpr auto variant_type_names() {
         constexpr auto N = std::variant_size_v<T>;
         std::array<sv, N> ret{};
         
         for_each<N>([&](auto I) {
            ret[I] = glz::name_v<std::decay_t<std::variant_alternative_t<I, T>>>;
         });
         
         return ret;
      }
      
      template <is_variant T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            match<'{'>(it, end);
            skip_ws(it, end);
            
            match<R"("type")">(it, end);
            skip_ws(it, end);
            match<':'>(it, end);
            
            using V = std::decay_t<decltype(value)>;
            
            static constexpr auto names = variant_type_names<V>();
            static constexpr auto N = names.size();
            
            // TODO: Change from linear search to map?
            static thread_local std::string type{};
            read<json>::op<Opts>(type, ctx, it, end);
            skip_ws(it, end);
            match<','>(it, end);
            
            for_each<N>([&](auto I) {
               if (string_cmp(type, names[I])) {
                  using V = std::variant_alternative_t<I, T>;
                  if (!std::holds_alternative<V>(value)) {
                     // default construct the value if not matching
                     value = V{};
                  }
                  read<json>::op<opening_handled<Opts>()>(std::get<I>(value), ctx, it, end);
                  return;
               }
            });
         }
      };
      
      template <bool_t T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(bool_t auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
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
         template <auto Options, class It>
         static void op(auto&& value, is_context auto&& ctx, It&& it, auto&& end)
         {
            if constexpr (!Options.ws_handled) {
               skip_ws(it, end);
            }
            
            if (it == end) [[unlikely]] {
               throw std::runtime_error("Unexpected end of buffer");
            }
            
            if constexpr (std::contiguous_iterator<std::decay_t<It>>)
            {
               auto start = reinterpret_cast<const uint8_t *>(&*it);
               auto s = parse_number(value, start);
               if (!s) [[unlikely]]
                  throw std::runtime_error("Failed to parse number");
               //if constexpr (std::floating_point<std::decay_t<T>>) {
               //   std::string ts = "2.2861746047729334,";
               //   double val2{};
               //   auto start2 = reinterpret_cast<const uint8_t*>(ts.data());
               //   auto s = parse_number(val2, start2);

               //   std::cout << "\nnum:" << value << ", " << val2 <<
               //      ", '"
               //             << std::string_view{&*it, std::size_t(start - reinterpret_cast<const uint8_t*>(&*it))}
               //             << "'\n";
               //}
               it += (start - reinterpret_cast<const uint8_t *>(&*it));
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
               buffer[i] = '\0';
               auto start = reinterpret_cast<const uint8_t*>(buffer);
               auto s = parse_number(value, start);
               if (!s) [[unlikely]]
                  throw std::runtime_error("Failed to parse number");
            }
         }
      };

      template <str_t T>
      struct from_json<T>
      {
         template <auto Opts, class It, class End>
         static void op(auto& value, is_context auto&& ctx, It&& it, End&& end)
         {
            // TODO: this does not handle control chars like \t and \n
            
            if constexpr (!Opts.ws_handled) {
               skip_ws(it, end);
            }
            
            if constexpr (!Opts.opening_handled) {
               match<'"'>(it, end);
            }
            
            // overwrite portion
            
            if constexpr (!std::contiguous_iterator<std::decay_t<It>>) {
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
            if constexpr (std::contiguous_iterator<std::decay_t<It>>) {
               value.clear(); // Single append on unescaped strings so overwrite opt isnt as important
               auto start = it;
               while (it < end) {
                  skip_till_escape_or_quote(it, end);
                  if (*it == '"') {
                     value.append(&*start, static_cast<size_t>(std::distance(start, it)));
                     ++it;
                     return;
                  }
                  else {
                     value.append(&*start, static_cast<size_t>(std::distance(start, it)));
                     auto esc = *it;
                     ++it;

                     switch (*it)
                     {
                     case '"':
                     case '\\':
                     case '/':
                        value.push_back(*it);
                        ++it;
                        break;
                     case 'b':
                     case 'f':
                     case 'n':
                     case 'r':
                     case 't':
                        value.push_back(esc);
                        value.push_back(*it);
                        ++it;
                        break;
                     case 'u': {
                        value.push_back(esc);
                        value.push_back(*it);
                        ++it;

                        std::string_view temp{ &*it, 4 };
                        if (std::all_of(temp.begin(), temp.end(), ::isxdigit)) {
                           value.append(&*it, 4);
                           it += 4;
                        }
                        else
                           throw std::runtime_error("Invalid hex value for unicode escape.");

                        break;
                     }
                     default:
                        throw std::runtime_error("Unknown escape character.");
                     }

                     start = it;
                  }
               }
            }
            else {
               while (it != end) {
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
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
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
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            const auto key = parse_key(it, end);

            static constexpr auto frozen_map = detail::make_string_to_enum_map<T>();
            const auto& member_it = frozen_map.find(frozen::string(key));
            if (member_it != frozen_map.end()) {
               value = member_it->second;
            }
            else [[unlikely]] {
               throw std::runtime_error("Enexpected enum value '" + std::string(key) + "'");
            }
         }
      };

      template <func_t T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& /*value*/, is_context auto&& /*ctx*/, auto&& /*it*/, auto&& /*end*/)
         {
         }
      };
      
      template <>
      struct from_json<raw_json>
      {
         template <auto Opts>
         static void op(raw_json& value, is_context auto&& ctx, auto&& it, auto&& end)
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
         template <auto Options>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (!Options.ws_handled) {
               skip_ws(it, end);
            }
            static constexpr auto Opts = ws_handled_off<Options>();
            
            match<'['>(it, end);
            skip_ws(it, end);
            if (it == end) {
               throw std::runtime_error("Unexpected end");
            }
            
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
               read<json>::op<ws_handled<Opts>()>(*value_it++, ctx, it, end);
               skip_ws(it, end);
               if (it == end) {
                  throw std::runtime_error("Unexpected end");
               }
               if (*it == ',') [[likely]] {
                  ++it;
                  skip_ws(it, end);
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
                  throw std::runtime_error("Expected ]");
               }
            }
            
            // growing
            if constexpr (emplace_backable<T>) {
               while (it < end) {
                  read<json>::op<ws_handled<Opts>()>(value.emplace_back(), ctx, it, end);
                  skip_ws(it, end);
                  if (*it == ',') [[likely]] {
                     ++it;
                     skip_ws(it, end);
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
         template <auto Options>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            using value_t = nano::ranges::range_value_t<T>;
            static thread_local std::vector<value_t> buffer{};
            buffer.clear();

            if constexpr (!Options.ws_handled) {
               skip_ws(it, end);
            }
            static constexpr auto Opts = ws_handled_off<Options>();
            
            match<'['>(it, end);
            skip_ws(it, end);
            for (size_t i = 0; it < end; ++i) {
               if (*it == ']') [[unlikely]] {
                  ++it;
                  value.resize(i);
                  
                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
                  
                  auto value_it = std::ranges::begin(value);
                  for (size_t j = 0; j < i; ++j) {
                     *value_it++ = buffer[j];
                  }
                  return;
               }
               if (i > 0) [[likely]] {
                  match<','>(it, end);
               }
               read<json>::op<Opts>(buffer.emplace_back(), ctx, it, end);
               skip_ws(it, end);
            }
            throw std::runtime_error("Expected ]");
         }
      };

      template <class T>
      requires glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>
      struct from_json<T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
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
            
            if constexpr (!Opts.ws_handled) {
               skip_ws(it, end);
            }
            
            match<'['>(it, end);
            skip_ws(it, end);
            
            for_each<N>([&](auto I) {
               if (it == end || *it == ']') {
                  return;
               }
               if constexpr (I != 0) {
                  match<','>(it, end);
                  skip_ws(it, end);
               }
               if constexpr (is_std_tuple<T>) {
                  read<json>::op<ws_handled<Opts>()>(std::get<I>(value), ctx, it, end);
               }
               else if constexpr (glaze_array_t<T>) {
                  read<json>::op<ws_handled<Opts>()>(value.*glz::tuplet::get<I>(meta_v<T>), ctx, it, end);
               }
               else {
                  read<json>::op<ws_handled<Opts>()>(glz::tuplet::get<I>(value), ctx, it, end);
               }
               skip_ws(it, end);
            });
            
            match<']'>(it, end);
         }
      };
      
      template <class T>
      struct from_json<includer<T>>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            static thread_local std::string path{};
            read<json>::op<Opts>(path, ctx, it, end);
            
            try {
               // TODO: change this to streaming
               
               const auto file_path = relativize_if_not_absolute(std::filesystem::path(ctx.current_file).parent_path(), std::filesystem::path{ path });
               
               std::string buffer{};
               std::ifstream file{ file_path };
               if (file) {
                  file.seekg(0, std::ios::end);
                  buffer.resize(file.tellg());
                  file.seekg(0);
                  file.read(buffer.data(), buffer.size());
                  
                  const auto current_file = ctx.current_file;
                  ctx.current_file = file_path.string();
                  
                  glz::read<Opts>(value.value, buffer, ctx);
                  
                  ctx.current_file = current_file;
               }
               else {
                  throw std::runtime_error("could not open file: " + path);
               }
            } catch (const std::exception& e) {
               throw std::runtime_error("include error for " + ctx.current_file + std::string(" | ") + e.what());
            }
         }
      };
      
      template <class T>
      requires map_t<T> || glaze_object_t<T>
      struct from_json<T>
      {
         template <auto Options, class It>
         static void op(auto& value, is_context auto&& ctx, It&& it, auto&& end)
         {
            if constexpr (!Options.opening_handled) {
               skip_ws(it, end);
               match<'{'>(it, end);
            }
            
            skip_ws(it, end);
            
            static constexpr auto Opts = opening_handled_off<Options>();
            
            bool first = true;
            while (it != end) {
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
                     skip_till_escape_or_quote(it, end);
                     if (*it == '\\') [[unlikely]] {
                        // we dont' optimize this currently because it would increase binary size significantly with the complexity of generating escaped compile time versions of keys
                        it = start;
                        static thread_local std::string static_key{};
                        read<json>::op<opening_handled<Opts>()>(static_key, ctx, it, end);
                        key = static_key;
                     }
                     else [[likely]] {
                        key = sv{ &*start, static_cast<size_t>(std::distance(start, it)) };
                        ++it;
                     }
                  }
                  else {
                     static thread_local std::string static_key{};
                     read<json>::op<Opts>(static_key, ctx, it, end);
                     key = static_key;
                  }
                  
                  skip_ws(it, end);
                  match<':'>(it, end);
                  
                  static constexpr auto frozen_map = detail::make_map<T, Opts.allow_hash_check>();
                  const auto& member_it = frozen_map.find(key);
                  if (member_it != frozen_map.end()) {
                     std::visit(
                        [&](auto&& member_ptr) {
                           read<json>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
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
                  read<json>::op<Opts>(key, ctx, it, end);
                  
                  skip_ws(it, end);
                  match<':'>(it, end);
                  
                  if constexpr (std::is_same_v<typename T::key_type,
                                               std::string>) {
                     read<json>::op<Opts>(value[key], ctx, it, end);
                  }
                  else {
                     static thread_local typename T::key_type key_value{};
                     read<json>::op<Opts>(key_value, ctx, key.begin(), key.end());
                     read<json>::op<Opts>(value[key_value], ctx, it, end);
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
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
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
               read<json>::op<Opts>(*value, ctx, it, end);
            }
         }
      };
   }  // namespace detail
   
   template <class T, class Buffer>
   inline void read_json(T& value, Buffer&& buffer) {
      context ctx{};
      read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
   }
   
   template <class T, class Buffer>
   inline auto read_json(Buffer&& buffer) {
      T value{};
      context ctx{};
      read<opts{}>(value, std::forward<Buffer>(buffer), ctx);
      return value;
   }
   
   template <class T>
   inline void read_file_json(T& value, const sv file_name) {
      
      context ctx{};
      ctx.current_file = file_name;
      
      std::string buffer;
      
      file_to_buffer(buffer, ctx.current_file);
      
      read<opts{}>(value, buffer, ctx);
   }
}
