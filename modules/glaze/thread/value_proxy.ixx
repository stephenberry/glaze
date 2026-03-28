// Glaze Library
// For the license information refer to glaze.ixx
export module glaze.thread.value_proxy;

import std;
import glaze.core.common;
import glaze.core.context;
import glaze.core.opts;
import glaze.json.read;

namespace glz
{
   export template <class T>
   concept is_value_proxy = requires { T::glaze_value_proxy; };

   export template <std::uint32_t Format, is_value_proxy T>
   struct from<Format, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         parse<JSON>::op<Opts>(value.value(), ctx, it, end);
      }
   };
}
