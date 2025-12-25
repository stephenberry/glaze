#pragma once

#include <ei.h>

#include <glaze/concepts/container_concepts.hpp>
#include <glaze/core/common.hpp>
#include <glaze/core/context.hpp>
#include <ranges>

#include "defs.hpp"
#include "types.hpp"

namespace glz
{

#define CHECK_OFFSET(off)                     \
   if ((it + (off)) > end) [[unlikely]] {     \
      ctx.error = error_code::unexpected_end; \
      return;                                 \
   }

   using header_pair = std::pair<std::size_t, std::size_t>;

   namespace detail
   {
      struct BigEndian
      {};
      struct LittleEndian
      {};

      template <class V, is_context Ctx, typename Endian = BigEndian>
         requires std::is_integral_v<V>
      V read(Ctx&& ctx, auto&& it, auto&& end)
      {
         if (it + sizeof(V) > end) {
            ctx.error = error_code::seek_failure;
            return {};
         }

         V v{};
         if constexpr (std::is_same_v<Endian, LittleEndian>) {
            std::memcpy(&v, it, sizeof(V));
         } else {
            const auto view = std::views::counted(reinterpret_cast<const uint8_t*>(it), sizeof(V));
            std::copy(std::ranges::rbegin(view), std::ranges::rend(view), &v);
         }

         return v;
      }

      template <class V, is_context Ctx, typename Endian = BigEndian>
         requires std::is_integral_v<V>
      V reada(Ctx&& ctx, auto&& it, auto&& end)
      {
         auto v = read<V, Ctx, Endian>(ctx, it, end);
         std::advance(it, sizeof(V));
         return v;
      }

      template <class F, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE void decode_impl(F&& func, Ctx&& ctx, It0&& it, It1&& end)
      {
         int index{};
         if (func(it, &index) < 0) [[unlikely]] {
            ctx.error = error_code::parse_number_failure;
            return;
         }

         CHECK_OFFSET(index);
         std::advance(it, index);
      }

      GLZ_ALWAYS_INLINE void resize_buffer(size_t extent, output_buffer auto&& b, auto&& ix)
      {
         if (const auto k = ix + extent; k > b.size()) [[unlikely]] {
            b.resize(2 * k);
         }
      }

      template <typename Endian = LittleEndian>
      GLZ_ALWAYS_INLINE void write_type(auto&& value, auto&& /* ctx */, auto&& b, auto&& ix)
      {
         using V = std::decay_t<decltype(value)>;
         constexpr auto n = sizeof(V);
         resize_buffer(n, b, ix);

         constexpr auto is_volatile = std::is_volatile_v<std::remove_reference_t<decltype(value)>>;
         if constexpr (std::is_same_v<Endian, LittleEndian>) {
            if constexpr (is_volatile) {
               const V temp = value;
               std::memcpy(&b[ix], &temp, n);
            }
            else {
               std::memcpy(&b[ix], &value, n);
            }
         }
         else {
            if constexpr (is_volatile) {
               const V temp = value;
               const auto view = std::views::counted(reinterpret_cast<const uint8_t*>(&temp), n);
               std::copy(std::ranges::rbegin(view), std::ranges::rend(view), &b[ix]);
            }
            else {
               const auto view = std::views::counted(reinterpret_cast<const uint8_t*>(&value), n);
               std::copy(std::ranges::rbegin(view), std::ranges::rend(view), &b[ix]);
            }
         }
         ix += n;
      }

      GLZ_ALWAYS_INLINE void write_buffer(contiguous auto&& buffer, auto&& /* ctx */, auto&& b, auto&& ix)
      {
         const size_t n = buffer.size();
         resize_buffer(n, b, ix);

         std::memcpy(&b[ix], buffer.data(), n);
         ix += n;
      }

