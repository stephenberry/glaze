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
      // custom_t allows a user to register member functions (and std::function members) to implement custom reading and
      // writing
      template <class T, class From, class To>
      struct custom_t final
      {
         static constexpr auto reflect = false;
         using from_t = From;
         using to_t = To;
         T& val;
         From from;
         To to;
      };

      template <class T, class From, class To>
      custom_t(T&, From, To) -> custom_t<T, From, To>;

      template <class T>
         requires(is_specialization_v<T, custom_t>)
      struct from_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using From = typename V::from_t;

            if constexpr (std::is_member_pointer_v<From>) {
               if constexpr (std::is_member_function_pointer_v<From>) {
                  using Ret = typename return_type<From>::type;
                  if constexpr (std::is_void_v<Ret>) {
                     using Tuple = typename inputs_as_tuple<From>::type;
                     if constexpr (std::tuple_size_v<Tuple> == 0) {
                        skip_array<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        (value.val.*(value.from))();
                     }
                     else if constexpr (std::tuple_size_v<Tuple> == 1) {
                        std::decay_t<std::tuple_element_t<0, Tuple>> input{};
                        read<json>::op<Opts>(input, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        (value.val.*(value.from))(input);
                     }
                     else {
                        static_assert(false_v<T>, "function cannot have more than one input");
                     }
                  }
                  else {
                     static_assert(false_v<T>, "function must have void return");
                  }
               }
               else if constexpr (std::is_member_object_pointer_v<From>) {
                  auto& from = value.val.*(value.from);
                  using Func = std::decay_t<decltype(from)>;
                  if constexpr (is_specialization_v<Func, std::function>) {
                     using Ret = typename function_traits<Func>::result_type;

                     if constexpr (std::is_void_v<Ret>) {
                        using Tuple = typename function_traits<Func>::arguments;
                        if constexpr (std::tuple_size_v<Tuple> == 0) {
                           skip_array<Opts>(ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                           from();
                        }
                        else if constexpr (std::tuple_size_v<Tuple> == 1) {
                           std::decay_t<std::tuple_element_t<0, Tuple>> input{};
                           read<json>::op<Opts>(input, ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                           from(input);
                        }
                        else {
                           static_assert(false_v<T>, "function cannot have more than one input");
                        }
                     }
                     else {
                        static_assert(false_v<T>, "std::function must have void return");
                     }
                  }
                  else {
                     read<json>::op<Opts>(from, ctx, it, end);
                  }
               }
               else {
                  static_assert(false_v<T>, "invalid type for custom");
               }
            }
            else {
               read<json>::op<Opts>(get_member(value.val, value.from), ctx, it, end);
            }
         }
      };

      template <class T>
         requires(is_specialization_v<T, custom_t>)
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            using V = std::decay_t<decltype(value)>;
            using To = typename V::to_t;

            if constexpr (std::is_member_pointer_v<To>) {
               if constexpr (std::is_member_function_pointer_v<To>) {
                  using Tuple = typename inputs_as_tuple<To>::type;
                  if constexpr (std::tuple_size_v<Tuple> == 0) {
                     write<json>::op<Opts>((value.val.*(value.to))(), ctx, args...);
                  }
                  else {
                     static_assert(false_v<T>, "function cannot have inputs");
                  }
               }
               else if constexpr (std::is_member_object_pointer_v<To>) {
                  auto& to = value.val.*(value.to);
                  using Func = std::decay_t<decltype(to)>;
                  if constexpr (is_specialization_v<Func, std::function>) {
                     using Ret = typename function_traits<Func>::result_type;

                     if constexpr (std::is_void_v<Ret>) {
                        static_assert(false_v<T>, "conversion to JSON must return a value");
                     }
                     else {
                        using Tuple = typename function_traits<Func>::arguments;
                        if constexpr (std::tuple_size_v<Tuple> == 0) {
                           write<json>::op<Opts>(to(), ctx, args...);
                        }
                        else {
                           static_assert(false_v<T>, "std::function cannot have inputs");
                        }
                     }
                  }
                  else {
                     write<json>::op<Opts>(to, ctx, args...);
                  }
               }
               else {
                  static_assert(false_v<T>, "invalid type for custom");
               }
            }
            else {
               write<json>::op<Opts>(get_member(value.val, value.to), ctx, args...);
            }
         }
      };

      template <auto From, auto To>
      inline constexpr decltype(auto) custom_impl() noexcept
      {
         return [](auto&& v) { return custom_t{v, From, To}; };
      }
   }

   template <auto From, auto To>
   constexpr auto custom = detail::custom_impl<From, To>();
}
