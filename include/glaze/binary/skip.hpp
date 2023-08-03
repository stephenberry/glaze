// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/core/format.hpp"
#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/dump.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct skip_binary
      {};

      // TODO: UNDER CONSTRUCTION
      template <class T>
         requires glaze_object_t<T>
      struct skip_binary<T> final
      {
         template <auto Opts>
         GLZ_FLATTEN static void op(is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            static constexpr uint8_t header =
               set_bits<5, 3, uint8_t>(set_bits<3, 2, uint8_t>(set_bits<3>(tag::object), 0), sizeof(decltype(*it)));

            const auto tag = uint8_t(*it);
            if (tag != header) {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it;

            const auto n_keys = int_from_compressed(it, end);

            static constexpr auto storage = detail::make_map<T, Opts.use_hash_comparison>();

            for (size_t i = 0; i < n_keys; ++i) {
               if (get_bits<3>(uint8_t(*it)) != tag::string) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               const auto length = int_from_compressed(it, end);

               const std::string_view key{it, length};

               std::advance(it, length);

               const auto value_tag = uint8_t(*it);

               const auto value_type = get_bits<3>(value_tag);
               switch (value_type) {
               case tag::null: {
                  break;
               }
               case tag::boolean: {
                  break;
               }
               case tag::number: {
                  break;
               }
               case tag::string: {
                  break;
               }
               case tag::object: {
                  break;
               }
               case tag::typed_array: {
                  break;
               }
               case tag::untyped_array: {
                  break;
               }
               case tag::type: {
                  break;
               }
               default: {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               }
            }
         }
      };
   }
}