      template <typename V>
      GLZ_ALWAYS_INLINE void write_integer(V&& value, auto&& ctx, auto&& b, auto&& ix)
      {
         const auto check_min_value = [value]() -> bool {
            if constexpr (std::is_signed_v<V>) {
               return value >= std::numeric_limits<int32_t>::min();
            }
            else {
               return true;
            }
         };

         if (value < 256 && value >= 0) {
            write_type(eetf_tag::SMALL_INTEGER, ctx, b, ix);
            write_type(static_cast<uint8_t>(value), ctx, b, ix);
         }
         else if (value <= std::numeric_limits<int32_t>::max() && check_min_value()) {
            // TODO tests
            write_type(eetf_tag::INTEGER, ctx, b, ix);
            write_type(static_cast<int32_t>(value), ctx, b, ix);
         }
         else {
            // TODO tests
            uint8_t arity = 0;

            using UV = std::make_unsigned_t<std::remove_cvref_t<V>>;
            UV uv = [value]() -> UV {
               if constexpr (std::is_signed_v<V>) {
                  return static_cast<UV>(std::abs(value));
               }
               else {
                  return value;
               }
            }();

            write_type(eetf_tag::SMALL_BIG, ctx, b, ix);
            auto arity_ix = ix; // store index to write later
            write_type(arity, ctx, b, ix);
            write_type(uint8_t(value < 0), ctx, b, ix);
            while (uv) {
               write_type(static_cast<uint8_t>(uv & 0xFF), ctx, b, ix);
               uv >>= 8;
               arity++;
            }
            write_type(arity, ctx, b, arity_ix);
         }
      }

   } // namespace detail

   template <class It0, class It1>
   GLZ_ALWAYS_INLINE eetf_tag get_type(size_t& s, is_context auto&& ctx, It0&& it, It1&& end)
   {
      eetf_tag type = static_cast<eetf_tag>(detail::read<uint8_t>(ctx, it, end));
      if (bool(ctx.error)) {
         return {};
      }

      auto next = it + 1;
      switch (type) {
      case SMALL_ATOM:
      case SMALL_ATOM_UTF8:
         type = ATOM_UTF8;
         [[fallthrough]];
      case SMALL_TUPLE:
         s = detail::read<uint8_t>(ctx, next, end);
         break;

      case ATOM_UTF8:
         type = ATOM;
         [[fallthrough]];
      case ATOM:
      case STRING:
         s = detail::read<uint16_t>(ctx, next, end);
         break;

      case FLOAT:
      case FLOAT_NEW:
         type = FLOAT;
         break;

      case LARGE_TUPLE:
      case LIST:
      case MAP:
      case BINARY:
      case BIT_BINARY:
         s = detail::read<uint32_t>(ctx, next, end);
         break;

      case SMALL_BIG:
         s = detail::read<uint8_t>(ctx, next, end);
         break;

      case LARGE_BIG:
         s = detail::read<uint32_t>(ctx, next, end);
         break;

      case NEW_PID:
         type = PID;
         break;
      case V4_PORT:
      case NEW_PORT:
         type = PORT;
         break;
      case NEWER_REFERENCE:
         type = NEW_REFERENCE;
         break;

      case INTEGER:
      case SMALL_INTEGER:
      case NEW_REFERENCE:
      case PORT:
      case PID:
      case NIL:
      case EXPORT:
      case REFERENCE:
      case NEW_FUN:
      case FUN:
         break;
      }

      if (bool(ctx.error)) {
         return {};
      }

      return type;
   }

   template <class It0, class It1>
   [[nodiscard]] GLZ_ALWAYS_INLINE bool decode_version(is_context auto&& ctx, It0&& it, It1&& end)
   {
      const uint8_t v = detail::reada<uint8_t>(ctx, it, end);
      if (bool(ctx.error)) [[unlikely]] {
         return false;
      }

      if (v != version_magic) [[unlikely]] {
         ctx.error = error_code::version_mismatch;
         return false;
      }
      return true;
   }

   template <class It0, class It1>
   auto skip_term(is_context auto&& ctx, It0&& it, It1&& end)
   {
      int index{};
      if (ei_skip_term(it, &index) < 0) {
         ctx.error = error_code::syntax_error;
         index = 0;
      }

      CHECK_OFFSET(index);
      std::advance(it, index);
   }

