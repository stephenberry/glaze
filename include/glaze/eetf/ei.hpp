#pragma once

#include <ei.h>

#include <glaze/concepts/container_concepts.hpp>
#include <glaze/core/common.hpp>
#include <glaze/core/context.hpp>

#include "defs.hpp"
#include "types.hpp"

namespace glz
{

   template <class Ctx, class It0, class It1>
   [[nodiscard]] GLZ_ALWAYS_INLINE bool check_invalid_offset(Ctx&& ctx, It0&& it, It1&& end, size_t off) noexcept
   {
      // Callers maintain it <= end (every advance is bounds-checked or followed by invalid_end),
      // so end - it is non-negative. Computing the remaining size as a subtraction avoids the
      // out-of-bounds pointer arithmetic that (it + off) > end would incur for a large off.
      if (static_cast<size_t>(end - it) < off) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return true;
      }
      return false;
   }

   using header_pair = std::pair<std::size_t, std::size_t>;

   namespace detail
   {
      template <class F, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE void decode_impl(F&& func, Ctx&& ctx, It0&& it, It1&& end) noexcept
      {
         int index{};
         if (func(reinterpret_cast<const char*>(it), &index) < 0) [[unlikely]] {
            ctx.error = error_code::parse_number_failure;
            return;
         }

         if (check_invalid_offset(ctx, it, end, index)) return;
         std::advance(it, index);
      }

      template <output_buffer B, class IX>
      [[nodiscard]] GLZ_ALWAYS_INLINE int resize_buffer(B&& b, IX&& ix, int index)
      {
         if (b.size() < static_cast<std::size_t>(index)) {
            b.resize((std::max)(b.size() * 2, static_cast<std::size_t>(index)));
         }

         return static_cast<int>(ix);
      }

      template <class F, is_context Ctx, class B, class IX>
      GLZ_ALWAYS_INLINE void encode_impl(F&& func, Ctx&& ctx, B&& b, IX&& ix)
      {
         int index{static_cast<int>(ix)};
         if (func(nullptr, &index) < 0) {
            ctx.error = error_code::seek_failure;
            return;
         }

         index = resize_buffer(b, ix, index);
         if (func(reinterpret_cast<char*>(b.data()), &index) < 0) {
            ctx.error = error_code::seek_failure;
            return;
         }

         ix = index;
      }

   } // namespace detail

   template <class It>
   [[nodiscard]] GLZ_ALWAYS_INLINE int decode_version(is_context auto&& ctx, It&& it) noexcept
   {
      int index{};
      int version{};
      if (ei_decode_version(reinterpret_cast<const char*>(it), &index, &version) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return -1;
      }

      std::advance(it, index);
      return version;
   }

   template <class It>
   GLZ_ALWAYS_INLINE int get_type(is_context auto&& ctx, It&& it) noexcept
   {
      int type{};
      int size{};
      int index{};
      if (ei_get_type(reinterpret_cast<const char*>(it), &index, &type, &size) < 0) {
         ctx.error = error_code::syntax_error;
         return -1;
      }

      return type;
   }

   template <class It0, class It1>
   auto skip_term(is_context auto&& ctx, It0&& it, It1&& end) noexcept
   {
      int index{};
      if (ei_skip_term(reinterpret_cast<const char*>(it), &index) < 0) {
         ctx.error = error_code::syntax_error;
         index = 0;
      }

      if (check_invalid_offset(ctx, it, end, index)) return;
      std::advance(it, index);
   }

   template <num_t T, class... Args>
   GLZ_ALWAYS_INLINE void decode_number(T&& value, Args&&... args) noexcept
   {
      using V = std::remove_cvref_t<T>;
      // Cast the scratch value through V (the decayed value type), never T. T is a forwarding
      // reference, so static_cast<T>(v) forms a reference cast that fails to compile when the
      // scratch type (long / long long) is a distinct type of the same width as V -- e.g. on
      // platforms where int64_t is long long while long is also 64-bit (macOS/LLP64-style).
      if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
         double v;
         detail::decode_impl([&](const char* buf, int* index) { return ei_decode_double(buf, index, &v); },
                             std::forward<Args>(args)...);
         value = static_cast<std::remove_cvref_t<T>>(v);
      }
      else {
         if constexpr (sizeof(V) > sizeof(long)) {
            if constexpr (std::is_signed_v<V>) {
               long long v;
               detail::decode_impl([&](const char* buf, int* index) { return ei_decode_longlong(buf, index, &v); },
                                   std::forward<Args>(args)...);
               value = static_cast<V>(v);
            }
            else {
               unsigned long long v;
               detail::decode_impl([&](const char* buf, int* index) { return ei_decode_ulonglong(buf, index, &v); },
                                   std::forward<Args>(args)...);
               value = static_cast<V>(v);
            }
         }
         else {
            if constexpr (std::is_signed_v<V>) {
               long v;
               detail::decode_impl([&](const char* buf, int* index) { return ei_decode_long(buf, index, &v); },
                                   std::forward<Args>(args)...);
               value = static_cast<V>(v);
            }
            else {
               unsigned long v;
               detail::decode_impl([&](const char* buf, int* index) { return ei_decode_ulong(buf, index, &v); },
                                   std::forward<Args>(args)...);
               value = static_cast<V>(v);
            }
         }
      }
   }

   template <class It0, class It1>
   GLZ_ALWAYS_INLINE void decode_token(auto&& value, is_context auto&& ctx, It0&& it, It1&& end)
   {
      int index{};
      int type{};
      int sz{};
      if (ei_get_type(reinterpret_cast<const char*>(it), &index, &type, &sz) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return;
      }

      if (check_invalid_offset(ctx, it, end, sz)) return;

      // ei_get_type reports sz as the on-wire token length without the terminating NUL, while
      // ei_decode_atom/ei_decode_string write the token plus a NUL. Sizing a buffer to exactly
      // sz would let that NUL be written one byte past the end (heap overflow on attacker-
      // controlled atoms/strings), so each branch must reserve room for the terminator.
      if (eetf::is_atom(type)) {
         // ei_decode_atom writes the Latin-1 name + a NUL; for a UTF-8 atom the decoded length is
         // <= sz, and the NUL is never counted in sz. Decode into a bounded scratch buffer (the
         // size ei itself uses for atom names; ei_decode_atom caps its own write at MAXATOMLEN and
         // errors rather than overrunning) and copy out the real length, so we neither overrun nor
         // leave trailing bytes.
         char buffer[MAXATOMLEN_UTF8];
         detail::decode_impl([&](const char* buf, int* index) { return ei_decode_atom(buf, index, buffer); }, ctx, it,
                             end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         const std::size_t len = std::char_traits<char>::length(buffer);
         value.resize(len);
         std::char_traits<char>::copy(value.data(), buffer, len);
      }
      else {
         // ei_decode_string writes sz bytes + a NUL; an Erlang string may contain embedded NULs,
         // so its length is sz, not strlen. Size to sz + 1 for the NUL, then trim it off.
         value.resize(static_cast<std::size_t>(sz) + 1);
         detail::decode_impl([&](const char* buf, int* index) { return ei_decode_string(buf, index, value.data()); },
                             ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         value.resize(static_cast<std::size_t>(sz));
      }
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void decode_boolean(auto&& value, Args&&... args) noexcept
   {
      int v{};
      detail::decode_impl([&](const char* buf, int* index) { return ei_decode_boolean(buf, index, &v); },
                          std::forward<Args>(args)...);
      value = v != 0;
   }

   template <auto Opts, class T, class It0>
   void decode_binary(T&& value, std::size_t sz, is_context auto&& ctx, It0&& it, auto&& end)
   {
      if (check_invalid_offset(ctx, it, end, sz * sizeof(std::uint8_t))) return;

      using V = range_value_t<std::decay_t<T>>;

      if constexpr (resizable<T>) {
         value.resize(sz);
         if constexpr (check_shrink_to_fit(Opts)) {
            value.shrink_to_fit();
         }
      }
      else {
         if (sz > value.size()) {
            ctx.error = error_code::syntax_error;
            return;
         }
      }

      [[maybe_unused]] long szl{};
      if constexpr (sizeof(V) == sizeof(std::uint8_t)) {
         detail::decode_impl(
            [&](const char* buf, int* index) { return ei_decode_binary(buf, index, value.data(), &szl); }, ctx, it,
            end);
      }
      else {
         std::vector<std::uint8_t> buff(sz);
         detail::decode_impl(
            [&](const char* buf, int* index) { return ei_decode_binary(buf, index, buff.data(), &szl); }, ctx, it, end);
         std::copy(buff.begin(), buff.end(), value.begin());
      }
   }

   template <is_context Ctx, class It>
   GLZ_ALWAYS_INLINE auto decode_list_header(Ctx&& ctx, It&& it) noexcept
   {
      int arity{};
      int index{};
      if (ei_decode_list_header(reinterpret_cast<const char*>(it), &index, &arity) < 0) [[unlikely]] {
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

      if (check_invalid_offset(ctx, it, end, index)) return;
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
      if (ei_get_type(reinterpret_cast<const char*>(it), &index, &type, &sz) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return;
      }

      if (eetf::is_binary(type)) {
         decode_binary<Opts>(std::forward<T>(value), static_cast<std::size_t>(sz), std::forward<Ctx>(ctx),
                             std::forward<It0>(it), end);
      }
      else if (eetf::is_list(type)) {
         if (eetf::is_string(type)) {
            std::string buff;
            decode_token(buff, std::forward<Ctx>(ctx), std::forward<It0>(it), end);
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
            decode_list<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), end);
         }
      }
      // else if tuple?
      else {
         ctx.error = error_code::elements_not_convertible_to_design;
      }
   }

   template <is_context Ctx, class It>
   GLZ_ALWAYS_INLINE auto decode_map_header(Ctx&& ctx, It&& it) noexcept
   {
      int arity{};
      int index{};
      if (ei_decode_map_header(reinterpret_cast<const char*>(it), &index, &arity) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return header_pair(-1ull, -1ull);
      }

      return header_pair(static_cast<std::size_t>(arity), static_cast<std::size_t>(index));
   }

   template <is_context Ctx, class It>
   GLZ_ALWAYS_INLINE auto decode_tuple_header(Ctx&& ctx, It&& it) noexcept
   {
      int arity{};
      int index{};
      if (ei_decode_tuple_header(reinterpret_cast<const char*>(it), &index, &arity) < 0) [[unlikely]] {
         ctx.error = error_code::syntax_error;
         return header_pair(-1ull, -1ull);
      }

      return header_pair(static_cast<std::size_t>(arity), static_cast<std::size_t>(index));
   }

   template <class B, class IX>
   GLZ_ALWAYS_INLINE void encode_version(is_context auto&& ctx, B&& b, IX&& ix) noexcept
   {
      int index{static_cast<int>(ix)};
      if (ei_encode_version(reinterpret_cast<char*>(b.data()), &index) < 0) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return;
      }

      ix = index;
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_boolean(const bool value, Args&&... args)
   {
      detail::encode_impl([&](char* buf, int* index) { return ei_encode_boolean(buf, index, value); },
                          std::forward<Args>(args)...);
   }

   template <class T, class... Args>
   GLZ_ALWAYS_INLINE void encode_number(T&& value, Args&&... args)
   {
      using V = std::remove_cvref_t<T>;
      if constexpr (std::floating_point<std::remove_cvref_t<T>>) {
         detail::encode_impl([&](char* buf, int* index) { return ei_encode_double(buf, index, value); },
                             std::forward<Args>(args)...);
      }
      else if constexpr (sizeof(T) > sizeof(long)) {
         if constexpr (std::is_signed_v<V>) {
            detail::encode_impl([&](char* buf, int* index) { return ei_encode_longlong(buf, index, value); },
                                std::forward<Args>(args)...);
         }
         else {
            detail::encode_impl([&](char* buf, int* index) { return ei_encode_ulonglong(buf, index, value); },
                                std::forward<Args>(args)...);
         }
      }
      else {
         if constexpr (std::is_signed_v<V>) {
            detail::encode_impl([&](char* buf, int* index) { return ei_encode_long(buf, index, value); },
                                std::forward<Args>(args)...);
         }
         else {
            detail::encode_impl([&](char* buf, int* index) { return ei_encode_ulong(buf, index, value); },
                                std::forward<Args>(args)...);
         }
      }
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_atom(auto&& value, Args&&... args)
   {
      detail::encode_impl([&](char* buf, int* index) { return ei_encode_atom(buf, index, value.data()); },
                          std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_atom_len(auto&& value, std::size_t sz, Args&&... args)
   {
      detail::encode_impl(
         [&](char* buf, int* index) { return ei_encode_atom_len(buf, index, value.data(), static_cast<int>(sz)); },
         std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_string(auto&& value, Args&&... args)
   {
      detail::encode_impl([&](char* buf, int* index) { return ei_encode_string(buf, index, value.data()); },
                          std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_tuple_header(std::size_t arity, Args&&... args)
   {
      detail::encode_impl(
         [&](char* buf, int* index) { return ei_encode_tuple_header(buf, index, static_cast<int>(arity)); },
         std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_list_header(std::size_t arity, Args&&... args)
   {
      detail::encode_impl(
         [&](char* buf, int* index) { return ei_encode_list_header(buf, index, static_cast<int>(arity)); },
         std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_list_tail(Args&&... args)
   {
      detail::encode_impl([&](char* buf, int* index) { return ei_encode_list_header(buf, index, 0); },
                          std::forward<Args>(args)...);
   }

   template <class... Args>
   GLZ_ALWAYS_INLINE void encode_map_header(std::size_t arity, Args&&... args)
   {
      detail::encode_impl(
         [&](char* buf, int* index) { return ei_encode_map_header(buf, index, static_cast<int>(arity)); },
         std::forward<Args>(args)...);
   }

} // namespace glz
