// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/beve/beve_to_json.hpp"
// glz:header std=<bit>
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<cstring>
// glz:header std=<type_traits>
export module glaze.beve.beve_to_json;

import std;

import glaze.beve.header;

import glaze.core.context;
import glaze.core.opts;

import glaze.concepts.container_concepts;

import glaze.json.write;

import glaze.util.dump;
import glaze.util.string_literal;

using std::int8_t;
using std::uint8_t;
using std::int16_t;
using std::uint16_t;
using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;
using std::size_t;

namespace glz
{
   namespace detail
   {
      template <auto Opts>
      inline void beve_to_json_number(auto&& tag, auto&& ctx, auto&& it, auto&& end, auto& out, auto& ix) noexcept
      {
         const auto number_type = (tag & 0b000'11'000) >> 3;
         const uint8_t byte_count = byte_count_lookup[tag >> 5];

         auto write_number = [&]<class T>(T&& value) {
            if (size_t(end - it) < sizeof(T)) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            using V = std::remove_cvref_t<T>;
            std::memcpy(&value, it, sizeof(V));
            if constexpr (std::endian::native == std::endian::big) {
               byteswap_le(value);
            }
            to<JSON, T>::template op<Opts>(value, ctx, out, ix);
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

      template <auto Opts, class Buffer>
      inline void beve_to_json_value(auto&& ctx, auto&& it, auto&& end, Buffer& out, auto&& ix,
                                     uint32_t recursive_depth)
      {
         // Check recursion depth limit
         if (recursive_depth >= max_recursive_depth_limit) [[unlikely]] {
            ctx.error = error_code::exceeded_max_recursive_depth;
            return;
         }

         if (it >= end) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         const auto tag = uint8_t(*it);
         const auto type = tag & 0b00000'111;
         switch (type) {
         case tag::null: {
            if (tag & tag::boolean) {
               if (tag >> 4) {
                  if (!emit_literal<"true">(ctx, out, ix)) return;
               }
               else {
                  if (!emit_literal<"false">(ctx, out, ix)) return;
               }
            }
            else {
               if (!emit_literal<"null">(ctx, out, ix)) return;
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
            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (uint64_t(end - it) < n) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }
            const sv value{reinterpret_cast<const char*>(it), n};
            detail::emit_untrusted_string<Opts>(ctx, value, out, ix);
            if (bool(ctx.error)) [[unlikely]]
               return;
            it += n;
            break;
         }
         case tag::object: {
            ++it;

            if (!emit_char(ctx, '{', out, ix)) return;
            {
               // The indentation this object adds belongs to it, so every way out -- including an
               // error return from a nested value -- puts the depth back.
               const indent_guard indent{ctx, Opts.prettify ? check_indentation_width(Opts) : 1};

               if constexpr (Opts.prettify) {
                  if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
               }

               const auto key_type = (tag & 0b000'11'000) >> 3;
               switch (key_type) {
               case 0: {
                  // string key
                  const auto n_fields = int_from_compressed(ctx, it, end);
                  if (bool(ctx.error)) {
                     return;
                  }
                  for (size_t i = 0; i < n_fields; ++i) {
                     // convert the key
                     const auto n = int_from_compressed(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (uint64_t(end - it) < n) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                     const sv key{reinterpret_cast<const char*>(it), n};
                     detail::emit_untrusted_string<Opts>(ctx, key, out, ix);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     if constexpr (Opts.prettify) {
                        if (!emit_literal<": ">(ctx, out, ix)) return;
                     }
                     else {
                        if (!emit_char(ctx, ':', out, ix)) return;
                     }
                     it += n;
                     beve_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (i != n_fields - 1) {
                        if (!emit_char(ctx, ',', out, ix)) return;
                        if constexpr (Opts.prettify) {
                           if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
                        }
                     }
                  }
                  break;
               }
               case 1:
                  [[fallthrough]]; // signed integer key
               case 2: {
                  // unsigned integer key
                  const auto n_fields = int_from_compressed(ctx, it, end);
                  if (bool(ctx.error)) {
                     return;
                  }
                  for (size_t i = 0; i < n_fields; ++i) {
                     // convert the key
                     if (!emit_char(ctx, '"', out, ix)) return;
                     beve_to_json_number<Opts>(tag, ctx, it, end, out, ix);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (!emit_char(ctx, '"', out, ix)) return;
                     if constexpr (Opts.prettify) {
                        if (!emit_literal<": ">(ctx, out, ix)) return;
                     }
                     else {
                        if (!emit_char(ctx, ':', out, ix)) return;
                     }
                     beve_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (i != n_fields - 1) {
                        if (!emit_char(ctx, ',', out, ix)) return;
                        if constexpr (Opts.prettify) {
                           if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
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
            }

            if constexpr (Opts.prettify) {
               if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
            }
            if (!emit_char(ctx, '}', out, ix)) return;
            break;
         }
         case tag::typed_array: {
            // Check for aligned typed array (tag == 0x5C)
            if (tag == tag::aligned_typed_array) {
               ++it; // skip aligned header
               if (it >= end) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               // Read the numeric header byte
               const auto numeric_tag = uint8_t(*it);
               if ((numeric_tag & 0b00000'111) != tag::typed_array) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it; // skip numeric header
               const auto value_type_inner = (numeric_tag & 0b000'11'000) >> 3;
               const uint8_t byte_count_inner = byte_count_lookup[numeric_tag >> 5];

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               // Read padding length byte and skip padding
               if (it >= end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               const uint8_t padding = uint8_t(*it);
               ++it;
               if (padding >= byte_count_inner) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if (typed_array_out_of_bounds(ctx, it, end, n, byte_count_inner, padding)) return;
               it += padding;

               // Now decode as a normal numeric typed array
               if (!emit_char(ctx, '[', out, ix)) return;

               auto write_aligned_array = [&]<class T>(T&& value) {
                  for (size_t i = 0; i < n; ++i) {
                     if (size_t(end - it) < sizeof(T)) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                     using V = std::remove_cvref_t<T>;
                     std::memcpy(&value, it, sizeof(V));
                     if constexpr (std::endian::native == std::endian::big) {
                        byteswap_le(value);
                     }
                     to<JSON, T>::template op<Opts>(value, ctx, out, ix);
                     it += sizeof(T);
                     if (i != n - 1) {
                        if (!emit_char(ctx, ',', out, ix)) return;
                     }
                  }
               };

               switch (value_type_inner) {
               case 0: {
                  switch (byte_count_inner) {
                  case 4:
                     write_aligned_array(float{});
                     break;
                  case 8:
                     write_aligned_array(double{});
                     break;
                  default:
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  break;
               }
               case 1: {
                  switch (byte_count_inner) {
                  case 1:
                     write_aligned_array(int8_t{});
                     break;
                  case 2:
                     write_aligned_array(int16_t{});
                     break;
                  case 4:
                     write_aligned_array(int32_t{});
                     break;
                  case 8:
                     write_aligned_array(int64_t{});
                     break;
                  default:
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  break;
               }
               case 2: {
                  switch (byte_count_inner) {
                  case 1:
                     write_aligned_array(uint8_t{});
                     break;
                  case 2:
                     write_aligned_array(uint16_t{});
                     break;
                  case 4:
                     write_aligned_array(uint32_t{});
                     break;
                  case 8:
                     write_aligned_array(uint64_t{});
                     break;
                  default:
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  break;
               }
               default:
                  ctx.error = error_code::syntax_error;
                  return;
               }

               if (!emit_char(ctx, ']', out, ix)) return;
               break;
            }

            ++it;
            const auto value_type = (tag & 0b000'11'000) >> 3;
            const uint8_t byte_count = byte_count_lookup[tag >> 5];

            auto write_array = [&]<class T>(T&& value) {
               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               for (size_t i = 0; i < n; ++i) {
                  if (size_t(end - it) < sizeof(T)) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  using V = std::remove_cvref_t<T>;
                  std::memcpy(&value, it, sizeof(V));
                  if constexpr (std::endian::native == std::endian::big) {
                     byteswap_le(value);
                  }
                  to<JSON, T>::template op<Opts>(value, ctx, out, ix);
                  it += sizeof(T);
                  if (i != n - 1) {
                     if (!emit_char(ctx, ',', out, ix)) return;
                  }
               }
            };

            if (!emit_char(ctx, '[', out, ix)) return;

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
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  for (size_t i = 0; i < n_strings; ++i) {
                     const auto n = int_from_compressed(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (uint64_t(end - it) < n) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                     const sv value{reinterpret_cast<const char*>(it), n};
                     detail::emit_untrusted_string<Opts>(ctx, value, out, ix);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     it += n;
                     if (i != n_strings - 1) {
                        if (!emit_char(ctx, ',', out, ix)) return;
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

            if (!emit_char(ctx, ']', out, ix)) return;

            break;
         }
         case tag::generic_array: {
            ++it;
            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (!emit_char(ctx, '[', out, ix)) return;
            for (size_t i = 0; i < n; ++i) {
               beve_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               if (i != n - 1) {
                  if (!emit_char(ctx, ',', out, ix)) return;
               }
            }
            if (!emit_char(ctx, ']', out, ix)) return;
            break;
         }
         case tag::extensions: {
            const uint8_t extension = tag >> 3;
            switch (extension) {
            case 0: {
               // delimiter
               ++it;
               if (!emit_char(ctx, '\n', out, ix)) return;
               break;
            }
            case 1: {
               // legacy (Version 1) type tag: transcode by dropping the positional index and
               // emitting the value. Version 2 variants are ordinary objects/values transcoded by
               // their normal cases and never reach here.
               ++it;
               skip_compressed_int(ctx, it, end);
               if (bool(ctx.error)) return;

               beve_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               break;
            }
            case 2: {
               // matrices
               ++it;
               if (it >= end) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               const auto matrix_header = uint8_t(*it);
               ++it;

               if (!emit_char(ctx, '{', out, ix)) return;
               {
                  // The indentation this matrix adds belongs to it, so every way out -- including an
                  // error return from a nested value -- puts the depth back.
                  const indent_guard indent{ctx, Opts.prettify ? check_indentation_width(Opts) : 1};

                  if constexpr (Opts.prettify) {
                     if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
                  }

                  if constexpr (Opts.prettify) {
                     if (!emit_literal<R"("layout": )">(ctx, out, ix)) return;
                  }
                  else {
                     if (!emit_literal<R"("layout":)">(ctx, out, ix)) return;
                  }

                  const auto layout = matrix_header & 0b0000000'1;
                  if (layout) {
                     if (!emit_literal<R"("layout_right")">(ctx, out, ix)) return;
                  }
                  else {
                     if (!emit_literal<R"("layout_left")">(ctx, out, ix)) return;
                  }

                  if (!emit_char(ctx, ',', out, ix)) return;
                  if constexpr (Opts.prettify) {
                     if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
                  }

                  if constexpr (Opts.prettify) {
                     if (!emit_literal<R"("extents": )">(ctx, out, ix)) return;
                  }
                  else {
                     if (!emit_literal<R"("extents":)">(ctx, out, ix)) return;
                  }

                  beve_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  if (!emit_char(ctx, ',', out, ix)) return;
                  if constexpr (Opts.prettify) {
                     if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
                  }

                  if constexpr (Opts.prettify) {
                     if (!emit_literal<R"("value": )">(ctx, out, ix)) return;
                  }
                  else {
                     if (!emit_literal<R"("value":)">(ctx, out, ix)) return;
                  }

                  beve_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
               }

               if constexpr (Opts.prettify) {
                  if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
               }
               if (!emit_char(ctx, '}', out, ix)) return;
               break;
            }
            case 3: {
               // complex numbers
               ++it;
               if (it >= end) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               const auto complex_header = uint8_t(*it);
               ++it;

               const auto complex_type = complex_header & 0b0000000'1;
               if (complex_type) {
                  // complex array
                  const auto number_tag = complex_header & 0b111'00000;
                  const auto n = int_from_compressed(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  if (!emit_char(ctx, '[', out, ix)) return;
                  for (size_t i = 0; i < n; ++i) {
                     if (!emit_char(ctx, '[', out, ix)) return;
                     beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (!emit_char(ctx, ',', out, ix)) return;
                     beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if (!emit_char(ctx, ']', out, ix)) return;
                     if (i != n - 1) {
                        if (!emit_char(ctx, ',', out, ix)) return;
                     }
                  }
                  if (!emit_char(ctx, ']', out, ix)) return;
               }
               else {
                  // complex number
                  const auto number_tag = complex_header & 0b111'00000;
                  if (!emit_char(ctx, '[', out, ix)) return;
                  beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  if (!emit_char(ctx, ',', out, ix)) return;
                  beve_to_json_number<Opts>(number_tag, ctx, it, end, out, ix);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  if (!emit_char(ctx, ']', out, ix)) return;
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

   // Convert a BEVE buffer directly to JSON without intermediate C++ types
   //
   // An empty buffer holds no value, which is not a document, and is reported rather than
   // converted into empty output.
   //
   // A buffer holding several values converts to one JSON document per line, the way NDJSON
   // separates its documents. A delimiter tag writes that newline itself. Where a stream was
   // concatenated without delimiters -- which read_beve_delimited also accepts -- the newline is
   // written here instead, so two values never run together into text that is no longer JSON.
   export template <auto Opts = glz::opts{}, class BEVEBuffer, class JSONBuffer>
   [[nodiscard]] inline error_ctx beve_to_json(const BEVEBuffer& beve, JSONBuffer& out)
   {
      size_t ix{}; // write index

      auto* it = beve.data();
      auto* end = it + beve.size();

      context ctx{};

      if (it >= end) {
         return {0, error_code::unexpected_end};
      }

      bool needs_separator = false;

      while (it < end) {
         const bool is_delimiter = uint8_t(*it) == tag::delimiter;

         if (needs_separator && !is_delimiter) {
            if (!detail::emit_char(ctx, '\n', out, ix)) {
               return {ix, ctx.error};
            }
         }
         needs_separator = !is_delimiter;

         detail::beve_to_json_value<Opts>(ctx, it, end, out, ix, 0);
         if (bool(ctx.error)) {
            return {ix, ctx.error};
         }
      }

      if constexpr (resizable<JSONBuffer>) {
         out.resize(ix);
      }

      // count is the number of bytes written. A resizable buffer carries its own size, but a
      // fixed-size one has no other way to learn how much of it now holds JSON.
      return {ix};
   }
}