   template <num_t T, class... Args>
   GLZ_ALWAYS_INLINE void decode_number(T&& value, Args&&... args)
   {
      using namespace std::placeholders;
      using V = std::remove_cvref_t<T>;
      if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
         double v;
         detail::decode_impl(std::bind(ei_decode_double, _1, _2, &v), std::forward<Args>(args)...);
         value = static_cast<std::remove_cvref_t<T>>(v);
      }
      else {
         if constexpr (sizeof(V) > sizeof(long)) {
            if constexpr (std::is_signed_v<V>) {
               long long v;
               detail::decode_impl(std::bind(ei_decode_longlong, _1, _2, &v), std::forward<Args>(args)...);
               value = static_cast<T>(v);
            }
            else {
               unsigned long long v;
               detail::decode_impl(std::bind(ei_decode_ulonglong, _1, _2, &v), std::forward<Args>(args)...);
               value = static_cast<T>(v);
            }
         }
         else {
            if constexpr (std::is_signed_v<V>) {
               long v;
               detail::decode_impl(std::bind(ei_decode_long, _1, _2, &v), std::forward<Args>(args)...);
               value = static_cast<T>(v);
            }
            else {
               unsigned long v;
               detail::decode_impl(std::bind(ei_decode_ulong, _1, _2, &v), std::forward<Args>(args)...);
               value = static_cast<T>(v);
            }
         }
      }
   }

   template <class It0, class It1>
   GLZ_ALWAYS_INLINE void decode_token(auto&& value, is_context auto&& ctx, It0&& it, It1&& end)
   {
      using namespace std::placeholders;

      size_t sz;
      const auto type = get_type(sz, ctx, it, end);
      if (bool(ctx.error)) {
         return;
      }

      CHECK_OFFSET(sz);

      value.resize(sz);
      if (eetf::is_atom(type)) {
         detail::decode_impl(std::bind(ei_decode_atom, _1, _2, value.data()), ctx, it, end);
      }
      else {
         detail::decode_impl(std::bind(ei_decode_string, _1, _2, value.data()), ctx, it, end);
      }
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void decode_boolean(auto&& value, Args&&... args)
   {
      using namespace std::placeholders;

      int v{};
      detail::decode_impl(std::bind(ei_decode_boolean, _1, _2, &v), std::forward<Args>(args)...);
      value = v != 0;
   }

   template <is_context Ctx, class It>
   GLZ_ALWAYS_INLINE auto decode_list_header(Ctx&& ctx, It&& it)
   {
      int arity{};
      int index{};
      if (ei_decode_list_header(it, &index, &arity) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return header_pair(-1ull, -1ull);
      }

      return header_pair(static_cast<std::size_t>(arity), static_cast<std::size_t>(index));
   }

   template <auto Opts, class T>
   GLZ_ALWAYS_INLINE void decode_list(T&& value, is_context auto&& ctx, auto&& it, auto&& end)
   {
      using V = range_value_t<std::decay_t<T>>;

      auto [arity, index] = decode_list_header(ctx, it);
      if (bool(ctx.error)) {
         return;
      }

      if constexpr (resizable<T>) {
         value.resize(arity);
         if constexpr (check_shrink_to_fit(Opts)) {
            value.shrink_to_fit();
         }
      }
      else {
         if (static_cast<std::size_t>(arity) > value.size()) {
            ctx.error = error_code::syntax_error;
            return;
         }
      }

      CHECK_OFFSET(index);
      std::advance(it, index);

      for (std::size_t idx = 0; idx < arity; idx++) {
         V v;
         from<EETF, V>::template op<Opts>(v, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         value[idx] = std::move(v);
      }

      // TODO handle elang list endings
   }

   template <auto Opts, class T, is_context Ctx, class It0, class It1>
   GLZ_ALWAYS_INLINE void decode_sequence(T&& value, Ctx&& ctx, It0&& it, It1&& end)
   {
      int index{};
      int type{};
      int sz{};
      if (ei_get_type(it, &index, &type, &sz) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return;
      }

      if (eetf::is_list(type)) {
         if (eetf::is_string(type)) {
            std::string buff;
            decode_token(buff, std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
            if constexpr (resizable<T>) {
               value.resize(sz);
               if constexpr (check_shrink_to_fit(Opts)) {
                  value.shrink_to_fit();
               }
            }
            else {
               if (static_cast<std::size_t>(sz) > value.size()) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            std::copy(buff.begin(), buff.end(), value.begin());
         }
         else {
            decode_list<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                              std::forward<It1>(end));
         }
      }
      // else if tuple?
      else {
         ctx.error = error_code::elements_not_convertible_to_design;
      }
   }

   template <is_context Ctx, class It>
   GLZ_ALWAYS_INLINE auto decode_map_header(Ctx&& ctx, It&& it)
   {
      int arity{};
      int index{};
      if (ei_decode_map_header(it, &index, &arity) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return header_pair(-1ull, -1ull);
      }

      return header_pair(static_cast<std::size_t>(arity), static_cast<std::size_t>(index));
   }

   template <is_context Ctx, class It>
   GLZ_ALWAYS_INLINE auto decode_tuple_header(Ctx&& ctx, It&& it)
   {
      int arity{};
      int index{};
      if (ei_decode_tuple_header(it, &index, &arity) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return header_pair(-1ull, -1ull);
      }

      return header_pair(static_cast<std::size_t>(arity), static_cast<std::size_t>(index));
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_version(Args&&... args)
   {
      detail::write_type(version_magic, std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_boolean(const bool value, Args&&... args)
   {
      detail::write_type(eetf_tag::SMALL_ATOM_UTF8, std::forward<Args>(args)...);

      using namespace std::string_view_literals;
      const std::string_view v{value ? "true"sv : "false"sv};
      detail::write_type(static_cast<uint8_t>(v.size()), std::forward<Args>(args)...);
      detail::write_buffer(v, std::forward<Args>(args)...);
   }

   template <class T, class... Args>
   GLZ_ALWAYS_INLINE void encode_number(T&& value, Args&&... args)
   {
      using namespace std::placeholders;

      using V = std::remove_cvref_t<T>;
      if constexpr (std::floating_point<V>) {
         detail::write_type(eetf_tag::FLOAT_NEW, std::forward<Args>(args)...);
         detail::write_type<detail::BigEndian>(value, std::forward<Args>(args)...);
      }
      else {
         detail::write_integer(value, std::forward<Args>(args)...);
      }
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_atom_len(auto&& value, std::size_t sz, is_context auto&& ctx, auto&& b, auto&& ix)
   {
      if (sz > max_atom_len) {
         ctx.error = error_code::seek_failure;
         return;
      }

      detail::write_type(eetf_tag::ATOM_UTF8, ctx, b, ix);
      detail::write_type<detail::BigEndian>(static_cast<uint16_t>(sz), ctx, b, ix);
      detail::write_buffer(value, ctx, b, ix);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_atom(auto&& value, Args&&... args)
   {
      encode_atom_len(value, value.size(), std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_string(auto&& value, Args&&... args)
   {
      const auto len = value.size();
      if (len == 0) {
         detail::write_type(eetf_tag::NIL, std::forward<Args>(args)...);
         return;
      }

      // TODO tests
      if (len <= 0xFFFF) {
         detail::write_type(eetf_tag::STRING, std::forward<Args>(args)...);
         detail::write_type<detail::BigEndian>(static_cast<uint16_t>(len), std::forward<Args>(args)...);
         detail::write_buffer(value, std::forward<Args>(args)...);
         return;
      }

      // TODO tests
      detail::write_type(eetf_tag::LIST, std::forward<Args>(args)...);
      detail::write_type<detail::BigEndian>(len, std::forward<Args>(args)...);

      for (const auto& c : value) {
         detail::write_type(eetf_tag::SMALL_INTEGER, std::forward<Args>(args)...);
         detail::write_type(c, std::forward<Args>(args)...);
      }

      detail::write_type(eetf_tag::NIL, std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_tuple_header(std::uint32_t arity, Args&&... args)
   {
      if (arity <= 0xFF) {
         detail::write_type(eetf_tag::SMALL_TUPLE, std::forward<Args>(args)...);
         detail::write_type(static_cast<uint8_t>(arity), std::forward<Args>(args)...);
      }
      else {
         // TODO tests
         detail::write_type(eetf_tag::LARGE_TUPLE, std::forward<Args>(args)...);
         detail::write_type<detail::BigEndian>(arity, std::forward<Args>(args)...);
      }
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_list_header(std::uint32_t arity, Args&&... args)
   {
      if (arity == 0) {
         detail::write_type(eetf_tag::NIL, std::forward<Args>(args)...);
         return;
      }

      detail::write_type(eetf_tag::LIST, std::forward<Args>(args)...);
      detail::write_type<detail::BigEndian>(arity, std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_list_tail(Args&&... args)
   {
      encode_list_header(0u, std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_map_header(std::uint32_t arity, Args&&... args)
   {
      detail::write_type(eetf_tag::MAP, std::forward<Args>(args)...);
      detail::write_type<detail::BigEndian>(arity, std::forward<Args>(args)...);
   }

} // namespace glz
