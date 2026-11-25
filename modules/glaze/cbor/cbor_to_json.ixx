// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/cbor/cbor_to_json.hpp"
// glz:header std=<bit>
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<cstring>
// glz:header std=<string>
// glz:header std=<string_view>
// glz:header std=<vector>
export module glaze.cbor.cbor_to_json;

import glaze.cbor.header;

import glaze.core.opts;
import glaze.core.context;
import glaze.concepts.container_concepts;
import glaze.json.write;
import glaze.util.expected;
import glaze.util.string_literal;
import glaze.util.dump;

import std;

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
      // Decode CBOR argument (variable-length unsigned integer)
      inline uint64_t cbor_to_json_decode_arg(is_context auto& ctx, auto& it, auto end, uint8_t additional_info) noexcept
      {
         using namespace cbor;

         if (additional_info < 24) {
            return additional_info;
         }

         switch (additional_info) {
         case info::uint8_follows: {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return 0;
            }
            uint8_t val;
            std::memcpy(&val, it, 1);
            ++it;
            return val;
         }
         case info::uint16_follows: {
            if ((end - it) < 2) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return 0;
            }
            uint16_t val;
            std::memcpy(&val, it, 2);
            if constexpr (std::endian::native == std::endian::little) {
               val = std::byteswap(val);
            }
            it += 2;
            return val;
         }
         case info::uint32_follows: {
            if ((end - it) < 4) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return 0;
            }
            uint32_t val;
            std::memcpy(&val, it, 4);
            if constexpr (std::endian::native == std::endian::little) {
               val = std::byteswap(val);
            }
            it += 4;
            return val;
         }
         case info::uint64_follows: {
            if ((end - it) < 8) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return 0;
            }
            uint64_t val;
            std::memcpy(&val, it, 8);
            if constexpr (std::endian::native == std::endian::little) {
               val = std::byteswap(val);
            }
            it += 8;
            return val;
         }
         default:
            ctx.error = error_code::syntax_error;
            return 0;
         }
      }

      template <auto Opts, class Buffer>
      inline void cbor_to_json_key(auto&& ctx, auto&& it, auto&& end, Buffer& out, auto&& ix, uint32_t recursive_depth);

      template <auto Opts, class Buffer>
      inline void cbor_to_json_value(auto&& ctx, auto&& it, auto&& end, Buffer& out, auto&& ix,
                                     uint32_t recursive_depth)
      {
         using namespace cbor;

         // Check recursion depth limit
         if (recursive_depth >= max_recursive_depth_limit) [[unlikely]] {
            ctx.error = error_code::exceeded_max_recursive_depth;
            return;
         }

         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         uint8_t initial;
         std::memcpy(&initial, it, 1);
         ++it;

         const uint8_t major_type = get_major_type(initial);
         const uint8_t additional_info = get_additional_info(initial);

         switch (major_type) {
         case major::uint: {
            // Unsigned integer
            const uint64_t value = cbor_to_json_decode_arg(ctx, it, end, additional_info);
            if (bool(ctx.error)) [[unlikely]]
               return;
            to<JSON, uint64_t>::template op<Opts>(value, ctx, out, ix);
            break;
         }

         case major::nint: {
            // Negative integer: -1 - n
            const uint64_t n = cbor_to_json_decode_arg(ctx, it, end, additional_info);
            if (bool(ctx.error)) [[unlikely]]
               return;
            // Use two's complement trick for safe conversion
            const int64_t value = static_cast<int64_t>(~n);
            to<JSON, int64_t>::template op<Opts>(value, ctx, out, ix);
            break;
         }

         case major::bstr: {
            // Byte string - written as a JSON string of hex digit pairs (see emit_hex_bytes)
            if (!emit_char(ctx, '"', out, ix)) return;

            if (additional_info == info::indefinite) {
               // Indefinite-length byte string - each chunk is hex encoded as it is read, so a long
               // byte string never has to be assembled in memory first
               while (true) {
                  if (it >= end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  uint8_t chunk_initial;
                  std::memcpy(&chunk_initial, it, 1);

                  if (chunk_initial == initial_byte(major::simple, simple::break_code)) {
                     ++it;
                     break;
                  }

                  ++it;
                  const uint8_t chunk_major = get_major_type(chunk_initial);
                  const uint8_t chunk_info = get_additional_info(chunk_initial);

                  if (chunk_major != major::bstr || chunk_info == info::indefinite) [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return;
                  }

                  const uint64_t chunk_len = cbor_to_json_decode_arg(ctx, it, end, chunk_info);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  if (static_cast<uint64_t>(end - it) < chunk_len) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }

                  if (!emit_hex_bytes(ctx, it, chunk_len, out, ix)) return;
                  it += chunk_len;
               }
            }
            else {
               const uint64_t length = cbor_to_json_decode_arg(ctx, it, end, additional_info);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (static_cast<uint64_t>(end - it) < length) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if (!emit_hex_bytes(ctx, it, length, out, ix)) return;
               it += length;
            }

            if (!emit_char(ctx, '"', out, ix)) return;
            break;
         }

         case major::tstr: {
            // Text string
            if (additional_info == info::indefinite) {
               // Indefinite-length text string - concatenate chunks
               std::string str;
               while (true) {
                  if (it >= end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  uint8_t chunk_initial;
                  std::memcpy(&chunk_initial, it, 1);

                  if (chunk_initial == initial_byte(major::simple, simple::break_code)) {
                     ++it;
                     break;
                  }

                  ++it;
                  const uint8_t chunk_major = get_major_type(chunk_initial);
                  const uint8_t chunk_info = get_additional_info(chunk_initial);

                  if (chunk_major != major::tstr || chunk_info == info::indefinite) [[unlikely]] {
                     ctx.error = error_code::syntax_error;
                     return;
                  }

                  const uint64_t chunk_len = cbor_to_json_decode_arg(ctx, it, end, chunk_info);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  if (static_cast<uint64_t>(end - it) < chunk_len) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }

                  str.append(reinterpret_cast<const char*>(it), chunk_len);
                  it += chunk_len;
               }
               detail::emit_untrusted_string<Opts>(ctx, str, out, ix);
            }
            else {
               const uint64_t length = cbor_to_json_decode_arg(ctx, it, end, additional_info);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (static_cast<uint64_t>(end - it) < length) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               const sv value{reinterpret_cast<const char*>(it), static_cast<size_t>(length)};
               detail::emit_untrusted_string<Opts>(ctx, value, out, ix);
               it += length;
            }
            break;
         }

         case major::array: {
            // The elements of an array stay on one line, so a separator is ',' or ", "
            const auto convert_element = [&](const bool needs_comma) {
               if (needs_comma) {
                  if constexpr (Opts.prettify) {
                     if (!emit_literal<", ">(ctx, out, ix)) return;
                  }
                  else {
                     if (!emit_char(ctx, ',', out, ix)) return;
                  }
               }
               cbor_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
            };

            if (!emit_char(ctx, '[', out, ix)) return;

            if (additional_info == info::indefinite) {
               // Indefinite-length array
               bool first = true;
               while (true) {
                  if (it >= end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  uint8_t peek;
                  std::memcpy(&peek, it, 1);

                  if (peek == initial_byte(major::simple, simple::break_code)) {
                     ++it;
                     break;
                  }

                  convert_element(!first);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
                  first = false;
               }
            }
            else {
               const uint64_t count = cbor_to_json_decode_arg(ctx, it, end, additional_info);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               for (uint64_t i = 0; i < count; ++i) {
                  convert_element(i > 0);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
            }

            if (!emit_char(ctx, ']', out, ix)) return;
            break;
         }

         case major::map: {
            if (!emit_char(ctx, '{', out, ix)) return;

            // A prettified map only breaks the line before its '}' when it holds at least one
            // pair, so an empty map stays "{}" whether its length was definite or indefinite.
            bool wrote_pair = false;

            // One pair: the ',' separating it from the pair before, the line a prettified map opens
            // for it, then the key, the colon, and the value.
            const auto convert_pair = [&](const bool needs_comma) {
               if (needs_comma) {
                  if (!emit_char(ctx, ',', out, ix)) return;
               }
               if constexpr (Opts.prettify) {
                  if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
               }

               // Key (must be a string for JSON compatibility)
               cbor_to_json_key<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if constexpr (Opts.prettify) {
                  if (!emit_literal<": ">(ctx, out, ix)) return;
               }
               else {
                  if (!emit_char(ctx, ':', out, ix)) return;
               }

               cbor_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
            };

            {
               // The indentation this map adds belongs to this map, so it is raised for the pairs
               // and no longer: leaving it up on an error path would indent whatever the context
               // is used for next.
               const indent_guard indent{ctx, Opts.prettify ? check_indentation_width(Opts) : 0};

               if (additional_info == info::indefinite) {
                  // Indefinite-length map
                  while (true) {
                     if (it >= end) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                     uint8_t peek;
                     std::memcpy(&peek, it, 1);

                     if (peek == initial_byte(major::simple, simple::break_code)) {
                        ++it;
                        break;
                     }

                     convert_pair(wrote_pair);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                     wrote_pair = true;
                  }
               }
               else {
                  const uint64_t count = cbor_to_json_decode_arg(ctx, it, end, additional_info);
                  if (bool(ctx.error)) [[unlikely]]
                     return;

                  wrote_pair = count > 0;

                  for (uint64_t i = 0; i < count; ++i) {
                     convert_pair(i > 0);
                     if (bool(ctx.error)) [[unlikely]]
                        return;
                  }
               }
            }

            if constexpr (Opts.prettify) {
               if (wrote_pair) {
                  if (!emit_newline_indent<Opts>(ctx, out, ix)) return;
               }
            }
            if (!emit_char(ctx, '}', out, ix)) return;
            break;
         }

         case major::tag: {
            // Semantic tag - for JSON output, we skip the tag and output the content
            // Some tags could have special handling (e.g., datetime)
            const uint64_t tag_num = cbor_to_json_decode_arg(ctx, it, end, additional_info);
            if (bool(ctx.error)) [[unlikely]]
               return;

            // Check for typed arrays (RFC 8746)
            const auto ta_info = typed_array::get_info(tag_num);
            if (ta_info.valid) {
               // It's a typed array - read the byte string and convert elements
               if (it >= end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               uint8_t bstr_initial;
               std::memcpy(&bstr_initial, it, 1);
               ++it;

               if (get_major_type(bstr_initial) != major::bstr) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               const uint64_t byte_len = cbor_to_json_decode_arg(ctx, it, end, get_additional_info(bstr_initial));
               if (bool(ctx.error)) [[unlikely]]
                  return;

               if (static_cast<uint64_t>(end - it) < byte_len) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               // Tags 83 and 87 hold IEEE binary128 elements, which no C++ floating point type this
               // library writes or reads represents. Rejecting the width here, before the array is
               // opened, keeps the element loop over widths it can decode and leaves no half
               // written array behind.
               if (ta_info.element_size > 8) [[unlikely]] {
                  ctx.error = error_code::feature_not_supported;
                  return;
               }

               if (byte_len % ta_info.element_size != 0) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               const size_t count = byte_len / ta_info.element_size;
               const bool need_swap = typed_array::needs_byteswap(tag_num);

               if (!emit_char(ctx, '[', out, ix)) return;

               for (size_t i = 0; i < count; ++i) {
                  if (i > 0) {
                     if (!emit_char(ctx, ',', out, ix)) return;
                  }

                  // Read and optionally byteswap the element
                  if (ta_info.is_float) {
                     if (ta_info.element_size == 2) {
                        // Half precision (tags 80 and 84) has no C++ type; it widens to double,
                        // the same conversion decode_half performs for a bare float16 value.
                        uint16_t half;
                        std::memcpy(&half, it, 2);
                        if (need_swap) {
                           half = std::byteswap(half);
                        }
                        to<JSON, double>::template op<Opts>(decode_half(half), ctx, out, ix);
                     }
                     else if (ta_info.element_size == 4) {
                        float val;
                        std::memcpy(&val, it, 4);
                        if (need_swap) {
                           uint32_t bits;
                           std::memcpy(&bits, &val, 4);
                           bits = std::byteswap(bits);
                           std::memcpy(&val, &bits, 4);
                        }
                        to<JSON, float>::template op<Opts>(val, ctx, out, ix);
                     }
                     else if (ta_info.element_size == 8) {
                        double val;
                        std::memcpy(&val, it, 8);
                        if (need_swap) {
                           uint64_t bits;
                           std::memcpy(&bits, &val, 8);
                           bits = std::byteswap(bits);
                           std::memcpy(&val, &bits, 8);
                        }
                        to<JSON, double>::template op<Opts>(val, ctx, out, ix);
                     }
                     else {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }
                  else if (ta_info.is_signed) {
                     if (ta_info.element_size == 1) {
                        int8_t val;
                        std::memcpy(&val, it, 1);
                        to<JSON, int8_t>::template op<Opts>(val, ctx, out, ix);
                     }
                     else if (ta_info.element_size == 2) {
                        int16_t val;
                        std::memcpy(&val, it, 2);
                        if (need_swap) {
                           uint16_t bits;
                           std::memcpy(&bits, &val, 2);
                           bits = std::byteswap(bits);
                           std::memcpy(&val, &bits, 2);
                        }
                        to<JSON, int16_t>::template op<Opts>(val, ctx, out, ix);
                     }
                     else if (ta_info.element_size == 4) {
                        int32_t val;
                        std::memcpy(&val, it, 4);
                        if (need_swap) {
                           uint32_t bits;
                           std::memcpy(&bits, &val, 4);
                           bits = std::byteswap(bits);
                           std::memcpy(&val, &bits, 4);
                        }
                        to<JSON, int32_t>::template op<Opts>(val, ctx, out, ix);
                     }
                     else if (ta_info.element_size == 8) {
                        int64_t val;
                        std::memcpy(&val, it, 8);
                        if (need_swap) {
                           uint64_t bits;
                           std::memcpy(&bits, &val, 8);
                           bits = std::byteswap(bits);
                           std::memcpy(&val, &bits, 8);
                        }
                        to<JSON, int64_t>::template op<Opts>(val, ctx, out, ix);
                     }
                  }
                  else {
                     // Unsigned
                     if (ta_info.element_size == 1) {
                        uint8_t val;
                        std::memcpy(&val, it, 1);
                        to<JSON, uint8_t>::template op<Opts>(val, ctx, out, ix);
                     }
                     else if (ta_info.element_size == 2) {
                        uint16_t val;
                        std::memcpy(&val, it, 2);
                        if (need_swap) {
                           val = std::byteswap(val);
                        }
                        to<JSON, uint16_t>::template op<Opts>(val, ctx, out, ix);
                     }
                     else if (ta_info.element_size == 4) {
                        uint32_t val;
                        std::memcpy(&val, it, 4);
                        if (need_swap) {
                           val = std::byteswap(val);
                        }
                        to<JSON, uint32_t>::template op<Opts>(val, ctx, out, ix);
                     }
                     else if (ta_info.element_size == 8) {
                        uint64_t val;
                        std::memcpy(&val, it, 8);
                        if (need_swap) {
                           val = std::byteswap(val);
                        }
                        to<JSON, uint64_t>::template op<Opts>(val, ctx, out, ix);
                     }
                  }

                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  it += ta_info.element_size;
               }

               if (!emit_char(ctx, ']', out, ix)) return;
            }
            else {
               // Other tags - just output the tagged content
               cbor_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
            }
            break;
         }

         case major::simple: {
            switch (additional_info) {
            case simple::false_value:
               if (!emit_literal<"false">(ctx, out, ix)) return;
               break;
            case simple::true_value:
               if (!emit_literal<"true">(ctx, out, ix)) return;
               break;
            case simple::null_value:
            // JSON has one empty value and CBOR has two. `undefined` becomes `null` because that is
            // the only thing JSON can say, so the distinction between "no value" and "absent value"
            // does not survive the conversion.
            case simple::undefined:
               if (!emit_literal<"null">(ctx, out, ix)) return;
               break;
            case simple::float16: {
               if ((end - it) < 2) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               uint16_t half;
               std::memcpy(&half, it, 2);
               if constexpr (std::endian::native == std::endian::little) {
                  half = std::byteswap(half);
               }
               it += 2;
               const double value = decode_half(half);
               to<JSON, double>::template op<Opts>(value, ctx, out, ix);
               break;
            }
            case simple::float32: {
               if ((end - it) < 4) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               uint32_t bits;
               std::memcpy(&bits, it, 4);
               if constexpr (std::endian::native == std::endian::little) {
                  bits = std::byteswap(bits);
               }
               float value;
               std::memcpy(&value, &bits, 4);
               it += 4;
               to<JSON, float>::template op<Opts>(value, ctx, out, ix);
               break;
            }
            case simple::float64: {
               if ((end - it) < 8) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               uint64_t bits;
               std::memcpy(&bits, it, 8);
               if constexpr (std::endian::native == std::endian::little) {
                  bits = std::byteswap(bits);
               }
               double value;
               std::memcpy(&value, &bits, 8);
               it += 8;
               to<JSON, double>::template op<Opts>(value, ctx, out, ix);
               break;
            }
            case simple::break_code:
               ctx.error = error_code::syntax_error; // Unexpected break
               break;
            default:
               if (additional_info < 24) {
                  // Simple value 0-23 - output as number
                  to<JSON, uint8_t>::template op<Opts>(additional_info, ctx, out, ix);
               }
               else if (additional_info == 24) {
                  // Simple value in next byte
                  if (it >= end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  uint8_t val;
                  std::memcpy(&val, it, 1);
                  ++it;
                  to<JSON, uint8_t>::template op<Opts>(val, ctx, out, ix);
               }
               else {
                  ctx.error = error_code::syntax_error;
               }
               break;
            }
            break;
         }

         default:
            ctx.error = error_code::syntax_error;
            break;
         }
      }

      // A JSON object key must be a string. Integer keys (pervasive in
      // COSE/CWT/WebAuthn) become quoted decimal strings, one of the string
      // forms RFC 8949 section 6.1 allows a converter to choose. Emitting them
      // bare produced structurally invalid JSON while reporting success. Text
      // and byte-string keys already emit as JSON strings via the value path,
      // a tag on a key is unwrapped and the tagged content re-checked, and key
      // types with no JSON string form (array, map, float, simple) are rejected.
      // Mirrors the string/integer key handling in beve_to_json_value's object case.
      template <auto Opts, class Buffer>
      inline void cbor_to_json_key(auto&& ctx, auto&& it, auto&& end, Buffer& out, auto&& ix, uint32_t recursive_depth)
      {
         using namespace cbor;

         if (recursive_depth >= max_recursive_depth_limit) [[unlikely]] {
            ctx.error = error_code::exceeded_max_recursive_depth;
            return;
         }

         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         uint8_t initial;
         std::memcpy(&initial, it, 1);
         const uint8_t major_type = get_major_type(initial);
         const uint8_t additional_info = get_additional_info(initial);

         switch (major_type) {
         case major::tstr:
         case major::bstr:
            // Both already emit as a JSON string (tstr escaped, bstr hex-quoted).
            cbor_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth);
            return;
         case major::tag: {
            // Unwrap the tag and key-check the tagged content, so e.g. a tag-0
            // datetime text key still emits as a string. Typed-array tags
            // (RFC 8746) decode to a JSON array and are rejected below.
            ++it;
            const uint64_t tag_num = cbor_to_json_decode_arg(ctx, it, end, additional_info);
            if (bool(ctx.error)) [[unlikely]]
               return;
            if (typed_array::get_info(tag_num).valid) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            cbor_to_json_key<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
            return;
         }
         case major::uint:
         case major::nint: {
            ++it;
            const uint64_t arg = cbor_to_json_decode_arg(ctx, it, end, additional_info);
            if (bool(ctx.error)) [[unlikely]]
               return;

            const auto emit_quoted = [&](const auto value) {
               if (!emit_char(ctx, '"', out, ix)) return;
               to<JSON, decltype(value)>::template op<Opts>(value, ctx, out, ix);
               if (bool(ctx.error)) [[unlikely]]
                  return;
               if (!emit_char(ctx, '"', out, ix)) return;
            };

            if (major_type == major::uint) {
               emit_quoted(arg);
            }
            else {
               emit_quoted(static_cast<int64_t>(~arg));
            }
            return;
         }
         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   }

   // Convert CBOR buffer directly to JSON without intermediate C++ types
   //
   // The buffer holds exactly one CBOR data item, which is what a CBOR-encoded document is. An
   // empty buffer is not one, and a buffer holding several is a CBOR sequence (RFC 8742), whose
   // items would run together into text no JSON parser accepts. Both are reported rather than
   // written, because a converter that answers a malformed document with a malformed one just
   // moves the problem downstream.
   export template <auto Opts = glz::opts{}, class CBORBuffer, class JSONBuffer>
   [[nodiscard]] inline error_ctx cbor_to_json(const CBORBuffer& cbor, JSONBuffer& out)
   {
      size_t ix{}; // write index

      auto* it = cbor.data();
      auto* end = it + cbor.size();

      context ctx{};

      if (it >= end) {
         return {0, error_code::unexpected_end};
      }

      detail::cbor_to_json_value<Opts>(ctx, it, end, out, ix, 0);
      if (bool(ctx.error)) {
         return {ix, ctx.error};
      }

      if (it < end) {
         return {ix, error_code::syntax_error};
      }

      if constexpr (resizable<JSONBuffer>) {
         out.resize(ix);
      }

      // count is the number of bytes written. A resizable buffer carries its own size, but a
      // fixed-size one has no other way to learn how much of it now holds JSON.
      return {ix};
   }

   // Convenience function returning string
   export template <auto Opts = glz::opts{}, class CBORBuffer>
   [[nodiscard]] inline expected<std::string, error_ctx> cbor_to_json(const CBORBuffer& cbor)
   {
      std::string out;
      auto ec = cbor_to_json<Opts>(cbor, out);
      if (ec) {
         return unexpected(ec);
      }
      return out;
   }
}
