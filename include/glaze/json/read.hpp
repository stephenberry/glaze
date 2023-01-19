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
      
      template <is_member_function_pointer T>
      struct from_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args)
         {
            throw std::runtime_error("attempted to read into member function pointer");
         }
      };
      
      template <is_reference_wrapper T>
      struct from_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args)
         {
            using V = std::decay_t<decltype(value.get())>;
            from_json<V>::template op<Opts>(value.get(), std::forward<Args>(args)...);
         };
      };
      
      template <>
      struct from_json<hidden>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&&, auto&&... args)
         {
            throw std::runtime_error("hidden type attempted to be read");
         };
      };
      
      template <>
      struct from_json<std::monostate>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            match<R"("std::monostate")">(args...);
         };
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
            const auto is_glaze_object = std::visit([](auto&& v) {
               using V = std::decay_t<decltype(v)>;
               if constexpr (glaze_object_t<V>) {
                  return true;
               }
               else {
                  return false;
               }
            }, value);
            
            if (is_glaze_object) {
               skip_ws(it, end);
               match<'{'>(it);
               skip_ws(it, end);
               
               match<R"("type")">(it, end);
               skip_ws(it, end);
               match<':'>(it);
               
               using V = std::decay_t<decltype(value)>;
               
               static constexpr auto names = variant_type_names<V>();
               static constexpr auto N = names.size();
               
               // TODO: Change from linear search to map?
               static thread_local std::string type{};
               read<json>::op<Opts>(type, ctx, it, end);
               skip_ws(it, end);
               match<','>(it);
               
               for_each<N>([&](auto I) {
                  constexpr auto N = [] {
                     return names[decltype(I)::value].size();
                  }(); // MSVC internal compiler error workaround
                  if (string_cmp_n<N>(type, names[I])) {
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
            else {
               std::visit([&](auto&& v) {
                  using V = std::decay_t<decltype(v)>;
                  read<json>::op<Opts>(v, ctx, it, end);
               }, value);
            }
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
            
            // TODO: fix this
            using X = std::conditional_t<std::is_const_v<std::remove_pointer_t<std::remove_reference_t<decltype(it)>>>, const uint8_t*, uint8_t*>;
            auto cur = reinterpret_cast<X>(it);
            auto s = parse_number(value, cur);
            if (!s) [[unlikely]]
               throw std::runtime_error("Failed to parse number");
            it = reinterpret_cast<std::remove_reference_t<decltype(it)>>(cur);
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
               match<'"'>(it);
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
                     throw std::runtime_error("\\u should be followed by 4 hex digits.");
                  }
                  uint32_t codepoint_integer;
                  std::stringstream ss;
                  ss << std::hex << sv{ it, 4 };
                  ss >> codepoint_integer;
                  
                  //auto [ptr, ec] = std::from_chars(it, it + 4, codepoint_double, std::chars_format::hex);
                  //if (ec != std::errc() || ptr - it != 4) {
                  //   throw std::runtime_error("Invalid hex value for unicode escape.");
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
                     throw std::runtime_error("Could not convert unicode escape.");
                  }
                  value.append(reinterpret_cast<char*>(buffer), to_next - buffer);
                  std::advance(it, 4);
                  break;
               }
               default:
                  throw std::runtime_error("Invalid escape.");
               }
            };
            
            // growth portion
            value.clear(); // Single append on unescaped strings so overwrite opt isnt as important
            auto start = it;
            while (it < end) {
               skip_till_escape_or_quote(it, end);
               if (*it == '"') {
                  value.append(start, static_cast<size_t>(it - start));
                  ++it;
                  return;
               }
               else {
                  value.append(start, static_cast<size_t>(it - start));
                  if (++it == end) [[unlikely]]
                     throw std::runtime_error(R"(Expected ")");
                  handle_escaped();
                  start = it;
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
            match<'"'>(it);
            if (it == end) [[unlikely]]
               throw std::runtime_error("Unxpected end of buffer");
            if (*it == '\\') [[unlikely]] {
               if (++it == end) [[unlikely]]
                  throw std::runtime_error("Unxpected end of buffer");
               switch (*it) {
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
                        throw std::runtime_error("\\u should be followed by 4 hex digits.");
                     }
                     //double codepoint_double;
                     //auto [ptr, ec] = from_chars(it, it + 4, codepoint_double, std::chars_format::hex);
                     //if (ec != std::errc() || ptr - it != 4) {
                     //   throw std::runtime_error("Invalid hex value for unicode escape.");
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
                        char32_t codepoint = codepoint_integer;
                        char8_t buffer[4];
                        auto& f = std::use_facet<std::codecvt<char32_t, char8_t, mbstate_t>>(std::locale());
                        std::mbstate_t mb{};
                        const char32_t* from_next;
                        char8_t* to_next;
                        const auto result = f.out(mb, &codepoint, &codepoint + 1, from_next, buffer, buffer + 4, to_next);
                        if (result != std::codecvt_base::ok) {
                           throw std::runtime_error("Could not convert unicode escape.");
                        }
                        
                        const auto n = to_next - buffer;
                        
                        if constexpr (sizeof(T) == 1) {
                           std::memcpy(&value, buffer, 1);
                        }
                        else {
                           using buffer_type = std::conditional_t<std::is_same_v<T, wchar_t>, char, char8_t>;
                           auto& f = std::use_facet<std::codecvt<T, buffer_type, mbstate_t>>(std::locale());
                           std::mbstate_t mb{};
                           const buffer_type* from_next;
                           T* to_next;
                           auto* rbuf = reinterpret_cast<buffer_type*>(buffer);
                           const auto result = f.in(mb, rbuf, rbuf + n, from_next, &value, &value + 1, to_next);
                           if (result != std::codecvt_base::ok) {
                              throw std::runtime_error("Could not convert unicode escape.");
                           }
                        }
                     }
                     
                     std::advance(it, 4);
                     break;
                  }
               }
            }
            else {
               value = *it++;
            }
            match<'"'>(it);
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
         static void op(auto& /*value*/, is_context auto&& /*ctx*/, auto&& it, auto&& end)
         {
            skip_ws(it, end);
            match<'"'>(it);
            skip_till_quote(it, end);
            match<'"'>(it);
         }
      };
      
      template <>
      struct from_json<raw_json>
      {
         template <auto Opts>
         static void op(raw_json& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
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
            
            match<'['>(it);
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
      
      // counts the number of JSON array elements
      // needed for classes that are resizable, but do not have an emplace_back
      // it is copied so that it does not actually progress the iterator
      // expects the opening brace ([) to have already been consumed
      inline size_t number_of_array_elements(auto it, auto&& end)
      {
         skip_ws(it, end);
         if (*it == ']') [[unlikely]] {
            return 0;
         }
         size_t count = 1;
         while (it != end) {
            switch (*it)
            {
               case ',': {
                  ++count;
                  ++it;
                  break;
               }
               case '/': {
                  skip_ws(it, end);
                  break;
               }
               case ']': {
                  return count;
               }
               default:
                  ++it;
            }
         }
         return {}; // should never be reached
      }
      
      template <class T> requires array_t<T> &&
      (!emplace_backable<T> &&
       resizeable<T>)
      struct from_json<T>
      {
         template <auto Options>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if constexpr (!Options.ws_handled) {
               skip_ws(it, end);
            }
            static constexpr auto Opts = ws_handled_off<Options>();
            
            match<'['>(it);
            const auto n = number_of_array_elements(it, end);
            value.resize(n);
            size_t i = 0;
            for (auto& x : value) {
               read<json>::op<Opts>(x, ctx, it, end);
               skip_ws(it, end);
               if (i < n - 1) {
                  match<','>(it);
               }
               ++i;
            }
            match<']'>(it);
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
            
            match<'['>(it);
            skip_ws(it, end);
            
            for_each<N>([&](auto I) {
               if (it == end || *it == ']') {
                  return;
               }
               if constexpr (I != 0) {
                  match<','>(it);
                  skip_ws(it, end);
               }
               if constexpr (is_std_tuple<T>) {
                  read<json>::op<ws_handled<Opts>()>(std::get<I>(value), ctx, it, end);
               }
               else if constexpr (glaze_array_t<T>) {
                  read<json>::op<ws_handled<Opts>()>(get_member(value, glz::tuplet::get<I>(meta_v<T>)), ctx, it, end);
               }
               else {
                  read<json>::op<ws_handled<Opts>()>(glz::tuplet::get<I>(value), ctx, it, end);
               }
               skip_ws(it, end);
            });
            
            match<']'>(it);
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
               match<'{'>(it);
            }
            
            skip_ws(it, end);
            
            static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();
            
            bool first = true;
            while (it != end) {
               if (*it == '}') [[unlikely]] {
                  ++it;
                  return;
               }
               else if (first) [[unlikely]]
                  first = false;
               else [[likely]] {
                  match<','>(it);
               }
               
               if constexpr (glaze_object_t<T>) {
                  std::string_view key;
                  // skip white space and escape characters and find the string
                  skip_ws(it, end);
                  match<'"'>(it);
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
                     key = sv{ start, static_cast<size_t>(it - start) };
                     ++it;
                  }
                  
                  skip_ws(it, end);
                  match<':'>(it);
                  
                  static constexpr auto frozen_map = detail::make_map<T, Opts.allow_hash_check>();
                  if constexpr (Opts.error_on_unknown_keys) {
                     std::visit(
                        [&](auto&& member_ptr) {
                           read<json>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
                        }, frozen_map.at(key));
                  }
                  else {
                     const auto& member_it = frozen_map.find(key);
                     if (member_it != frozen_map.end()) [[likely]] {
                        std::visit(
                           [&](auto&& member_ptr) {
                              read<json>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
                           },
                           member_it->second);
                     }
                     else [[unlikely]] {
                        skip_object_value(it, end);
                     }
                  }
               }
               else {
                  static thread_local std::string key{};
                  read<json>::op<Opts>(key, ctx, it, end);
                  
                  skip_ws(it, end);
                  match<':'>(it);
                  
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
   
   template <auto Opts = opts{}, class T>
   inline void read_file_json(T& value, const sv file_name) {
      
      context ctx{};
      ctx.current_file = file_name;
      
      std::string buffer;
      
      file_to_buffer(buffer, ctx.current_file);
      
      read<Opts>(value, buffer, ctx);
   }
}
