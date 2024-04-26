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
         static constexpr auto glaze_reflect = false;
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

            if constexpr (std::same_as<From, skip>) {
               skip_value<Opts>(ctx, it, end);
            }
            else if constexpr (std::is_member_pointer_v<From>) {
               if constexpr (std::is_member_function_pointer_v<From>) {
                  using Ret = typename return_type<From>::type;
                  if constexpr (std::is_void_v<Ret>) {
                     using Tuple = typename inputs_as_tuple<From>::type;
                     if constexpr (glz::tuple_size_v<Tuple> == 0) {
                        skip_array<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        (value.val.*(value.from))();
                     }
                     else if constexpr (glz::tuple_size_v<Tuple> == 1) {
                        std::decay_t<glz::tuple_element_t<0, Tuple>> input{};
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
                        if constexpr (glz::tuple_size_v<Tuple> == 0) {
                           skip_array<Opts>(ctx, it, end);
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                           from();
                        }
                        else if constexpr (glz::tuple_size_v<Tuple> == 1) {
                           std::decay_t<glz::tuple_element_t<0, Tuple>> input{};
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
               if constexpr (is_invocable_concrete<From>) {
                  using Ret = invocable_result_t<From>;
                  if constexpr (std::is_void_v<Ret>) {
                     using Tuple = invocable_args_t<From>;
                     constexpr auto N = glz::tuple_size_v<Tuple>;
                     if constexpr (N == 0) {
                        static_assert(false_v<T>, "lambda must take in the class as the first argument");
                     }
                     else if constexpr (N == 1) {
                        skip_array<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        value.from(value.val);
                     }
                     else if constexpr (N == 2) {
                        std::decay_t<glz::tuple_element_t<1, Tuple>> input{};
                        read<json>::op<Opts>(input, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        value.from(value.val, input);
                     }
                     else {
                        static_assert(false_v<T>, "lambda cannot have more than two inputs");
                     }
                  }
                  else {
                     static_assert(false_v<T>, "lambda must have void return");
                  }
               }
               else if constexpr (std::invocable<From, decltype(value.val)>) {
                  read<json>::op<Opts>(value.from(value.val), ctx, it, end);
               }
               else {
                  static_assert(
                     false_v<T>,
                     "IMPORTANT: If you have two arguments in your lambda (e.g. [](my_struct&, const std::string& "
                     "input)) you must make all the arguments concrete types. None of the inputs can be `auto`. Also, "
                     "you probably cannot define these lambdas within a local `struct glaze`, but instead need to use "
                     "`glz::meta` outside your class so that your lambda can operate on a defined class.");
               }
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
                  if constexpr (glz::tuple_size_v<Tuple> == 0) {
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
                        if constexpr (glz::tuple_size_v<Tuple> == 0) {
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
               if constexpr (std::invocable<To, decltype(value.val)>) {
                  write<json>::op<Opts>(std::invoke(value.to, value.val), ctx, args...);
               }
               else {
                  static_assert(false_v<To>,
                                "expected invocable function, perhaps you need const qualified input on your lambda");
               }
            }
         }
      };

      template <auto From, auto To>
      inline constexpr auto custom_impl() noexcept
      {
         return [](auto&& v) { return custom_t{v, From, To}; };
      }
   }

   template <auto From, auto To>
   constexpr auto custom = detail::custom_impl<From, To>();
}
