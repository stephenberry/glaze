// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json.hpp"

namespace glz
{
   template <class T>
   struct quoted_t
   {
      T& val;
   };

   namespace detail
   {
      template <class T>
      struct from_json<quoted_t<T>>
      {
         template <auto Opts>
         static void op(auto&& value, auto&&... args)
         {
            skip_ws<Opts>(args...);
            match<'"'>(args...);
            read<json>::op<Opts>(value.val, args...);
            match<'"'>(args...);
         }
      };

      template <class T>
      struct to_json<quoted_t<T>>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            dump<'"'>(args...);
            write<json>::op<Opts>(value.val, ctx, args...);
            dump<'"'>(args...);
         }
      };
   }
   
   template <auto MemPtr>
   inline constexpr decltype(auto) qouted() noexcept
   {
      return [](auto&& val) { return quoted_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }
}
