// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/msgpack/wrappers.hpp"
// glz:header std=<cstdint>
// glz:header std=<type_traits>
export module glaze.msgpack.wrappers;

import std;
import glaze.util.type_traits;
import glaze.core.opts;
import glaze.core.context;
import glaze.core.custom;
import glaze.msgpack.read;
import glaze.msgpack.skip;

using std::uint8_t;

export namespace glz
{
   // MessagePack readers receive the format tag that the caller already consumed, so glz::custom
   // needs its own entry point here; the generic one in core/custom.hpp cannot pass the tag along.
   // The write side needs nothing extra - to<Format, custom_t> is already format independent.
   template <class T>
      requires(is_specialization_v<T, custom_t>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end)
      {
         detail::dispatch_custom_read<T>(
            value, ctx,
            [&](auto& input) {
               from<MSGPACK, std::decay_t<decltype(input)>>::template op<Opts>(input, tag, ctx, it, end);
            },
            [&] { skip_value<MSGPACK>::template op<Opts>(tag, ctx, it, end); });
      }
   };
}
