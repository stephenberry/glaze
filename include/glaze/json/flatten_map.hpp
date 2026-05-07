// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/write.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   template <class T>
   struct flatten_map_wrapper final
   {
      static constexpr auto glaze_reflect = false;
      using value_type = T;
      T& value;
   };

   template <class T>
      requires(range<T> && pair_t<range_value_t<T>>)
   flatten_map_wrapper(T&) -> flatten_map_wrapper<T>;

   template <auto MemPtr>
   constexpr auto flatten_map_impl() noexcept
   {
      return [](auto&& value) {
         using Value = std::decay_t<decltype(value)>;
         using Member = std::decay_t<member_t<Value, decltype(MemPtr)>>;
         return flatten_map_wrapper<Member>{get_member(value, MemPtr)};
      };
   }

   template <auto MemPtr>
   inline constexpr auto flatten_map = flatten_map_impl<MemPtr>();

   template <auto Opts, class T, class B>
   void write_flatten_map_elements(const T& value, is_context auto&& ctx, B&& b, auto& ix, bool& first)
   {
      using V = std::remove_cvref_t<T>;

      auto write_value = [&](const auto& item) {
         if (!first) {
            write_array_entry_separator<Opts>(ctx, b, ix);
         }
         serialize<JSON>::op<opening_and_closing_handled_off<Opts>()>(item, ctx, b, ix);
         first = false;
      };

      if constexpr (pair_t<V>) {
         write_flatten_map_elements<Opts>(value.first, ctx, b, ix, first);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         write_flatten_map_elements<Opts>(value.second, ctx, b, ix, first);
      }
      else if constexpr (glaze_array_t<V>) {
         static constexpr auto N = glz::tuple_size_v<meta_t<V>>;
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            write_flatten_map_elements<Opts>(get_member(value, glz::get<I>(meta_v<V>)), ctx, b, ix, first);
         });
      }
      else if constexpr (is_std_tuple<V>) {
         static constexpr auto N = glz::tuple_size_v<V>;
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            write_flatten_map_elements<Opts>(std::get<I>(value), ctx, b, ix, first);
         });
      }
      else if constexpr (tuple_t<V>) {
         static constexpr auto N = glz::tuple_size_v<V>;
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            write_flatten_map_elements<Opts>(glz::get<I>(value), ctx, b, ix, first);
         });
      }
      else if constexpr (has_fixed_size_container<V>) {
         static constexpr auto N = get_size<V>();
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            write_flatten_map_elements<Opts>(value[I], ctx, b, ix, first);
         });
      }
      else {
         write_value(value);
      }
   }

   template <auto Opts, class T, class B>
   void write_flatten_map_container(const T& value, is_context auto&& ctx, B&& b, auto& ix)
   {
      if constexpr (!check_opening_handled(Opts)) {
         dump('[', b, ix);
      }

      bool first = true;

      if (!empty_range(value) && Opts.prettify && check_new_lines_in_arrays(Opts)) {
         ctx.depth += check_indentation_width(Opts);
         dump_newline_indent(check_indentation_char(Opts), ctx.depth, b, ix);
      }

      for (auto&& [key, entry_val] : value) {
         write_flatten_map_elements<Opts>(key, ctx, b, ix, first);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         if (!first) {
            write_array_entry_separator<Opts>(ctx, b, ix);
         }
         serialize<JSON>::op<opening_and_closing_handled_off<Opts>()>(entry_val, ctx, b, ix);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         first = false;
      }

      if (!empty_range(value) && Opts.prettify && check_new_lines_in_arrays(Opts)) {
         ctx.depth -= check_indentation_width(Opts);
         dump_newline_indent(check_indentation_char(Opts), ctx.depth, b, ix);
      }

      if constexpr (!check_closing_handled(Opts)) {
         dump(']', b, ix);
      }
   }

   template <auto Opts, class T>
   void read_flatten_map_elements(T& value, is_context auto&& ctx, auto&& it, auto end, bool& first)
   {
      using V = std::remove_cvref_t<T>;

      auto parse_value = [&](auto& item) {
         if (!first) {
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
            if (match_invalid_end<',', Opts>(ctx, it, end)) {
               return;
            }
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
         }
         parse<JSON>::op<ws_handled<Opts>()>(item, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         first = false;
      };

      if constexpr (pair_t<V>) {
         read_flatten_map_elements<Opts>(value.first, ctx, it, end, first);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         read_flatten_map_elements<Opts>(value.second, ctx, it, end, first);
      }
      else if constexpr (glaze_array_t<V>) {
         static constexpr auto N = reflect<V>::size;
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            read_flatten_map_elements<Opts>(get_member(value, glz::get<I>(meta_v<V>)), ctx, it, end, first);
         });
      }
      else if constexpr (is_std_tuple<V>) {
         static constexpr auto N = glz::tuple_size_v<V>;
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            read_flatten_map_elements<Opts>(std::get<I>(value), ctx, it, end, first);
         });
      }
      else if constexpr (tuple_t<V>) {
         static constexpr auto N = glz::tuple_size_v<V>;
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            read_flatten_map_elements<Opts>(glz::get<I>(value), ctx, it, end, first);
         });
      }
      else if constexpr (has_fixed_size_container<V>) {
         static constexpr auto N = get_size<V>();
         for_each<N>([&]<size_t I>() {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            read_flatten_map_elements<Opts>(value[I], ctx, it, end, first);
         });
      }
      else {
         parse_value(value);
      }
   }

   template <auto Options, class T>
      requires(pair_t<range_value_t<T>>)
   void read_flatten_map_container(T& value, is_context auto&& ctx, auto&& it, auto end)
   {
      static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();

      if constexpr (!check_opening_handled(Options)) {
         if constexpr (!check_ws_handled(Options)) {
            if (skip_ws<Opts>(ctx, it, end)) {
               return;
            }
         }
         if (match_invalid_end<'[', Opts>(ctx, it, end)) {
            return;
         }
         if constexpr (not Opts.null_terminated) {
            if (it == end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            ++ctx.depth;
         }
      }

      if constexpr (readable_array_t<T>) {
         value.clear();
      }

      if (skip_ws<Opts>(ctx, it, end)) {
         return;
      }

      if (*it == ']') {
         ++it;
         if constexpr (not Opts.null_terminated) {
            --ctx.depth;
            if (it == end) {
               ctx.error = error_code::end_reached;
            }
         }
         return;
      }

      while (it < end) {
         using entry_t = range_value_t<T>;
         using key_t = std::remove_const_t<typename entry_t::first_type>;

         key_t key{};
         bool first = true;
         read_flatten_map_elements<Opts>(key, ctx, it, end, first);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         if (skip_ws<Opts>(ctx, it, end)) {
            return;
         }
         if (match_invalid_end<',', Opts>(ctx, it, end)) {
            return;
         }
         if (skip_ws<Opts>(ctx, it, end)) {
            return;
         }

         if constexpr (readable_array_t<T>) {
            if constexpr (has_try_emplace_back<T>) {
               if (not value.try_emplace_back()) [[unlikely]] {
                  ctx.error = error_code::exceeded_static_array_size;
                  return;
               }
            }
            else {
               value.emplace_back();
            }

            auto& entry = value.back();
            entry.first = std::move(key);
            parse<JSON>::op<ws_handled<Opts>()>(entry.second, ctx, it, end);
         }
         else {
            parse<JSON>::op<ws_handled<Opts>()>(value[key], ctx, it, end);
         }
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         if (skip_ws<Opts>(ctx, it, end)) {
            return;
         }

         if (*it == ',') {
            ++it;
            if constexpr (not Opts.null_terminated) {
               if (it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
            }
            continue;
         }

         if (*it == ']') {
            ++it;
            if constexpr (not Opts.null_terminated) {
               --ctx.depth;
               if (it == end) {
                  ctx.error = error_code::end_reached;
               }
            }
            return;
         }

         ctx.error = error_code::expected_bracket;
         return;
      }

      ctx.error = error_code::unexpected_end;
   }

   template <class T>
      requires(is_specialization_v<T, flatten_map_wrapper>)
   struct from<JSON, T>
   {
      template <auto Opts>
      static void op(auto&& wrapper, is_context auto&& ctx, auto&& it, auto end)
      {
         using V = typename std::remove_cvref_t<decltype(wrapper)>::value_type;
         static_assert(range<V> && pair_t<range_value_t<V>>,
                       "glz::flatten_map requires a map-like container or a range of pairs");
         read_flatten_map_container<Opts>(wrapper.value, ctx, it, end);
      }
   };

   template <class T>
      requires(is_specialization_v<T, flatten_map_wrapper>)
   struct to<JSON, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& wrapper, is_context auto&& ctx, Args&&... args)
      {
         using V = typename std::remove_cvref_t<decltype(wrapper)>::value_type;
         static_assert(range<V> && pair_t<range_value_t<V>>,
                       "glz::flatten_map requires a map-like container or a range of pairs");
         write_flatten_map_container<Opts>(wrapper.value, ctx, std::forward<Args>(args)...);
      }
   };
}
