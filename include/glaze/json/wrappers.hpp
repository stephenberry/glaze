// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <type_traits>

#include "glaze/core/custom.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/wrappers.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // treat a value as quoted to avoid double parsing into a value
   template <class T>
   struct quoted_t
   {
      static constexpr bool glaze_wrapper = true;
      using value_type = T;
      T& val;
   };

   namespace detail
   {
      template <class T>
      struct from<JSON, quoted_t<T>>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            static thread_local std::string s{};
            read<JSON>::op<Opts>(s, ctx, args...);
            auto pe = glz::read<Opts>(value.val, s);
            if (pe) [[unlikely]] {
               ctx.error = pe.ec;
            }
         }
      };

      template <class T>
      struct to<JSON, quoted_t<T>>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            static thread_local std::string s(128, ' ');
            size_t ix = 0; // overwrite index
            write<JSON>::op<Opts>(value.val, ctx, s, ix);
            s.resize(ix);
            write<JSON>::op<Opts>(s, ctx, args...);
         }
      };

      template <is_opts_wrapper T>
      struct from<JSON, T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, auto&&... args)
         {
            read<JSON>::op<opt_true<Opts, T::opts_member>>(value.val, args...);
         }
      };

      template <is_opts_wrapper T>
      struct to<JSON, T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args)
         {
            write<JSON>::op<opt_true<Opts, T::opts_member>>(value.val, ctx, args...);
         }
      };

      template <auto MemPtr>
      inline constexpr decltype(auto) quoted_impl() noexcept
      {
         return [](auto&& val) { return quoted_t<std::remove_reference_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
      }
   }

   // Read a value as a string and unescape, to avoid the user having to parse twice
   template <auto MemPtr>
   constexpr auto quoted = detail::quoted_impl<MemPtr>();
}
