// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail
   {
      // manage_t invokes a function call before reading and after writing from a value
      template <class T, class Member, class From, class To>
      struct manage_t
      {
         static constexpr auto reflect = false;
         using from_t = From;
         using to_t = To;
         T& val;
         Member member;
         From from;
         To to;
      };

      template <class T, class Member, class From, class To>
      manage_t(T&, Member, From, To) -> manage_t<T, Member, From, To>;

      template <class T>
         requires(is_specialization_v<T, manage_t>)
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using From = typename V::from_t;

            read<json>::op<Opts>(get_member(value.val, value.member), ctx, it, end);

            if constexpr (std::is_member_pointer_v<From>) {
               if constexpr (std::is_member_function_pointer_v<From>) {
                  if (!(value.val.*(value.from))()) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               else if constexpr (std::is_member_object_pointer_v<From>) {
                  auto& from = value.val.*(value.from);
                  using Func = std::decay_t<decltype(from)>;
                  if constexpr (std::is_invocable_r_v<bool, Func>) {
                     if (!value.from()) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }
                  else {
                     static_assert(false_v<T>, "function must have no arguments with a bool return");
                  }
               }
               else {
                  static_assert(false_v<T>, "invalid type");
               }
            }
            else {
               if (!value.from(value.val)) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
      };

      template <class T>
         requires(is_specialization_v<T, manage_t>)
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using To = typename V::to_t;

            if constexpr (std::is_member_pointer_v<To>) {
               if constexpr (std::is_member_function_pointer_v<To>) {
                  if (!(value.val.*(value.to))()) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               else if constexpr (std::is_member_object_pointer_v<To>) {
                  auto& to = value.val.*(value.to);
                  using Func = std::decay_t<decltype(to)>;
                  if constexpr (std::is_invocable_r_v<bool, Func>) {
                     if (!value.to()) {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }
                  else {
                     static_assert(false_v<T>, "function must have no arguments with a bool return");
                  }
               }
               else {
                  static_assert(false_v<T>, "invalid type");
               }
            }
            else {
               if (!value.to(value.val)) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            write<json>::op<Opts>(get_member(value.val, value.member), ctx, args...);
         }
      };

      template <auto Member, auto From, auto To>
      inline constexpr decltype(auto) manage_impl() noexcept
      {
         return [](auto&& v) { return manage_t{v, Member, From, To}; };
      }
   }

   template <auto Member, auto From, auto To>
   constexpr auto manage = detail::manage_impl<Member, From, To>();
}
