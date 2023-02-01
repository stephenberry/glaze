// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <iterator>
#include <ostream>
#include <variant>

#include "glaze/core/format.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/json/ptr.hpp"

#include "glaze/util/itoa.hpp"
#include "glaze/core/write_chars.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct to_json {};
      
      template <>
      struct write<json>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) {
            to_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };
      
      template <glaze_value_t T>
      struct to_json<T>
      {
         template <auto Opts, is_context Ctx, class B, class IX>
         static void op(auto&& value, Ctx&& ctx, B&& b, IX&& ix) {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            to_json<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };
      
      template <glaze_flags_t T>
      struct to_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix) {
            static constexpr auto N = std::tuple_size_v<meta_t<T>>;
            
            dump<'['>(b, ix);
            
            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<T>);
               
               if (get_member(value, glz::tuplet::get<1>(item))) {
                  dump<'"'>(b, ix);
                  dump(glz::tuplet::get<0>(item), b, ix);
                  dump<'"'>(b, ix);
                  dump<','>(b, ix);
               }
            });
            
            if (b[ix - 1] == ',') {
               b[ix - 1] = ']';
            }
            else {
               dump<']'>(b, ix);
            }
         }
      };
      
      template <>
      struct to_json<hidden>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&&, auto&&... args)
         {
            dump<'"'>(args...);
            dump("hidden type should not have been written", args...);
            dump<'"'>(args...);
         };
      };
      
      template <>
      struct to_json<skip>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&&, auto&&... args)
         {
            dump<'"'>(args...);
            dump("skip type should not have been written", args...);
            dump<'"'>(args...);
         };
      };
      
      template <is_member_function_pointer T>
      struct to_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&&, auto&&... args) {
         }
      };
      
      template <is_reference_wrapper T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, Args&&... args)
         {
            using V = std::decay_t<decltype(value.get())>;
            to_json<V>::template op<Opts>(value.get(), std::forward<Args>(args)...);
         };
      };
      
      template <boolean_like T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(const bool value, is_context auto&&, Args&&... args) noexcept
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
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            write_chars::op<Opts>(value, ctx, b, ix);
         }
      };

      template <class T>
      requires str_t<T> || char_t<T>
      struct to_json<T>
      {
         template <auto Opts, class B>
         static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            if constexpr (char_t<T>) {
               dump<'"'>(b, ix);
               switch (value) {
               case '"':
                  dump<"\\\"">(b, ix);
                  break;
               case '\\':
                  dump<"\\\\">(b, ix);
                  break;
               case '\b':
                  dump<"\\b">(b, ix);
                  break;
               case '\f':
                  dump<"\\f">(b, ix);
                  break;
               case '\n':
                  dump<"\\n">(b, ix);
                  break;
               case '\r':
                  dump<"\\r">(b, ix);
                  break;
               case '\t':
                  dump<"\\t">(b, ix);
                  break;
               default:
                  dump(value, b, ix);
               }
               dump<'"'>(b, ix);
            }
            else {
               const sv str = value;
               const auto n = str.size();
               
               // we use 4 * n to handle potential escape characters and quoted bounds (excessive safety)
               if constexpr (detail::resizeable<B>) {
                  if ((ix + 4 * n) >= b.size()) [[unlikely]] {
                     b.resize(std::max(b.size() * 2, ix + 4 * n));
                  }
               }
               
               dump_unchecked<'"'>(b, ix);
               
               using V = std::decay_t<decltype(b[0])>;
               if constexpr (std::same_as<V, std::byte>) {
                  // now we don't have to check writing
                  for (auto&& c : str) {
                     switch (c) {
                     case '"':
                        b[ix++] = static_cast<std::byte>('\\');
                        b[ix++] = static_cast<std::byte>('\"');
                        break;
                     case '\\':
                        b[ix++] = static_cast<std::byte>('\\');
                        b[ix++] = static_cast<std::byte>('\\');
                        break;
                     case '\b':
                        b[ix++] = static_cast<std::byte>('\\');
                        b[ix++] = static_cast<std::byte>('b');
                        break;
                     case '\f':
                        b[ix++] = static_cast<std::byte>('\\');
                        b[ix++] = static_cast<std::byte>('f');
                        break;
                     case '\n':
                        b[ix++] = static_cast<std::byte>('\\');
                        b[ix++] = static_cast<std::byte>('n');
                        break;
                     case '\r':
                        b[ix++] = static_cast<std::byte>('\\');
                        b[ix++] = static_cast<std::byte>('r');
                        break;
                     case '\t':
                        b[ix++] = static_cast<std::byte>('\\');
                        b[ix++] = static_cast<std::byte>('t');
                        break;
                     default:
                        b[ix++] = static_cast<std::byte>(c);
                     }
                  }
               }
               else {
                  // now we don't have to check writing
                  for (auto&& c : str) {
                     switch (c) {
                     case '"':
                        b[ix++] = '\\';
                        b[ix++] = '\"';
                        break;
                     case '\\':
                        b[ix++] = '\\';
                        b[ix++] = '\\';
                        break;
                     case '\b':
                        b[ix++] = '\\';
                        b[ix++] = 'b';
                        break;
                     case '\f':
                        b[ix++] = '\\';
                        b[ix++] = 'f';
                        break;
                     case '\n':
                        b[ix++] = '\\';
                        b[ix++] = 'n';
                        break;
                     case '\r':
                        b[ix++] = '\\';
                        b[ix++] = 'r';
                        break;
                     case '\t':
                        b[ix++] = '\\';
                        b[ix++] = 't';
                        break;
                     default:
                        b[ix++] = c;
                     }
                  }
               }
               
               dump_unchecked<'"'>(b, ix);
            }
         }
      };

      template <glaze_enum_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
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
                  static_cast<std::underlying_type_t<T>>(value), ctx, std::forward<Args>(args)...);
            }
         }
      };

      template <func_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<'"'>(args...);
            dump(name_v<std::decay_t<decltype(value)>>, args...);
            dump<'"'>(args...);
         }
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct to_json<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix) noexcept {
            dump(value.str, b, ix);
         }
      };
      
      template <array_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<'['>(std::forward<Args>(args)...);
            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(std::forward<Args>(args)...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
            }
            const auto is_empty = [&]() -> bool {
               if constexpr (has_size<T>) {
                  return value.size() ? false : true;
               }
               else {
                  return value.empty();
               }
            }();
            
            if (!is_empty) {
               auto it = value.begin();
               write<json>::op<Opts>(*it, ctx, std::forward<Args>(args)...);
               ++it;
               const auto end = value.end();
               for (; it != end; ++it) {
                  dump<','>(std::forward<Args>(args)...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(std::forward<Args>(args)...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
                  }
                  write<json>::op<Opts>(*it, ctx, std::forward<Args>(args)...);
               }
               if constexpr (Opts.prettify) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump<'\n'>(std::forward<Args>(args)...);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
               }
            }
            dump<']'>(std::forward<Args>(args)...);
         }
      };

      template <map_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<'{'>(std::forward<Args>(args)...);
            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(std::forward<Args>(args)...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
            }
            if (!value.empty()) {
               auto it = value.cbegin();
               auto write_pair = [&] {
                  using Key = decltype(it->first);
                  if constexpr (str_t<Key> || char_t<Key>) {
                     write<json>::op<Opts>(it->first, ctx, std::forward<Args>(args)...);
                  }
                  else {
                     dump<'"'>(std::forward<Args>(args)...);
                     write<json>::op<Opts>(it->first, ctx, std::forward<Args>(args)...);
                     dump<'"'>(std::forward<Args>(args)...);
                  }
                  dump<':'>(std::forward<Args>(args)...);
                  if constexpr (Opts.prettify) {
                     dump<' '>(std::forward<Args>(args)...);
                  }
                  write<json>::op<Opts>(it->second, ctx, std::forward<Args>(args)...);
               };
               write_pair();
               ++it;
               
               const auto end = value.cend();
               for (; it != end; ++it) {
                  using Value = std::decay_t<decltype(it->second)>;
                  if constexpr (nullable_t<Value> && Opts.skip_null_members) {
                     if (!bool(it->second)) continue;
                  }
                  dump<','>(std::forward<Args>(args)...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(std::forward<Args>(args)...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
                  }
                  write_pair();
               }
               if constexpr (Opts.prettify) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump<'\n'>(std::forward<Args>(args)...);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
               }
            }
            dump<'}'>(std::forward<Args>(args)...);
         }
      };
      
      template <nullable_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            if (value)
               write<json>::op<Opts>(*value, ctx, std::forward<Args>(args)...);
            else {
               dump<"null">(std::forward<Args>(args)...);
            }
         }
      };
      
      template <>
      struct to_json<std::monostate>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            dump<R"("std::monostate")">(args...);
         };
      };

      template <is_variant T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            std::visit([&](auto&& val) {
               using V = std::decay_t<decltype(val)>;
               if constexpr (Opts.write_type_info && glaze_object_t<V>) {
                  // must first write out type
                  if constexpr (Opts.prettify) {
                     dump<"{\n">(std::forward<Args>(args)...);
                     ctx.indentation_level += Opts.indentation_width;
                     dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
                     dump<R"("type": ")">(std::forward<Args>(args)...);
                     dump(name_v<V>, std::forward<Args>(args)...);
                     dump<"\",\n">(std::forward<Args>(args)...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
                  }
                  else {
                     dump<R"({"type":")">(std::forward<Args>(args)...);
                     dump(name_v<V>, std::forward<Args>(args)...);
                     dump<R"(",)">(std::forward<Args>(args)...);
                  }
                  write<json>::op<opening_handled<Opts>()>(val, ctx, std::forward<Args>(args)...);
               }
               else {
                  write<json>::op<Opts>(val, ctx, std::forward<Args>(args)...);
               }
            }, value);
         }
      };

      template <class T>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
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
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(std::forward<Args>(args)...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
            }
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<T>)), ctx, std::forward<Args>(args)...);
               }
               else {
                  write<json>::op<Opts>(glz::tuplet::get<I>(value), ctx, std::forward<Args>(args)...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  dump<','>(std::forward<Args>(args)...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(std::forward<Args>(args)...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
                  }
               }
            });
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(std::forward<Args>(args)...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
            }
            dump<']'>(std::forward<Args>(args)...);
         }
      };
      
      template <class T>
      struct to_json<includer<T>>
      {
         template <auto Opts, class... Args>
         static void op(auto&& /*value*/, is_context auto&& /*ctx*/, Args&&... args) noexcept {
         }
      };      

      template <class T>
      requires is_std_tuple<std::decay_t<T>>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
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
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(std::forward<Args>(args)...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
            }
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(value.*std::get<I>(meta_v<V>), ctx, std::forward<Args>(args)...);
               }
               else {
                  write<json>::op<Opts>(std::get<I>(value), ctx, std::forward<Args>(args)...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  dump<','>(std::forward<Args>(args)...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(std::forward<Args>(args)...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
                  }
               }
            });
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(std::forward<Args>(args)...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, std::forward<Args>(args)...);
            }
            dump<']'>(std::forward<Args>(args)...);
         }
      };

      template <const std::string_view& S>
      inline constexpr auto array_from_sv() noexcept
      {
         constexpr auto s = S; // Needed for MSVC to avoid an internal compiler error
         constexpr auto N = s.size();
         std::array<char, N> arr;
         std::copy_n(s.data(), N, arr.data());
         return arr;
      }

      inline constexpr bool needs_escaping(const auto& S) noexcept
      {
         for (const auto& c : S) {
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
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            if constexpr (!Options.opening_handled) {
               dump<'{'>(b, ix);
               if constexpr (Options.prettify) {
                  ctx.indentation_level += Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
            }
            
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            
            bool first = true;
            for_each<N>([&](auto I) {
               static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();
               static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
               using mptr_t = std::tuple_element_t<1, decltype(item)>;
               using val_t = member_t<V, mptr_t>;

               if constexpr (nullable_t<val_t> && Opts.skip_null_members) {
                  auto is_null = [&]()
                  {
                     if constexpr (std::is_member_pointer_v<std::tuple_element_t<1, decltype(item)>>) {
                        return !bool(value.*glz::tuplet::get<1>(item));
                     }
                     else {
                        return !bool(glz::tuplet::get<1>(item)(value));
                     }
                  }();
                  if (is_null) return;
               }
               
               // skip file_include
               if constexpr (std::is_same_v<val_t, includer<std::decay_t<V>>>) {
                  return;
               }
               
               if constexpr (std::is_same_v<val_t, hidden> || std::same_as<val_t, skip>) {
                  return;
               }

               if (first) {
                  first = false;
               }
               else {
                  // Null members may be skipped so we cant just write it out for all but the last member unless
                  // trailing commas are allowed
                  dump<','>(b, ix);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(b, ix);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, b, ix);
                  }
               }

               using Key = typename std::decay_t<std::tuple_element_t<0, decltype(item)>>;
               
               if constexpr (str_t<Key> || char_t<Key>) {
                  static constexpr sv key = glz::tuplet::get<0>(item);
                  if constexpr (needs_escaping(key)) {
                     write<json>::op<Opts>(key, ctx, b, ix);
                     dump<':'>(b, ix);
                     if constexpr (Opts.prettify) {
                        dump<' '>(b, ix);
                     }
                  }
                  else {
                     if constexpr (Opts.prettify) {
                        static constexpr auto quoted = join_v<chars<"\"">, key, chars<"\": ">>;
                        dump<quoted>(b, ix);
                     }
                     else {
                        static constexpr auto quoted = join_v<chars<"\"">, key, chars<"\":">>;
                        dump<quoted>(b, ix);
                     }
                  }
               }
               else {
                  static constexpr auto quoted =
                     concat_arrays(concat_arrays("\"", glz::tuplet::get<0>(item)), "\":", Opts.prettify ? " " : "");
                  write<json>::op<Opts>(quoted, ctx, b, ix);
               }
               
               write<json>::op<Opts>(get_member(value, glz::tuplet::get<1>(item)), ctx, b, ix);
               
               static constexpr auto S = std::tuple_size_v<decltype(item)>;
               if constexpr (Opts.comments && S > 2) {
                  static constexpr sv comment = glz::tuplet::get<2>(item);
                  if constexpr (comment.size() > 0) {
                     if constexpr (Opts.prettify) {
                        dump<' '>(b, ix);
                     }
                     dump<"/*">(b, ix);
                     dump(comment, b, ix);
                     dump<"*/">(b, ix);
                  }
               }
            });
            if constexpr (Options.prettify) {
               ctx.indentation_level -= Options.indentation_width;
               dump<'\n'>(b, ix);
               dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
            }
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

   void buffer_to_file(auto&& buffer, auto&& file_name) {
      auto file = std::ofstream(file_name, std::ios::out);
      if (!file) {
         throw std::runtime_error("glz::buffer_to_file: Could not create file with path (" + file_name + ").");
      }
      file.write(buffer.data(), buffer.size());
   }
   
   // std::string file_name needed for std::ofstream
   template <class T>
   inline void write_file_json(T&& value, const std::string& file_name) {
      std::string buffer{};
      write<opts{}>(std::forward<T>(value), buffer);
      buffer_to_file(buffer, file_name);
   }
}
