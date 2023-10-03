// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   enum class manage_state : uint32_t
   {
      read,
      write,
      error
   };
   
   namespace detail
   {
      // manage_t invokes a single function call before reading or after writing from a value
      template <class Member, class Func>
      struct manage_t;

      template <class Member, class Func>
         requires(!std::is_member_function_pointer_v<Func>)
      struct manage_t<Member, Func> final
      {
         using member_t = Member;
         using func_t = Func;
         Member& member;
         Func& func;
      };

      template <class Member, class Func>
         requires(std::is_member_function_pointer_v<Func>)
      struct manage_t<Member, Func> final
      {
         using member_t = Member;
         using func_t = Func;
         typename parent_of_fn<Func>::type& val;
         Member& member;
         Func func;
      };

      template <class T>
      concept is_manage = requires {
                             typename T::member_t;
                             typename T::func_t;
                          };

      template <is_manage T>
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using Func = typename V::func_t;

            if constexpr (std::is_member_function_pointer_v<Func>) {
               read<json>::op<Opts>(value.member, ctx, it, end);
               (value.val.*value.func)(manage_state::read);
            }
            else if constexpr (is_specialization_v<Func, std::function>) {
               if constexpr (std::is_invocable_r_v<void, Func, manage_state>) {
                  read<json>::op<Opts>(value.member, ctx, it, end);
                  value.func(manage_state::read);
               }
               else {
                  static_assert(false_v<T>, "function must have one argument of glz::manage_state with a void return");
               }
            }
            else {
               static_assert(false_v<T>, "function must have one argument of glz::manage_state with a void return");
            }
         }
      };

      template <is_manage T>
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using Func = typename V::func_t;
            
            if constexpr (std::is_member_function_pointer_v<Func>) {
               (value.val.*value.func)(manage_state::write);
               write<json>::op<Opts>(value.member, ctx, args...);
            }
            else if constexpr (is_specialization_v<Func, std::function>) {
               if constexpr (std::is_invocable_r_v<void, Func, manage_state>) {
                  value.func(manage_state::write);
                  write<json>::op<Opts>(value.member, ctx, args...);
               }
               else {
                  static_assert(false_v<T>, "function must have one argument of glz::manage_state with a void return");
               }
            }
            else {
               static_assert(false_v<T>, "function must have one argument of glz::manage_state with a void return");
            }
         }
      };
      
      template <auto Member, auto Func>
      inline constexpr decltype(auto) manage_impl() noexcept
      {
         using M = decltype(Member);
         using F = decltype(Func);
         if constexpr (std::is_member_function_pointer_v<F>) {
            return [](auto&& v) { return manage_t<std::decay_t<decltype(v.*Member)>, std::decay_t<F>>{v, v.*Member, Func}; };
         }
         else if constexpr (!std::is_member_function_pointer_v<F>) {
            return [](auto&& v) { return manage_t<std::decay_t<decltype(v.*Member)>, std::decay_t<decltype(v.*Func)>>{v, v.*Member, v.*Func}; };
         }
         else {
            static_assert(false_v<std::pair<M, F>>, "invalid types");
         }
      }
   }

   template <auto From, auto To>
   constexpr auto manage = detail::manage_impl<From, To>();
}
