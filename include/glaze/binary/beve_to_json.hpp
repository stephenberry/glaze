// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail
   {
      template <opts Opts>
      inline void beve_to_json_number(auto&& tag, auto&& ctx, auto&& it, auto&&, auto& out, auto&& ix) noexcept
      {
         const auto number_type = (tag & 0b000'11'000) >> 3;
         const uint8_t byte_count = detail::byte_count_lookup[tag >> 5];

         auto write_number = [&]<class T>(T&& value) {
            std::memcpy(&value, it, sizeof(T));
            to_json<T>::template op<Opts>(value, ctx, out, ix);
            it += sizeof(T);
         };

         switch (number_type) {
         case 0: {
            // floating point
            switch (byte_count) {
            case 4: {
               write_number(float{});
               break;
            }
            case 8: {
               write_number(double{});
               break;
            }
            default: {
               ctx.error = error_code::syntax_error;
               return;
            }
            }
            break;
         }
         case 1: {
            // signed integer
            switch (byte_count) {
            case 1: {
               write_number(int8_t{});
               break;
            }
            case 2: {
               write_number(int16_t{});
               break;
            }
            case 4: {
               write_number(int32_t{});
               break;
            }
            case 8: {
               write_number(int64_t{});
               break;
            }
            default: {
               ctx.error = error_code::syntax_error;
               return;
            }
            }
            break;
         }
         case 2: {
            // unsigned integer
            switch (byte_count) {
            case 1: {
               write_number(uint8_t{});
               break;
            }
            case 2: {
               write_number(uint16_t{});
               break;
            }
            case 4: {
               write_number(uint32_t{});
               break;
            }
            case 8: {
               write_number(uint64_t{});
               break;
            }
            default: {
               ctx.error = error_code::syntax_error;
               return;
            }
            }
            break;
         }
         default: {
            ctx.error = error_code::syntax_error;
            return;
         }
         }
      }

      template <glz::opts Opts, class Buffer>
      inline void beve_to_json_value(auto&& ctx, auto&& it, auto&& end, Buffer& out, auto&& ix) noexcept
      {
         const auto tag = uint8_t(*it);
         const auto type = tag & 0b00000'111;
         switch (type) {
         case tag::null: {
            if (tag & tag::boolean) {
               if (tag >> 4) {
                  dump<"true">(out, ix);
               }
               else {
                  dump<"false">(out, ix);
               }
            }
            else {
               dump<"null">(out, ix);
            }
            ++it;
            break;
         }
         case tag::number: {
            ++it;
            beve_to_json_number<Opts>(tag, ctx, it, end, out, ix);
            if (bool(ctx.error)) return;
            break;
         }
         case tag::string: {
            ++it;
            const auto n = detail::int_from_compressed(ctx, it, end);
            const sv value{reinterpret_cast<const char*>(it), n};
            to_json<sv>::template op<Opts>(value, ctx, out, ix);
            it += n;
            break;
         }
         case tag::object: {
            ++it;

            dump<'{'>(out, ix);
            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(out, ix);
               dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
            }

            const auto key_type = (tag & 0b000'11'000) >> 3;
            switch (key_type) {
            case 0: {
               // string key
               const auto n_fields = detail::int_from_compressed(ctx, it, end);
               for (size_t i = 0; i < n_fields; ++i) {
                  // convert the key
                  const auto n = detail::int_from_compressed(ctx, it, end);
                  const sv key{reinterpret_cast<const char*>(it), n};
                  to_json<sv>::template op<Opts>(key, ctx, out, ix);
                  if constexpr (Opts.prettify) {
                     dump<": ">(out, ix);
                  }
                  else {
                     dump<':'>(out, ix);
                  }
                  it += n;
                  // convert the value
                  beve_to_json_value<Opts>(ctx, it, end, out, ix);
                  if (i != n_fields - 1) {
                     dump<','>(out, ix);
                     if constexpr (Opts.prettify) {
                        dump<'\n'>(out, ix);
                        dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
                     }
                  }
               }
               break;
            }
            default: {
               ctx.error = error_code::syntax_error;
               return;
            }
            }

            if constexpr (Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(out, ix);
               dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
            }
            dump<'}'>(out, ix);
            break;
         }
         case tag::typed_array: {
            ++it;
            const auto value_type = (tag & 0b000'11'000) >> 3;
            const uint8_t byte_count = detail::byte_count_lookup[tag >> 5];

            auto write_array = [&]<class T>(T&& value) {
               const auto n = int_from_compressed(ctx, it, end);
               for (size_t i = 0; i < n; ++i) {
                  std::memcpy(&value, it, sizeof(T));
                  to_json<T>::template op<Opts>(value, ctx, out, ix);
                  it += sizeof(T);
                  if (i != n - 1) {
                     dump<','>(out, ix);
                  }
               }
            };

            dump<'['>(out, ix);

            switch (value_type) {
            case 0: {
               // floating point
               switch (byte_count) {
               case 4: {
                  write_array(float{});
                  break;
               }
               case 8: {
                  write_array(double{});
                  break;
               }
               default: {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               }
               break;
            }
            case 1: {
               // signed integer
               switch (byte_count) {
               case 1: {
                  write_array(int8_t{});
                  break;
               }
               case 2: {
                  write_array(int16_t{});
                  break;
               }
               case 4: {
                  write_array(int32_t{});
                  break;
               }
               case 8: {
                  write_array(int64_t{});
                  break;
               }
               default: {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               }
               break;
            }
            case 2: {
               // unsigned integer
               switch (byte_count) {
               case 1: {
                  write_array(uint8_t{});
                  break;
               }
               case 2: {
                  write_array(uint16_t{});
                  break;
               }
               case 4: {
                  write_array(uint32_t{});
                  break;
               }
               case 8: {
                  write_array(uint64_t{});
                  break;
               }
               default: {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               }
               break;
            }
            case 3: {
               // string or boolean
               const auto string_or_boolean = (tag & 0b001'00'000) >> 5;
               switch (string_or_boolean) {
               case 0: {
                  // boolean array (bit packed)
                  // TODO: implement
                  ctx.error = error_code::syntax_error;
                  break;
               }
               case 1: {
                  // array of strings
                  const auto n_strings = int_from_compressed(ctx, it, end);
                  for (size_t i = 0; i < n_strings; ++i) {
                     const auto n = detail::int_from_compressed(ctx, it, end);
                     const sv value{reinterpret_cast<const char*>(it), n};
                     to_json<sv>::template op<Opts>(value, ctx, out, ix);
                     it += n;
                     if (i != n_strings - 1) {
                        dump<','>(out, ix);
                     }
                  }
                  break;
               }
               default: {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               }
               break;
            }
            default: {
               ctx.error = error_code::syntax_error;
               return;
            }
            }

            dump<']'>(out, ix);

            break;
         }
         case tag::generic_array: {
            ++it;
            const auto n = int_from_compressed(ctx, it, end);
            dump<'['>(out, ix);
            for (size_t i = 0; i < n; ++i) {
               beve_to_json_value<Opts>(ctx, it, end, out, ix);
               if (i != n - 1) {
                  dump<','>(out, ix);
               }
            }
            dump<']'>(out, ix);
            break;
         }
         case tag::extensions: {
            const uint8_t extension = tag >> 3;
            switch (extension) {
            case 0: {
               // delimiter
               ++it;
               dump<'\n'>(out, ix);
               break;
            }
            case 1: {
               // variants
               ++it;
               const auto index = int_from_compressed(ctx, it, end);

               dump<'{'>(out, ix);
               if constexpr (Opts.prettify) {
                  ctx.indentation_level += Opts.indentation_width;
                  dump<'\n'>(out, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
               }

               if constexpr (Opts.prettify) {
                  dump<R"("index": )">(out, ix);
               }
               else {
                  dump<R"("index":)">(out, ix);
               }

               to_json<std::remove_cvref_t<decltype(index)>>::template op<Opts>(index, ctx, out, ix);

               dump<','>(out, ix);
               if constexpr (Opts.prettify) {
                  dump<'\n'>(out, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
               }

               if constexpr (Opts.prettify) {
                  dump<R"("value": )">(out, ix);
               }
               else {
                  dump<R"("value":)">(out, ix);
               }

               beve_to_json_value<Opts>(ctx, it, end, out, ix);

               if constexpr (Opts.prettify) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump<'\n'>(out, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
               }
               dump<'}'>(out, ix);
               break;
            }
            case 2: {
               // matrices
               ++it;
               const auto matrix_header = uint8_t(*it);
               ++it;

               dump<'{'>(out, ix);
               if constexpr (Opts.prettify) {
                  ctx.indentation_level += Opts.indentation_width;
                  dump<'\n'>(out, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
               }

               if constexpr (Opts.prettify) {
                  dump<R"("layout": )">(out, ix);
               }
               else {
                  dump<R"("layout":)">(out, ix);
               }

               const auto layout = matrix_header & 0b0000000'1;
               layout ? dump<R"("layout_right")">(out, ix) : dump<R"("layout_left")">(out, ix);

               dump<','>(out, ix);
               if constexpr (Opts.prettify) {
                  dump<'\n'>(out, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
               }

               if constexpr (Opts.prettify) {
                  dump<R"("extents": )">(out, ix);
               }
               else {
                  dump<R"("extents":)">(out, ix);
               }

               beve_to_json_value<Opts>(ctx, it, end, out, ix);

               dump<','>(out, ix);
               if constexpr (Opts.prettify) {
                  dump<'\n'>(out, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
               }

               if constexpr (Opts.prettify) {
                  dump<R"("value": )">(out, ix);
               }
               else {
                  dump<R"("value":)">(out, ix);
               }

               beve_to_json_value<Opts>(ctx, it, end, out, ix);

               if constexpr (Opts.prettify) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump<'\n'>(out, ix);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, out, ix);
               }
               dump<'}'>(out, ix);
               break;
            }
            case 3: {
               // complex numbers
               ++it;
               const auto complex_header = uint8_t(*it);
               ++it;

               const auto complex_type = complex_header & 0b0000000'1;
               if (complex_type) {
                  // complex array
                  const auto number_tag = complex_header & 0b111'00000;
                  const auto n = int_from_compressed(ctx, it, end);
                  dump<'['>(out, ix);
                  for (size_t i = 0; i < n; ++i) {
                     dump<'['>(out, ix);
                     beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                     dump<','>(out, ix);
                     beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                     dump<']'>(out, ix);
                     if (i != n - 1) {
                        dump<','>(out, ix);
                     }
                  }
                  dump<']'>(out, ix);
               }
               else {
                  // complex number
                  const auto number_tag = complex_header & 0b111'00000;
                  dump<'['>(out, ix);
                  beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                  dump<','>(out, ix);
                  beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                  dump<']'>(out, ix);
               }

               break;
            }
            default: {
               ctx.error = error_code::syntax_error;
               return;
            }
            }
            break;
         }
         default: {
            ctx.error = error_code::syntax_error;
            return;
         }
         }
      }
   }

   template <glz::opts Opts = glz::opts{}, class BEVEBuffer, class JSONBuffer>
   inline write_error beve_to_json(const BEVEBuffer& beve, JSONBuffer& out) noexcept
   {
      size_t ix{}; // write index

      auto* it = beve.data();
      auto* end = it + beve.size();

      context ctx{};

      while (it < end) {
         detail::beve_to_json_value<Opts>(ctx, it, end, out, ix);
         if (bool(ctx.error)) {
            return {ctx.error};
         }
      }

      if constexpr (resizable<JSONBuffer>) {
         out.resize(ix);
      }

      return {};
   }
}
