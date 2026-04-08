// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.core.custom;

import std;

import glaze.core.context;
import glaze.core.opts;
import glaze.core.read;
import glaze.core.write;
import glaze.tuplet;
import glaze.util.type_traits;
import glaze.core.common;

using std::uint32_t;
using std::size_t;

namespace glz
{
   template <uint32_t Format, class T>
      requires(is_specialization_v<T, custom_t>)
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         using V = std::decay_t<decltype(value)>;
         using From = typename V::from_t;

         if constexpr (std::same_as<From, skip>) {
            skip_value<Format>::template op<Opts>(ctx, it, end);
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
                     glz::from<Format, std::decay_t<decltype(input)>>::template op<Opts>(input, ctx, it, end);
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
                        skip_array<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                        from();
                     }
                     else if constexpr (glz::tuple_size_v<Tuple> == 1) {
                        std::decay_t<glz::tuple_element_t<0, Tuple>> input{};
                        glz::from<Format, std::decay_t<decltype(input)>>::template op<Opts>(input, ctx, it, end);
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
                  glz::from<Format, std::decay_t<decltype(from)>>::template op<Opts>(from, ctx, it, end);
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
                  else if constexpr (N > 1) {
                     std::decay_t<glz::tuple_element_t<1, Tuple>> input{};
                     glz::from<Format, std::decay_t<decltype(input)>>::template op<Opts>(input, ctx, it, end);
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
               glz::from<Format, std::decay_t<decltype(ref)>>::template op<Opts>(ref, ctx, it, end);
            }
            else if constexpr (std::invocable<From, decltype(value.val), context&>) {
               decltype(auto) ref = value.from(value.val, ctx);
               glz::from<Format, std::decay_t<decltype(ref)>>::template op<Opts>(ref, ctx, it, end);
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

   template <uint32_t Format, class T>
      requires(is_specialization_v<T, custom_t>)
   struct to<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args)
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
   };

   // treat a value as quoted to avoid double parsing into a value
   export template <class T>
   struct quoted_t
   {
      static constexpr bool glaze_wrapper = true;
      using value_type = T;
      T& val;
   };

   export template <class T>
   struct escape_bytes_t
   {
      static constexpr bool glaze_wrapper = true;
      using value_type = T;
      T& val;
   };

   export template <class T>
   escape_bytes_t(T&) -> escape_bytes_t<T>;

   export template <class T, auto OptsMemPtr>
   struct opts_wrapper_t
   {
      static constexpr bool glaze_wrapper = true;
      static constexpr auto glaze_reflect = false;
      static constexpr auto opts_member = OptsMemPtr;
      using value_type = T;
      T& val;
   };

   export template <class T>
   concept is_opts_wrapper = requires {
      requires T::glaze_wrapper == true;
      requires T::glaze_reflect == false;
      T::opts_member;
      typename T::value_type;
      requires std::is_lvalue_reference_v<decltype(T::val)>;
   };

   template <auto MemPtr, auto OptsMemPtr>
   inline constexpr decltype(auto) opts_wrapper() noexcept
   {
      return [](auto&& val) {
         using V = std::remove_reference_t<decltype(val.*MemPtr)>;
         return opts_wrapper_t<V, OptsMemPtr>{val.*MemPtr};
      };
   }

   // custom_t allows a user to register member functions, std::function members, and member variables
   // to implement custom reading and writing
   export template <class T, class From, class To>
   struct custom_t final
   {
      static constexpr auto glaze_reflect = false;
      using from_t = From;
      using to_t = To;
      T& val;
      From from;
      To to;
   };

   export template <class T, class From, class To>
   custom_t(T&, From, To) -> custom_t<T, From, To>;

   template <auto From, auto To>
   constexpr auto custom_impl() noexcept
   {
      return [](auto&& v) { return custom_t{v, From, To}; };
   }

   // When reading into an array that is appendable, the new data will be appended rather than overwrite
   export template <auto MemPtr>
   constexpr auto append_arrays = opts_wrapper<MemPtr, append_arrays_opt_tag{}>();

   // Read and write booleans as numbers
   export template <auto MemPtr>
   constexpr auto bools_as_numbers = opts_wrapper<MemPtr, bools_as_numbers_opt_tag{}>();

   // Read and write numbers as strings
   export template <auto MemPtr>
   constexpr auto quoted_num = opts_wrapper<MemPtr, quoted_num_opt_tag{}>();

   // Treat types like std::string as numbers: read and write them without quotes
   export template <auto MemPtr>
   constexpr auto string_as_number = opts_wrapper<MemPtr, string_as_number_opt_tag{}>();

   // Deprecated: use string_as_number instead
   export template <auto MemPtr>
   [[deprecated("Use glz::string_as_number instead of glz::number")]]
   constexpr auto number = opts_wrapper<MemPtr, string_as_number_opt_tag{}>();

   // Write out string like types without quotes
   export template <auto MemPtr>
   constexpr auto unquoted = opts_wrapper<MemPtr, unquoted_opt_tag{}>();

   // Deprecated: use unquoted instead
   export template <auto MemPtr>
   [[deprecated("Use glz::unquoted instead of glz::raw")]]
   constexpr auto raw = opts_wrapper<MemPtr, unquoted_opt_tag{}>();

   // Reads into only existing fields and elements and then exits without parsing the rest of the input
   export template <auto MemPtr>
   constexpr auto partial_read = opts_wrapper<MemPtr, &opts::partial_read>();

   // Customize reading and writing
   export template <auto From, auto To>
   constexpr auto custom = custom_impl<From, To>();

   template <auto MemPtr>
   inline constexpr decltype(auto) escape_bytes_impl() noexcept
   {
      return [](auto&& val) { return escape_bytes_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }

   // Treat char arrays as byte sequences to be fully escaped
   export template <auto MemPtr>
   constexpr auto escape_bytes = escape_bytes_impl<MemPtr>();

   // Limit string length or array size when reading
   // For strings: limits max_string_length
   // For arrays/vectors: limits max_array_size
   export template <class T, size_t MaxLen>
   struct max_length_t
   {
      static constexpr bool glaze_wrapper = true;
      static constexpr auto glaze_reflect = false;
      static constexpr size_t max_len = MaxLen;
      using value_type = T;
      T& val;
   };

   template <auto MemPtr, size_t MaxLen>
   inline constexpr decltype(auto) max_length_impl() noexcept
   {
      return [](auto&& val) {
         using V = std::remove_reference_t<decltype(val.*MemPtr)>;
         return max_length_t<V, MaxLen>{val.*MemPtr};
      };
   }

   // Limit string length or array size when reading from binary formats
   // Usage: glz::max_length<&T::field, 100>
   export template <auto MemPtr, size_t MaxLen>
   constexpr auto max_length = max_length_impl<MemPtr, MaxLen>();
}
