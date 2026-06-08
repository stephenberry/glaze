// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/util/for_each.hpp"

namespace glz
{
   // Unified Format Driver Engine
   // This framework allows Glaze developers to implement a serialization format by writing
   // only a leaf/container-boundary driver, avoiding copy-pasting structural recursive logic.

   template <class Driver, auto Opts, class T>
   struct meta_serialize
   {
      template <class B, class IX>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, IX&& ix)
      {
         using V = std::remove_cvref_t<T>;

         if constexpr (nullable_like<V>) {
            if (bool(value)) {
               using val_t = std::remove_cvref_t<decltype(*value)>;
               meta_serialize<Driver, Opts, val_t>::op(*value, ctx, b, ix);
            }
            else {
               Driver::template write_null<Opts>(ctx, b, ix);
            }
         }
         else if constexpr (boolean_like<V>) {
            Driver::template write_bool<Opts>(value, ctx, b, ix);
         }
         else if constexpr (num_t<V>) {
            Driver::template write_number<Opts>(value, ctx, b, ix);
         }
         else if constexpr (str_t<V>) {
            Driver::template write_string<Opts>(value, ctx, b, ix);
         }
         else if constexpr (glaze_object_t<V> || reflectable<V>) {
            constexpr auto N = reflect<V>::size;
            Driver::template start_object<Opts>(N, ctx, b, ix);

            for_each<N>([&]<size_t I>() {
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               constexpr auto key = reflect<V>::keys[I];
               Driver::template write_object_key<Opts>(key, ctx, b, ix);

               using field_type = field_t<V, I>;
               decltype(auto) field_val = [&]() -> decltype(auto) {
                  if constexpr (reflectable<V>) {
                     return get_member(value, get<I>(to_tie(value)));
                  }
                  else {
                     return get_member(value, get<I>(reflect<V>::values));
                  }
               }();

               meta_serialize<Driver, Opts, field_type>::op(field_val, ctx, b, ix);

               if constexpr (I != N - 1) {
                  Driver::template write_object_separator<Opts>(ctx, b, ix);
               }
            });

            Driver::template end_object<Opts>(ctx, b, ix);
         }
         else if constexpr (writable_map_t<V>) {
            Driver::template start_object<Opts>(value.size(), ctx, b, ix);

            size_t i = 0;
            const size_t N = value.size();
            for (auto&& [k, v] : value) {
               if (bool(ctx.error)) [[unlikely]] {
                  break;
               }

               Driver::template write_object_key<Opts>(k, ctx, b, ix);

               using val_type = std::remove_cvref_t<decltype(v)>;
               meta_serialize<Driver, Opts, val_type>::op(v, ctx, b, ix);

               if (i != N - 1) {
                  Driver::template write_object_separator<Opts>(ctx, b, ix);
               }
               ++i;
            }

            Driver::template end_object<Opts>(ctx, b, ix);
         }
         else if constexpr (writable_array_t<V>) {
            Driver::template start_array<Opts>(value.size(), ctx, b, ix);

            size_t i = 0;
            const size_t N = value.size();
            for (auto&& item : value) {
               if (bool(ctx.error)) [[unlikely]] {
                  break;
               }

               using item_type = std::remove_cvref_t<decltype(item)>;
               meta_serialize<Driver, Opts, item_type>::op(item, ctx, b, ix);

               if (i != N - 1) {
                  Driver::template write_array_separator<Opts>(ctx, b, ix);
               }
               ++i;
            }

            Driver::template end_array<Opts>(ctx, b, ix);
         }
      }
   };
}
