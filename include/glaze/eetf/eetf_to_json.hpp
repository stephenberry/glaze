// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>
#include <cstring>

#include "glaze/eetf/ei.hpp"
#include "glaze/eetf/opts.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail
   {
      template <class It0, class It1>
      GLZ_ALWAYS_INLINE size_t get8s(auto&& ctx, It0&& it, It1&& end)
      {
         CHECK_OFFSET_RET(1, 0);
         return std::size_t(*it++);
      }

      template <class It0, class It1>
      GLZ_ALWAYS_INLINE size_t get16be(auto&& ctx, It0&& it, It1&& end)
      {
         CHECK_OFFSET_RET(2, 0);
         const std::size_t b1 = (*it++) << 8;
         return b1 | *it++;
      }

      template <auto Opts, typename I>
      GLZ_ALWAYS_INLINE void term_to_json_number(I&& val, auto&& ctx, auto&& it, auto&& end, auto& out,
                                                 auto&& ix) noexcept
      {
         decode_number(val, ctx, it, end);
         if (bool(ctx.error)) {
            return;
         }
         to<JSON, I>::template op<Opts>(val, ctx, out, ix);
      }

      template <auto Opts>
      GLZ_ALWAYS_INLINE void term_to_json_big_integer(auto&& ctx, auto&& it, auto&& end, auto& out, auto&& ix) noexcept
      {
         auto tit = it;
         ++tit; // skip type
         const auto size = get8s(ctx, tit, end);
         if (size > 8u) {
            ctx.error = error_code::no_matching_variant_type;
            return;
         }

         const int sign = *tit++;
         if (sign) {
            term_to_json_number<Opts>(std::int64_t{}, ctx, it, end, out, ix);
         }
         else {
            term_to_json_number<Opts>(std::uint64_t{}, ctx, it, end, out, ix);
         }
      }

      template <auto Opts, class Buffer>
      void term_to_json_value(auto&& ctx, auto&& it, auto&& end, Buffer& out, auto&& ix, uint32_t recursive_depth)
      {
         // Check recursion depth limit
         if (recursive_depth >= max_recursive_depth_limit) [[unlikely]] {
            ctx.error = error_code::exceeded_max_recursive_depth;
            return;
         }

         if (invalid_end(ctx, it, end)) [[unlikely]] {
            return;
         }

         auto write_sequence = [](size_t arity, size_t index, auto&& ctx, auto&& it, auto&& end, Buffer& out,
                                   auto&& ix, auto recursive_depth) {
            std::advance(it, index);
            if (invalid_end(ctx, it, end)) [[unlikely]] {
               return;
            }

            dump('[', out, ix);
            while (arity--) {
               term_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth);
               if (arity) {
                  dump(',', out, ix);
                  if constexpr (Opts.prettify) {
                     dump(' ', out, ix);
                  }
               }
            }
            dump(']', out, ix);
         };

         const auto type = uint8_t(*it);
         switch (type) {
         case ERL_SMALL_INTEGER_EXT:
         case ERL_INTEGER_EXT: {
            term_to_json_number<Opts>(std::int64_t{}, ctx, it, end, out, ix);
            if (bool(ctx.error)) return;
            break;
         }

         case ERL_SMALL_BIG_EXT: {
            term_to_json_big_integer<Opts>(ctx, it, end, out, ix);
            if (bool(ctx.error)) return;
            break;
         }

         case ERL_FLOAT_EXT:
         case NEW_FLOAT_EXT: {
            term_to_json_number<Opts>(double{}, ctx, it, end, out, ix);
            if (bool(ctx.error)) return;
            break;
         }

         case ERL_STRING_EXT:
         case ERL_ATOM_EXT:
         case ERL_ATOM_UTF8_EXT: {
            ++it; // skip type
            const size_t len = get16be(ctx, it, end);
            CHECK_OFFSET(len);
            const sv value{reinterpret_cast<const char*>(it), len};
            to<JSON, sv>::template op<Opts>(value, ctx, out, ix);
            std::advance(it, len);
            break;
         }

         case ERL_SMALL_ATOM_EXT:
         case ERL_SMALL_ATOM_UTF8_EXT: {
            ++it; // skip type
            const size_t len = get8s(ctx, it, end);
            CHECK_OFFSET(len);
            const sv value{reinterpret_cast<const char*>(it), len};
            if (len == 4 && std::memcmp(it, "true", 4) == 0) {
               dump("true", out, ix);
            }
            else if (len == 5 && std::memcmp(it, "false", 5) == 0) {
               dump("false", out, ix);
            }
            else {
               to<JSON, sv>::template op<Opts>(value, ctx, out, ix);
            }
            std::advance(it, len);
            break;
         }

         case ERL_LIST_EXT: {
            [[maybe_unused]] auto [arity, idx] = decode_list_header(ctx, it);
            if (bool(ctx.error)) {
               return;
            }
            write_sequence(arity, idx, ctx, it, end, out, ix, recursive_depth + 1);

            // handle tail
            const auto tag = get_type(ctx, it);
            if (tag != ERL_NIL_EXT) {
               ctx.error = error_code::array_element_not_found;
               return;
            }
            ++it;
            break;
         }

         case ERL_NIL_EXT: {
            dump("[]", out, ix);
            ++it;
            break;
         }

         case ERL_SMALL_TUPLE_EXT:
         case ERL_LARGE_TUPLE_EXT: {
            [[maybe_unused]] auto [arity, idx] = decode_tuple_header(ctx, it);
            if (bool(ctx.error)) {
               return;
            }
            write_sequence(arity, idx, ctx, it, end, out, ix, recursive_depth + 1);
            break;
         }

         case ERL_MAP_EXT: {
            [[maybe_unused]] auto [arity, idx] = decode_map_header(ctx, it);
            if (bool(ctx.error)) {
               return;
            }

            std::advance(it, idx);
            dump('{', out, ix);
            if constexpr (Opts.prettify) {
               ctx.depth += check_indentation_width(Opts);
               dump('\n', out, ix);
               dumpn(check_indentation_char(Opts), ctx.depth, out, ix);
            }
            else {
               ++ctx.depth;
            }

            while (arity--) {
               // write key
               const auto key_type = get_type(ctx, it);
               // support only string or atom keys in json
               if (!eetf::is_string(key_type) && !eetf::is_atom(key_type)) {
                  ctx.error = error_code::syntax_error;
                  ctx.custom_error_message = "unsupported key type";
                  return;
               }
               term_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
               if (bool(ctx.error)) return;
               if constexpr (Opts.prettify) {
                  dump(": ", out, ix);
               }
               else {
                  dump(':', out, ix);
               }
               // write value
               term_to_json_value<Opts>(ctx, it, end, out, ix, recursive_depth + 1);
               if (arity) {
                  dump(',', out, ix);
                  if constexpr (Opts.prettify) {
                     dump('\n', out, ix);
                     dumpn(check_indentation_char(Opts), ctx.depth, out, ix);
                  }
               }
            }
            if constexpr (Opts.prettify) {
               ctx.depth -= check_indentation_width(Opts);
               dump('\n', out, ix);
               dumpn(check_indentation_char(Opts), ctx.depth, out, ix);
            }
            else {
               --ctx.depth;
            }
            dump('}', out, ix);
            break;
         }

         default: {
            ctx.error = error_code::syntax_error;
            return;
         }
         }
      }
   } // namespace detail

   template <auto Opts = opts{}, class EETFBuffer, class JSONBuffer>
   [[nodiscard]] inline error_ctx eetf_to_json(const EETFBuffer& term, JSONBuffer& out)
   {
      size_t ix{}; // write index

      auto* it = term.data();
      auto* end = it + term.size();

      context ctx{};

      // skip magic version
      const int version = (unsigned char)*it++;
      if (eetf_magic_version != version) {
         return {1, error_code::version_mismatch};
      }

      while (it < end) {
         detail::term_to_json_value<Opts>(ctx, it, end, out, ix, 0);
         if (bool(ctx.error)) {
            return {ix, ctx.error};
         }
      }

      if constexpr (resizable<JSONBuffer>) {
         out.resize(ix);
      }

      return {};
   }
} // namespace glz
