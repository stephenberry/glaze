// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/context.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/wrappers.hpp"
#include "glaze/core/write.hpp"

namespace glz::detail
{
   // Shared read dispatch for glz::custom.
   //
   // `parse_into(x)` reads one value of the format into x and `discard()` consumes one value
   // without producing anything. Both are supplied by the caller so that formats whose readers
   // take a tag the caller has already consumed (MSGPACK, BSON) can bind it here. What remains
   // is purely the shape of the user's handler, which is the same for every format.
   template <auto Opts, class T, class ParseInto, class Discard>
   void custom_read(auto&& value, is_context auto&& ctx, ParseInto&& parse_into, Discard&& discard)
   {
      {
         using V = std::decay_t<decltype(value)>;
         using From = typename V::from_t;

         if constexpr (std::same_as<From, skip>) {
            discard();
         }
         else if constexpr (std::is_member_pointer_v<From>) {
            if constexpr (std::is_member_function_pointer_v<From>) {
               using Ret = typename return_type<From>::type;
               if constexpr (std::is_void_v<Ret>) {
                  using Tuple = typename inputs_as_tuple<From>::type;
                  if constexpr (glz::tuple_size_v<Tuple> == 0) {
                     // A handler that takes no input only needs the incoming value discarded,
                     // whatever its shape. skip_value is defined for every format, so this works
                     // outside JSON and it round-trips a getter that writes a non-array value.
                     discard();
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     (value.val.*(value.from))();
                  }
                  else if constexpr (glz::tuple_size_v<Tuple> == 1) {
                     std::decay_t<glz::tuple_element_t<0, Tuple>> input{};
                     parse_into(input);
                     if constexpr (check_null_terminated(Opts)) {
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else {
                        if (size_t(ctx.error) > size_t(error_code::end_reached)) [[unlikely]]
                           return;
                     }
                     (value.val.*(value.from))(std::move(input));
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
                        // See the member function case above.
                        discard();
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        from();
                     }
                     else if constexpr (glz::tuple_size_v<Tuple> == 1) {
                        std::decay_t<glz::tuple_element_t<0, Tuple>> input{};
                        parse_into(input);
                        if constexpr (check_null_terminated(Opts)) {
                           if (bool(ctx.error)) [[unlikely]]
                              return;
                        }
                        else {
                           if (size_t(ctx.error) > size_t(error_code::end_reached)) [[unlikely]]
                              return;
                        }
                        from(std::move(input));
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
                  parse_into(from);
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
                     // Only the class is taken, so there is no input to parse - discard the value.
                     discard();
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     value.from(value.val);
                  }
                  else if constexpr (N > 1) {
                     std::decay_t<glz::tuple_element_t<1, Tuple>> input{};
                     parse_into(input);
                     if constexpr (check_null_terminated(Opts)) {
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                     else {
                        if (size_t(ctx.error) > size_t(error_code::end_reached)) [[unlikely]]
                           return;
                     }
                     if constexpr (N == 2) {
                        value.from(value.val, std::move(input));
                     }
                     else {
                        // Version that passes the glz::context for custom error handling
                        value.from(value.val, std::move(input), ctx);
                     }
                  }
               }
               else {
                  static_assert(false_v<T>, "lambda must have void return");
               }
            }
            else if constexpr (std::invocable<From, decltype(value.val)>) {
               decltype(auto) ref = value.from(value.val);
               parse_into(ref);
            }
            else if constexpr (std::invocable<From, decltype(value.val), context&>) {
               decltype(auto) ref = value.from(value.val, ctx);
               parse_into(ref);
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
   }
}

namespace glz::detail
{
   // Shared write dispatch for glz::custom. Split out so a format that must specialize
   // to<Format, custom_t> for its own reasons - BSON needs to expose a type_code - can reuse the
   // getter dispatch instead of restating it.
   template <uint32_t Format, auto Opts, class T>
   void custom_write(auto&& value, is_context auto&& ctx, auto&&... args)
   {
      using V = std::decay_t<decltype(value)>;
      using To = typename V::to_t;

      if constexpr (std::is_member_pointer_v<To>) {
         if constexpr (std::is_member_function_pointer_v<To>) {
            using Tuple = typename inputs_as_tuple<To>::type;
            if constexpr (glz::tuple_size_v<Tuple> == 0) {
               serialize<Format>::template op<Opts>((value.val.*(value.to))(), ctx, args...);
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
                     serialize<Format>::template op<Opts>(to(), ctx, args...);
                  }
                  else {
                     static_assert(false_v<T>, "std::function cannot have inputs");
                  }
               }
            }
            else {
               serialize<Format>::template op<Opts>(to, ctx, args...);
            }
         }
         else {
            static_assert(false_v<T>, "invalid type for custom");
         }
      }
      else {
         if constexpr (std::invocable<To, decltype(value.val)>) {
            serialize<Format>::template op<Opts>(std::invoke(value.to, value.val), ctx, args...);
         }
         else if constexpr (std::invocable<To, decltype(value.val), context&>) {
            serialize<Format>::template op<Opts>(std::invoke(value.to, value.val, ctx), ctx, args...);
         }
         else {
            static_assert(false_v<To>,
                          "expected invocable function, perhaps you need const qualified input on your lambda");
         }
      }
   }
}

namespace glz
{
   template <uint32_t Format, class T>
      requires(is_specialization_v<T, custom_t>)
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         detail::custom_read<Opts, T>(
            value, ctx,
            [&](auto& input) {
               glz::from<Format, std::decay_t<decltype(input)>>::template op<Opts>(input, ctx, it, end);
            },
            [&] { skip_value<Format>::template op<Opts>(ctx, it, end); });
      }
   };

   template <uint32_t Format, class T>
      requires(is_specialization_v<T, custom_t>)
   struct to<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         detail::custom_write<Format, Opts, T>(value, ctx, args...);
      }
   };
}
