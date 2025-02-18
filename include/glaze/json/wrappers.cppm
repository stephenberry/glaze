// Glaze Library
// For the license information refer to glaze.hpp

#pragma once
#ifdef CPP_MODULES
module;
#endif
#include "../../Export.hpp"
#include <type_traits>
#ifdef CPP_MODULES
export module glaze.json.wrappers;
import glaze.core.custom;
import glaze.core.opts;
import glaze.core.wrappers;
import glaze.json.read;
import glaze.json.write;
#else
#include "glaze/core/custom.cppm"
#include "glaze/core/opts.cppm"
#include "glaze/core/wrappers.cppm"
#include "glaze/json/read.cppm"
#include "glaze/json/write.cppm"
#endif





namespace glz
{
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
         static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix)
         {
            static thread_local std::string s(128, ' ');
            size_t oix = 0; // overwrite index
            using Value = core_t<decltype(value.val)>;
            to<JSON, Value>::template op<Opts>(value.val, ctx, s, oix);
            s.resize(oix);
            using S = core_t<decltype(s)>;
            to<JSON, S>::template op<Opts>(s, ctx, b, ix);
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
            using Value = core_t<decltype(value.val)>;
            to<JSON, Value>::template op<opt_true<Opts, T::opts_member>>(value.val, ctx, args...);
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
