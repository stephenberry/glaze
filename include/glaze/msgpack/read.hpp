// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <chrono>
#include <cstring>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "glaze/core/chrono.hpp"
#include "glaze/core/common.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/json/generic_fwd.hpp"
#include "glaze/msgpack/common.hpp"
#include "glaze/msgpack/skip.hpp"
#include "glaze/util/bit_array.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/variant.hpp"

// Recursion depth: a fixarray or fixmap nesting level costs a single byte, so input alone can drive
// the reader arbitrarily deep and overflow the stack. Every reader that reads an array or map length
// and then descends into the entries takes one level with glz::depth_guard, and skip_value<MSGPACK>
// does the same, which bounds the descent at max_recursive_depth_limit and reports
// exceeded_max_recursive_depth instead of crashing. A reader that delegates to another reader (a
// reflectable struct defers to the tuple reader) leaves the level to that reader.

namespace glz::msgpack::detail
{
   template <class It>
   GLZ_ALWAYS_INLINE bool read_integer_value(is_context auto& ctx, uint8_t tag, It& it, const It& end, bool& is_signed,
                                             int64_t& signed_value, uint64_t& unsigned_value) noexcept
   {
      if (is_positive_fixint(tag)) {
         is_signed = false;
         unsigned_value = tag;
         return true;
      }
      if (is_negative_fixint(tag)) {
         is_signed = true;
         signed_value = static_cast<int8_t>(tag);
         return true;
      }

      switch (tag) {
      case uint8: {
         uint8_t v{};
         if (!read_uint8(ctx, it, end, v)) {
            return false;
         }
         is_signed = false;
         unsigned_value = v;
         return true;
      }
      case uint16: {
         uint16_t v{};
         if (!read_uint16(ctx, it, end, v)) {
            return false;
         }
         is_signed = false;
         unsigned_value = v;
         return true;
      }
      case uint32: {
         uint32_t v{};
         if (!read_uint32(ctx, it, end, v)) {
            return false;
         }
         is_signed = false;
         unsigned_value = v;
         return true;
      }
      case uint64: {
         uint64_t v{};
         if (!read_uint64(ctx, it, end, v)) {
            return false;
         }
         is_signed = false;
         unsigned_value = v;
         return true;
      }
      case int8: {
         uint8_t v{};
         if (!read_uint8(ctx, it, end, v)) {
            return false;
         }
         is_signed = true;
         signed_value = static_cast<int8_t>(v);
         return true;
      }
      case int16: {
         uint16_t v{};
         if (!read_uint16(ctx, it, end, v)) {
            return false;
         }
         is_signed = true;
         signed_value = static_cast<int16_t>(v);
         return true;
      }
      case int32: {
         uint32_t v{};
         if (!read_uint32(ctx, it, end, v)) {
            return false;
         }
         is_signed = true;
         signed_value = static_cast<int32_t>(v);
         return true;
      }
      case int64: {
         uint64_t v{};
         if (!read_uint64(ctx, it, end, v)) {
            return false;
         }
         is_signed = true;
         signed_value = static_cast<int64_t>(v);
         return true;
      }
      default:
         ctx.error = error_code::syntax_error;
         return false;
      }
   }

   template <class It>
   GLZ_ALWAYS_INLINE bool read_string_view(is_context auto& ctx, uint8_t tag, It& it, const It& end,
                                           std::string_view& out) noexcept
   {
      size_t len{};
      if (!read_str_length(ctx, tag, it, end, len)) {
         return false;
      }
      if (static_cast<size_t>(end - it) < len) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return false;
      }
      out = std::string_view{it, len};
      it += len;
      return true;
   }

   template <class It>
   GLZ_ALWAYS_INLINE bool read_binary_view(is_context auto& ctx, uint8_t tag, It& it, const It& end,
                                           std::string_view& out) noexcept
   {
      size_t len{};
      if (!read_bin_length(ctx, tag, it, end, len)) {
         return false;
      }
      if (static_cast<size_t>(end - it) < len) [[unlikely]] {
         ctx.error = error_code::unexpected_end;
         return false;
      }
      out = std::string_view{it, len};
      it += len;
      return true;
   }

   template <class T>
   GLZ_ALWAYS_INLINE constexpr bool should_skip_field()
   {
      // Mirrors the writer's count_members predicate so structs_as_arrays reads
      // stay aligned with the wire layout, including is_includer and types
      // opted out via meta::value = skip{}.
      return always_skipped<T>;
   }

}

namespace glz
{
   template <>
   struct parse<MSGPACK>
   {
      // 5-parameter version with tag - for when header has already been read
      template <auto Opts, class T, class Tag, is_context Ctx, class It, class End>
         requires(check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(T&& value, Tag&& tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if constexpr (const_value_v<T>) {
            if constexpr (check_error_on_const_read(Opts)) {
               ctx.error = error_code::attempt_const_read;
            }
            else {
               skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
            }
            return;
         }

         using V = std::remove_cvref_t<T>;
         from<MSGPACK, V>::template op<Opts>(std::forward<T>(value), std::forward<Tag>(tag), std::forward<Ctx>(ctx), it,
                                             end);
      }

      // 4-parameter version without tag - reads header first.
      //
      // Accepts a no_header Opts and clears it for the callee. no_header says that the tag of the
      // value being read was already consumed and is passed alongside it -- a statement about that
      // one value, not about its children, which carry their own tags and reach the reader through
      // here. from<MSGPACK, glaze_value_t> turns it on so that glz::cast's tag overload is viable;
      // leaving it set made this overload not viable one level down, so a glaze_value_t wrapping a
      // container or an aggregate failed to compile on read.
      template <auto Opts, class T, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if constexpr (const_value_v<T>) {
            if constexpr (check_error_on_const_read(Opts)) {
               ctx.error = error_code::attempt_const_read;
            }
            else {
               skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
            }
            return;
         }

         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         const uint8_t tag = static_cast<uint8_t>(*it++);
         from<MSGPACK, std::remove_cvref_t<T>>::template op<no_header_off<Opts>()>(std::forward<T>(value), tag, ctx, it,
                                                                                   end);
      }
   };

   template <class T>
      requires(glaze_value_t<T> && !custom_read<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         using V = std::remove_cvref_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         from<MSGPACK, V>::template op<no_header_on<Opts>()>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                                             tag, ctx, it, end);
      }
   };

   // Silently consume any value bound to a glz::skip sentinel. Reached when a
   // type opts out of serialization via meta::value = glz::skip{}.
   template <>
   struct from<MSGPACK, skip>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&&, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         skip_value<MSGPACK>::template op<Opts>(tag, ctx, it, end);
      }
   };

   template <always_null_t T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&&, uint8_t tag, Ctx&& ctx, It&, const End&) noexcept
      {
         if (tag != msgpack::nil) {
            ctx.error = error_code::syntax_error;
         }
      }
   };

   template <nullable_like T>
      requires(!std::is_pointer_v<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if (tag == msgpack::nil) {
            value.reset();
            return;
         }
         if (!value) {
            value.emplace();
         }
         from<MSGPACK, std::remove_cvref_t<decltype(*value)>>::template op<Opts>(*value, tag, ctx, it, end);
      }
   };

   // Raw pointer support
   template <class T>
      requires(std::is_pointer_v<T> && !std::is_array_v<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if (tag == msgpack::nil) {
            // null pointer - do nothing (can't reset a raw pointer safely)
            return;
         }
         if (!value) {
            if constexpr (can_allocate_raw_pointer<Opts, std::decay_t<Ctx>>) {
               if (!try_allocate_raw_pointer<Opts>(value, ctx)) {
                  return;
               }
            }
            else {
               ctx.error = error_code::invalid_nullable_read;
               return;
            }
         }
         from<MSGPACK, std::remove_pointer_t<std::remove_cvref_t<Value>>>::template op<Opts>(*value, tag, ctx, it, end);
      }
   };

   template <class T>
      requires(nullable_value_t<T> && !nullable_like<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if (tag == msgpack::nil) {
            value.reset();
            return;
         }
         if (!value.has_value()) {
            value.emplace();
         }
         from<MSGPACK, std::remove_cvref_t<decltype(value.value())>>::template op<Opts>(value.value(), tag, ctx, it,
                                                                                        end);
      }
   };

   template <nullable_wrapper T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if (tag == msgpack::nil) {
            value.val.reset();
            return;
         }
         if (!value.val) {
            value.val.emplace();
         }
         from<MSGPACK, std::remove_cvref_t<decltype(*value.val)>>::template op<Opts>(*value.val, tag, ctx, it, end);
      }
   };

   template <boolean_like T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It&, const End&) noexcept
      {
         if (tag == msgpack::bool_true) {
            value = true;
         }
         else if (tag == msgpack::bool_false) {
            value = false;
         }
         else {
            ctx.error = error_code::expected_true_or_false;
         }
      }
   };

   template <is_bitset T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         size_t len{};
         if (!msgpack::read_bin_length(ctx, tag, it, end, len)) {
            return;
         }

         const auto num_bytes = (value.size() + 7) / 8;
         if (len != num_bytes) {
            ctx.error = error_code::syntax_error;
            return;
         }

         if (static_cast<size_t>(end - it) < num_bytes) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i, ++it) {
            uint8_t byte = static_cast<uint8_t>(*it);
            for (size_t bit_i = 0; bit_i < 8 && i < value.size(); ++bit_i, ++i) {
               value[i] = (byte >> bit_i) & uint8_t(1);
            }
         }
      }
   };

   template <class T>
      requires(std::is_enum_v<T> && !glaze_enum_t<T> && !custom_read<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         using U = std::underlying_type_t<std::decay_t<T>>;
         U temp{};
         from<MSGPACK, U>::template op<Opts>(temp, tag, ctx, it, end);
         if (ctx.error == error_code::none) {
            value = static_cast<T>(temp);
         }
      }
   };

   template <class T>
      requires(is_named_enum<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         std::string_view sv{};
         if (!msgpack::detail::read_string_view(ctx, tag, it, end, sv)) {
            return;
         }

         constexpr auto N = reflect<T>::size;

         if constexpr (N == 0) {
            ctx.error = error_code::unexpected_enum;
            return;
         }
         else if constexpr (N == 1) {
            if (sv == get<0>(reflect<T>::keys)) {
               value = get<0>(reflect<T>::values);
            }
            else {
               ctx.error = error_code::unexpected_enum;
            }
         }
         else {
            static constexpr auto HashInfo = hash_info<T>;
            const auto index = decode_hash_with_size<MSGPACK, T, HashInfo, HashInfo.type>::op(
               sv.data(), sv.data() + sv.size(), sv.size());

            if (index >= N || reflect<T>::keys[index] != sv) [[unlikely]] {
               ctx.error = error_code::unexpected_enum;
               return;
            }

            visit<N>([&]<size_t I>() { value = get<I>(reflect<T>::values); }, index);
         }
      }
   };

   template <class T>
      requires(num_t<T> || char_t<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         using V = std::remove_cvref_t<decltype(value)>;
         if constexpr (std::floating_point<V>) {
            if (tag == msgpack::float32) {
               float temp{};
               if (!msgpack::read_float32(ctx, it, end, temp)) {
                  return;
               }
               value = static_cast<V>(temp);
               return;
            }
            if (tag == msgpack::float64) {
               double temp{};
               if (!msgpack::read_float64(ctx, it, end, temp)) {
                  return;
               }
               value = static_cast<V>(temp);
               return;
            }

            // An integer is a number, so a floating point target accepts one -- but only as a
            // conversion. Variant resolution runs a strict pass first so that `double` cannot claim
            // a value a later integer alternative holds exactly.
            if constexpr (check_allow_conversions(Opts)) {
               bool is_signed{};
               int64_t signed_value{};
               uint64_t unsigned_value{};
               if (!msgpack::detail::read_integer_value(ctx, tag, it, end, is_signed, signed_value, unsigned_value)) {
                  return;
               }
               if (is_signed) {
                  value = static_cast<V>(signed_value);
               }
               else {
                  value = static_cast<V>(unsigned_value);
               }
            }
            else {
               ctx.error = error_code::syntax_error;
            }
         }
         else {
            bool is_signed{};
            int64_t signed_value{};
            uint64_t unsigned_value{};
            if (!msgpack::detail::read_integer_value(ctx, tag, it, end, is_signed, signed_value, unsigned_value)) {
               return;
            }

            if constexpr (std::is_signed_v<V>) {
               if (is_signed) {
                  if (signed_value < (std::numeric_limits<V>::min)() ||
                      signed_value > (std::numeric_limits<V>::max)()) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  value = static_cast<V>(signed_value);
               }
               else {
                  // Range-check the unsigned magnitude itself. Narrowing to int64_t first and then
                  // checking wraps UINT64_MAX to -1, which is in range for every signed V, so the
                  // widest unsigned input was accepted as -1 instead of being rejected.
                  if (unsigned_value > static_cast<uint64_t>((std::numeric_limits<V>::max)())) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  value = static_cast<V>(unsigned_value);
               }
            }
            else {
               if (is_signed) {
                  if (signed_value < 0) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  if (static_cast<uint64_t>(signed_value) > (std::numeric_limits<V>::max)()) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  value = static_cast<V>(signed_value);
               }
               else {
                  if (unsigned_value > (std::numeric_limits<V>::max)()) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  value = static_cast<V>(unsigned_value);
               }
            }
         }
      }
   };

   // The four string specializations below are mutually exclusive, which matters beyond avoiding an
   // ambiguity: leaving two of them viable for one type forces the compiler to partially order
   // constrained partial specializations, and normalizing these concepts for that subsumption check
   // is costly enough to exhaust clang 22's stack (see issue #2742, and the note in write.hpp).
   // `string_t` stays disjoint from the other three via its `!string_view_t` and `!is_static_string`
   // clauses, so keep any new string specialization here exclusive as well.
   template <string_t T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         std::string_view sv{};
         if (!msgpack::detail::read_string_view(ctx, tag, it, end, sv)) {
            return;
         }
         value.assign(sv.data(), sv.size());
      }
   };

   template <static_string_t T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         std::string_view sv{};
         if (!msgpack::detail::read_string_view(ctx, tag, it, end, sv)) {
            return;
         }
         if (sv.size() > value.max_size()) {
            ctx.error = error_code::syntax_error;
            return;
         }
         value.assign(sv.data(), sv.size());
      }
   };

   template <string_view_t T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         std::string_view sv{};
         if (!msgpack::detail::read_string_view(ctx, tag, it, end, sv)) {
            return;
         }
         value = sv;
      }
   };

   // Fixed-size std::array<char, N>: read the string into the buffer, bounds-checked,
   // zero-filling any unused tail so a shorter payload yields a deterministic buffer.
   template <array_char_t T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         std::string_view sv{};
         if (!msgpack::detail::read_string_view(ctx, tag, it, end, sv)) {
            return;
         }
         if (sv.size() > value.size()) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         std::memcpy(value.data(), sv.data(), sv.size());
         if (sv.size() < value.size()) {
            std::memset(value.data() + sv.size(), 0, value.size() - sv.size());
         }
      }
   };

   // TagKey names an internally tagged variant's discriminator, which the variant reader has already
   // resolved. It appears in this object's map like any other key, so tolerate it: skip it unless the
   // alternative declares a member of that name, in which case it reads normally into that member.
   // Mirrors the JSON object reader's `string_literal tag` parameter.
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && !custom_read<T>)
   struct from<MSGPACK, T>
   {
      static constexpr auto N = reflect<T>::size;

      template <auto Opts, string_literal TagKey = "", class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T>) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         if constexpr (check_structs_as_arrays(Opts)) {
            size_t len{};
            if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
               return;
            }

            size_t idx = 0;
            for_each<N>([&]<size_t I>() {
               if (ctx.error != error_code::none) {
                  return;
               }
               if constexpr (!msgpack::detail::should_skip_field<field_t<T, I>>()) {
                  if (idx >= len) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  if constexpr (reflectable<T>) {
                     parse<MSGPACK>::template op<Opts>(get_member(value, get<I>(t)), ctx, it, end);
                  }
                  else {
                     parse<MSGPACK>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
                  }
                  ++idx;
               }
            });

            // skip remaining entries if len > number of fields
            for (; idx < len && ctx.error == error_code::none; ++idx) {
               skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
            }
         }
         else {
            size_t len{};
            if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
               return;
            }

            static constexpr bit_array<N> tracked_fields = [] {
               bit_array<N> arr{};
               if constexpr (N > 0) {
                  for_each<N>([&]<size_t I>() {
                     if constexpr (!msgpack::detail::should_skip_field<field_t<T, I>>()) {
                        arr[I] = true;
                     }
                  });
               }
               return arr;
            }();

            auto fields = [&]() -> decltype(auto) {
               if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
                  return bit_array<N>{};
               }
               else {
                  return nullptr;
               }
            }();

            static constexpr auto HashInfo = hash_info<T>;

            for (size_t pair_i = 0; pair_i < len && ctx.error == error_code::none; ++pair_i) {
               if (it >= end) {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               const uint8_t key_tag = static_cast<uint8_t>(*it++);
               std::string_view key{};
               if (!msgpack::detail::read_string_view(ctx, key_tag, it, end, key)) {
                  return;
               }

               const auto index = decode_hash_with_size<MSGPACK, T, HashInfo, HashInfo.type>::op(
                  key.data(), key.data() + key.size(), key.size());

               if (index >= N || reflect<T>::keys[index] != key) {
                  if constexpr (not TagKey.sv().empty()) {
                     // The discriminator the variant reader already consumed conceptually.
                     if (key == TagKey.sv()) {
                        skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
                        continue;
                     }
                  }
                  if constexpr (Opts.error_on_unknown_keys) {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
                  else {
                     skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
                     continue;
                  }
               }

               visit<N>(
                  [&]<size_t I>() {
                     if constexpr (msgpack::detail::should_skip_field<field_t<T, I>>()) {
                        skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
                     }
                     else {
                        if constexpr (reflectable<T>) {
                           parse<MSGPACK>::template op<Opts>(get_member(value, get<I>(t)), ctx, it, end);
                        }
                        else {
                           parse<MSGPACK>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it,
                                                             end);
                        }
                        if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
                           fields[I] = true;
                        }
                     }
                  },
                  index);

               if constexpr (Opts.partial_read) {
                  if (ctx.error == error_code::partial_read_complete) {
                     return;
                  }
                  if ((fields & tracked_fields) == tracked_fields) {
                     ctx.error = error_code::partial_read_complete;
                     return;
                  }
               }
            }

            if constexpr (Opts.error_on_missing_keys) {
               constexpr auto req_fields = required_fields<T, Opts>();
               if ((req_fields & fields) != req_fields) {
                  for (size_t i = 0; i < N; ++i) {
                     if (not fields[i] && req_fields[i]) {
                        ctx.custom_error_message = reflect<T>::keys[i];
                        break;
                     }
                  }
                  ctx.error = error_code::missing_key;
                  return;
               }
            }
         }
      }
   };

   template <writable_map_t T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
            return;
         }

         if constexpr (!Opts.partial_read) {
            value.clear();
            if constexpr (has_reserve<std::decay_t<Value>>) {
               // Each map entry is a key plus a value, so it occupies at least two bytes on the
               // wire and a valid len can never exceed the bytes remaining. Cap the reservation
               // against the input size to avoid an allocation bomb from a tiny header (e.g. map32
               // claiming 2^32-1 entries); the loop below still parses every entry and reports
               // unexpected_end on truncated input.
               const size_t remaining = size_t(end - it);
               value.reserve(len < remaining ? len : remaining);
            }

            for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
               typename std::decay_t<Value>::key_type key{};
               parse<MSGPACK>::template op<Opts>(key, ctx, it, end);
               if (ctx.error != error_code::none) {
                  return;
               }
               auto [it_insert, _] = value.emplace(std::move(key), typename std::decay_t<Value>::mapped_type{});
               parse<MSGPACK>::template op<Opts>(it_insert->second, ctx, it, end);
            }
         }
         else {
            for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
               typename std::decay_t<Value>::key_type key{};
               parse<MSGPACK>::template op<Opts>(key, ctx, it, end);
               if (ctx.error != error_code::none) {
                  return;
               }
               if (auto it_existing = value.find(key); it_existing != value.end()) {
                  parse<MSGPACK>::template op<Opts>(it_existing->second, ctx, it, end);
               }
               else {
                  skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
               }
            }
         }
      }
   };

   // for set-like containers (emplaceable but not emplace_backable)
   template <class T>
      requires(writable_array_t<T> && !emplace_backable<T> && emplaceable<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }

         value.clear();
         for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
            using V = range_value_t<std::decay_t<Value>>;
            V v{};
            parse<MSGPACK>::template op<Opts>(v, ctx, it, end);
            if (ctx.error != error_code::none) {
               return;
            }
            value.emplace(std::move(v));
         }
      }
   };

   // for vector-like containers (emplace_backable) and fixed-size arrays
   template <class T>
      requires(writable_array_t<T> && (emplace_backable<T> || !emplaceable<T>))
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         using Range = std::remove_reference_t<Value>;
         if constexpr (msgpack::binary_range_v<Range>) {
            using Reference = std::ranges::range_reference_t<Range>;
            if constexpr (!std::is_const_v<std::remove_reference_t<Reference>>) {
               if (tag == msgpack::bin8 || tag == msgpack::bin16 || tag == msgpack::bin32) {
                  std::string_view payload{};
                  if (!msgpack::detail::read_binary_view(ctx, tag, it, end, payload)) {
                     return;
                  }
                  const size_t len = payload.size();
                  if constexpr (resizable<std::remove_cvref_t<Range>>) {
                     if (exceeds_capacity(value, len, ctx)) [[unlikely]] {
                        return;
                     }
                     value.clear();
                     if constexpr (has_reserve<std::remove_cvref_t<Range>>) {
                        value.reserve(len);
                     }
                     value.resize(len);
                  }
                  else {
                     if (len > value.size()) {
                        ctx.error = error_code::exceeded_static_array_size;
                        return;
                     }
                  }
                  if (len > 0) {
                     std::memcpy(value.data(), payload.data(), len);
                  }
                  if constexpr (!resizable<std::remove_cvref_t<Range>>) {
                     for (size_t i = len; i < value.size(); ++i) {
                        value[i] = {};
                     }
                  }
                  return;
               }
            }
         }

         // Placed after the binary-range branch above, which is flat and returns before this point.
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }

         if constexpr (emplace_backable<std::decay_t<Value>>) {
            if (exceeds_capacity(value, len, ctx)) [[unlikely]] {
               return;
            }
            value.clear();
            if constexpr (has_reserve<std::decay_t<Value>>) {
               // Each element occupies at least one byte on the wire, so a valid len can never
               // exceed the bytes remaining. Cap the reservation against the input size to avoid an
               // allocation bomb from a tiny header (e.g. array32 claiming 2^32-1 elements); the
               // loop below still parses every element and reports unexpected_end on truncated input.
               const size_t remaining = size_t(end - it);
               value.reserve(len < remaining ? len : remaining);
            }
            for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
               value.emplace_back();
               parse<MSGPACK>::template op<Opts>(value.back(), ctx, it, end);
               if (ctx.error != error_code::none) {
                  return;
               }
            }
         }
         else {
            // Fixed-size array
            if (len > value.size()) {
               ctx.error = error_code::exceeded_static_array_size;
               return;
            }
            size_t i = 0;
            for (; i < len && ctx.error == error_code::none; ++i) {
               parse<MSGPACK>::template op<Opts>(value[i], ctx, it, end);
            }
            for (; i < value.size(); ++i) {
               value[i] = {};
            }
         }
      }
   };

   template <>
   struct from<MSGPACK, msgpack::ext>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         size_t len{};
         int8_t type{};
         if (!msgpack::read_ext_header(ctx, tag, it, end, len, type)) {
            return;
         }
         value.type = type;
         if (exceeds_capacity(value.data, len, ctx)) [[unlikely]] {
            return;
         }
         value.data.resize(len);
         if (len > 0) {
            std::memcpy(value.data.data(), it, len);
            it += len;
         }
      }
   };

   // MessagePack timestamp extension (type -1)
   // Supports all three timestamp formats:
   // - Timestamp 32 (fixext 4): seconds only
   // - Timestamp 64 (fixext 8): 30-bit nanoseconds + 34-bit seconds
   // - Timestamp 96 (ext 8 with 12 bytes): 32-bit nanoseconds + 64-bit signed seconds
   template <>
   struct from<MSGPACK, msgpack::timestamp>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         size_t len{};
         int8_t type{};
         if (!msgpack::read_ext_header(ctx, tag, it, end, len, type)) {
            return;
         }

         if (type != msgpack::timestamp_type) {
            ctx.error = error_code::syntax_error;
            return;
         }

         switch (len) {
         case 4: {
            // Timestamp 32: 4 bytes, seconds only (uint32)
            uint32_t sec32{};
            if (!msgpack::read_uint32(ctx, it, end, sec32)) {
               return;
            }
            value.seconds = sec32;
            value.nanoseconds = 0;
            break;
         }
         case 8: {
            // Timestamp 64: 8 bytes
            // Upper 30 bits: nanoseconds, lower 34 bits: seconds
            uint64_t val64{};
            if (!msgpack::read_uint64(ctx, it, end, val64)) {
               return;
            }
            value.nanoseconds = static_cast<uint32_t>(val64 >> 34);
            value.seconds = static_cast<int64_t>(val64 & 0x3FFFFFFFF);
            break;
         }
         case 12: {
            // Timestamp 96: 12 bytes
            // First 4 bytes: nanoseconds (uint32)
            // Next 8 bytes: seconds (int64)
            uint32_t nsec{};
            if (!msgpack::read_uint32(ctx, it, end, nsec)) {
               return;
            }
            uint64_t sec64{};
            if (!msgpack::read_uint64(ctx, it, end, sec64)) {
               return;
            }
            value.nanoseconds = nsec;
            value.seconds = static_cast<int64_t>(sec64);
            break;
         }
         default:
            ctx.error = error_code::syntax_error;
            return;
         }
      }
   };

   // std::chrono::system_clock::time_point support
   // Converts from msgpack::timestamp during deserialization
   template <class T>
      requires std::same_as<std::remove_cvref_t<T>, std::chrono::system_clock::time_point>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         msgpack::timestamp ts;
         from<MSGPACK, msgpack::timestamp>::template op<Opts>(ts, tag, ctx, it, end);
         if (ctx.error != error_code::none) {
            return;
         }

         using namespace std::chrono;
         value = system_clock::time_point{
            duration_cast<system_clock::duration>(seconds{ts.seconds} + nanoseconds{ts.nanoseconds})};
      }
   };

   template <glaze_array_t T>
      requires(!custom_read<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }
         static constexpr auto N = reflect<T>::size;
         if (len != N) {
            ctx.error = error_code::syntax_error;
            return;
         }
         for_each<N>([&]<size_t I>() {
            if (ctx.error == error_code::none) {
               parse<MSGPACK>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
            }
         });
      }
   };

   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }
         static constexpr auto N = glz::tuple_size_v<T>;
         if (len != N) {
            ctx.error = error_code::syntax_error;
            return;
         }
         if constexpr (is_std_tuple<T>) {
            [&]<size_t... I>(std::index_sequence<I...>) {
               (parse<MSGPACK>::template op<Opts>(std::get<I>(value), ctx, it, end), ...);
            }(std::make_index_sequence<N>{});
         }
         else {
            [&]<size_t... I>(std::index_sequence<I...>) {
               (parse<MSGPACK>::template op<Opts>(glz::get<I>(value), ctx, it, end), ...);
            }(std::make_index_sequence<N>{});
         }
      }
   };

   // The counterpart of to<MSGPACK, T>: the shape is whatever glz::meta declared, and an undeclared
   // variant is a bare value resolved from MessagePack's own self-describing type byte.
   template <is_variant T>
      requires(not custom_read<T>)
   struct from<MSGPACK, T>
   {
      static constexpr auto tagging = variant_tagging_v<T>;
      static constexpr size_t variant_size = std::variant_size_v<T>;

      // One pass over the alternatives at a fixed conversion strictness, rewinding after each miss.
      // Every MessagePack reader validates the type byte it was handed, so a wrong alternative fails
      // on the first byte rather than consuming input. Alternatives that share a wire shape cannot be
      // told apart -- declare a `tag` in glz::meta when that matters.
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      static bool try_each_pass(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end,
                                error_code& input_error) noexcept
      {
         bool matched = false;
         bool exhausted = false;
         const auto start = it;
         for_each<variant_size>([&]<size_t I>() {
            if (matched || exhausted || input_error == error_code::exceeded_max_recursive_depth) {
               // Nesting past the limit is a property of the input: no other alternative can read it,
               // and re-parsing the subtree per alternative at every level is exponential.
               return;
            }
            using V = std::variant_alternative_t<I, T>;
            it = start;
            ctx.error = error_code::none;
            ctx.custom_error_message = {}; // else a rejected alternative's message outlives its error
            // Read into a fresh alternative and move it in, so a miss leaves `value` as it was.
            V v{};
            from<MSGPACK, V>::template op<Opts>(v, tag, ctx, it, end);
            // Charge only what a REJECTED attempt parsed. That is the wasted work the bound is about;
            // charging the match too would bill every enclosing level for the same bytes and make a
            // valid nest look exponential.
            const bool budget_left = bool(ctx.error) ? charge_speculation(ctx, size_t(it - start)) : true;
            if (!bool(ctx.error)) {
               value.template emplace<I>(std::move(v));
               matched = true;
            }
            else if (ctx.error == error_code::unexpected_end || ctx.error == error_code::exceeded_max_recursive_depth) {
               input_error = ctx.error;
            }
            if (!budget_left) {
               // Out of speculation budget: keep this alternative's error and stop.
               exhausted = true;
            }
         });
         if (!matched) {
            it = start;
         }
         return matched;
      }

      // Resolve an undeclared variant from MessagePack's own type byte.
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      static void try_each(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         const auto start = it;
         error_code input_error{};
         // Strict first, so a lenient reader cannot claim a value an exact alternative wants: the
         // floating point reader accepts an integer under allow_conversions, which would otherwise
         // let `double` take a value that a later `int64_t` alternative holds exactly.
         if constexpr (check_allow_conversions(Opts)) {
            if (try_each_pass<opt_false<Opts, allow_conversions_opt_tag{}>>(value, tag, ctx, it, end, input_error)) {
               return;
            }
         }
         // A lenient retry cannot make over-nested input readable, and running it doubles the work at
         // every level of a deep nest.
         if (input_error != error_code::exceeded_max_recursive_depth &&
             try_each_pass<Opts>(value, tag, ctx, it, end, input_error)) {
            return;
         }
         it = start;
         // An incomplete or over-nested buffer is a property of the input, not of the alternative set,
         // so report it as such instead of blaming variant resolution.
         ctx.error = input_error != error_code::none ? input_error : error_code::no_matching_variant_type;
      }

      // Decode the discriminator value at `it` and map it to an alternative index, advancing past it.
      // Returns variant_size when the id names no alternative and there is no unlabeled default.
      template <auto Opts, class It, class End>
      static size_t resolve_id(is_context auto&& ctx, It& it, const End& end) noexcept
      {
         using id_type = std::decay_t<decltype(ids_v<T>[0])>;
         size_t index = ids_v<T>.size();

         if (it >= end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return variant_size;
         }
         const uint8_t id_tag = static_cast<uint8_t>(*it++);

         if constexpr (std::integral<id_type>) {
            bool is_signed{};
            int64_t signed_id{};
            uint64_t unsigned_id{};
            if (!msgpack::detail::read_integer_value(ctx, id_tag, it, end, is_signed, signed_id, unsigned_id)) {
               return variant_size;
            }
            index = variant_id_to_index<T>::op(is_signed ? static_cast<id_type>(signed_id)
                                                         : static_cast<id_type>(unsigned_id));
         }
         else {
            sv id{};
            if (!msgpack::detail::read_string_view(ctx, id_tag, it, end, id)) {
               return variant_size;
            }
            index = variant_id_to_index<T>::op(id.data(), id.data() + id.size(), id.size());
         }

         return variant_index_from_id<T>(index);
      }

      // Walk the map once, resolving the discriminator and consuming every entry. `on_content` sees
      // each non-discriminator key and decides whether to parse or skip its value.
      template <auto Opts, class It, class End>
      static size_t scan_map(is_context auto&& ctx, uint8_t tag, It& it, const End& end, auto&& on_content) noexcept
      {
         size_t len{};
         if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
            return variant_size;
         }
         size_t index = variant_size;
         for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return variant_size;
            }
            const uint8_t key_tag = static_cast<uint8_t>(*it++);
            sv key{};
            if (!msgpack::detail::read_string_view(ctx, key_tag, it, end, key)) {
               return variant_size;
            }
            if (key == tag_v<T>) {
               index = resolve_id<Opts>(ctx, it, end);
            }
            else {
               on_content(key);
            }
         }
         return index;
      }

      template <auto Opts, class Value, is_context Ctx, class It, class End>
      static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if constexpr (tagging == variant_tagging_kind::none) {
            try_each<Opts>(value, tag, ctx, it, end);
            return;
         }
         else if constexpr (check_structs_as_arrays(Opts)) {
            // Positional data carries no keys, so an adjacently tagged variant was written as the two
            // element array [id, value]. Internal tagging cannot be written positionally at all, and
            // the writer static_asserts on it, so only the adjacent shape reaches here.
            depth_guard guard{ctx};
            if (!guard) [[unlikely]] {
               return;
            }
            size_t len{};
            if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
               return;
            }
            if (len != 2) [[unlikely]] {
               ctx.error = error_code::invalid_variant_array;
               return;
            }
            const size_t index = resolve_id<Opts>(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (index >= variant_size) [[unlikely]] {
               ctx.error = error_code::no_matching_variant_type;
               ctx.custom_error_message = variant_ids_string_v<T>;
               return;
            }
            if (value.index() != index) {
               emplace_runtime_variant(value, index);
            }
            std::visit([&](auto&& v) { parse<MSGPACK>::template op<Opts>(v, ctx, it, end); }, value);
         }
         else {
            // The adjacent form is a map this reader consumes itself, so it owns that level. The
            // internal form hands the same map to the alternative's object reader, which guards it --
            // counting it here too would halve the nesting a tagged variant is allowed.
            [[maybe_unused]] depth_guard guard{ctx};
            if constexpr (tagging == variant_tagging_kind::adjacent) {
               if (!guard) [[unlikely]] {
                  return;
               }
            }

            static constexpr auto tag_literal = string_literal_from_view<tag_v<T>.size()>(tag_v<T>);
            const auto start = it;

            // One walk resolves the discriminator wherever it sits in the map and consumes the whole
            // item, noting where the content value began so the adjacent form can return to it. An
            // alternative that needs no body is therefore already finished when the walk returns.
            It content_it{};
            bool content_seen = false;
            const size_t index = scan_map<Opts>(ctx, tag, it, end, [&](sv key) {
               if constexpr (tagging == variant_tagging_kind::adjacent) {
                  if (not content_seen && key == content_v<T>) {
                     content_it = it;
                     content_seen = true;
                  }
               }
               skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
            });
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (index >= variant_size) [[unlikely]] {
               ctx.error = error_code::no_matching_variant_type;
               ctx.custom_error_message = variant_ids_string_v<T>;
               return;
            }

            if constexpr (tagging == variant_tagging_kind::adjacent) {
               if (not content_seen) [[unlikely]] {
                  // Resolving the discriminator is not enough: without the content key there is no
                  // value. Checked before emplacing so a rejected buffer leaves `value` as it was.
                  ctx.error = error_code::missing_key;
                  ctx.custom_error_message = content_v<T>;
                  return;
               }
            }

            const auto after = it;
            if (value.index() != index) {
               emplace_runtime_variant(value, index);
            }

            if constexpr (tagging == variant_tagging_kind::adjacent) {
               it = content_it;
               std::visit([&](auto&& v) { parse<MSGPACK>::template op<Opts>(v, ctx, it, end); }, value);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               it = after; // the content entry need not be the last one
            }
            else {
               visit<variant_size>(
                  [&]<size_t I>() {
                     using V = std::variant_alternative_t<I, T>;
                     using X = variant_alternative_object_t<V>;
                     constexpr bool struct_like = (glaze_object_t<X> || reflectable<X>) && (not custom_read<X>);

                     if constexpr (variant_unit_alternative<V>) {
                        // Nothing but the discriminator: pass one already consumed the whole map.
                     }
                     else if constexpr (struct_like && (not is_memory_object<V>)) {
                        // Thread the discriminator key through so it is skipped without disabling
                        // unknown-key checking for the alternative's real fields. An alternative that
                        // declares a member of that name keeps receiving its value (JSON parity).
                        it = start;
                        from<MSGPACK, X>::template op<Opts, tag_literal>(std::get<I>(value), tag, ctx, it, end);
                     }
                     else if constexpr (requires { Opts.error_on_unknown_keys; }) {
                        // memory_object / map / pair alternative: tolerate the discriminator key.
                        static constexpr auto AltOpts = [] {
                           auto o = Opts;
                           o.error_on_unknown_keys = false;
                           return o;
                        }();
                        it = start;
                        from<MSGPACK, V>::template op<AltOpts>(std::get<I>(value), tag, ctx, it, end);
                     }
                     else {
                        it = after;
                     }
                  },
                  index);
            }
         }
      }
   };

   template <class T>
      requires is_specialization_v<T, arr>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V>;
         if (len != N) {
            ctx.error = error_code::syntax_error;
            return;
         }
         for_each<N>([&]<size_t I>() {
            if (ctx.error == error_code::none) {
               parse<MSGPACK>::template op<Opts>(glz::get<I>(value.value), ctx, it, end);
            }
         });
      }
   };

   template <class T>
      requires is_specialization_v<T, arr_copy>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V>;
         if (len != N) {
            ctx.error = error_code::syntax_error;
            return;
         }
         for_each<N>([&]<size_t I>() {
            if (ctx.error == error_code::none) {
               parse<MSGPACK>::template op<Opts>(glz::get<I>(value.value), ctx, it, end);
            }
         });
      }
   };

   template <class T>
      requires is_specialization_v<T, obj>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
            return;
         }
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V> / 2;
         if (len != N) {
            ctx.error = error_code::syntax_error;
            return;
         }
         for_each<N>([&]<size_t I>() {
            if (ctx.error == error_code::none) {
               parse<MSGPACK>::template op<Opts>(glz::get<2 * I>(value.value), ctx, it, end);
               if (ctx.error == error_code::none) {
                  parse<MSGPACK>::template op<Opts>(glz::get<2 * I + 1>(value.value), ctx, it, end);
               }
            }
         });
      }
   };

   template <class T>
      requires is_specialization_v<T, obj_copy>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         size_t len{};
         if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
            return;
         }
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V> / 2;
         if (len != N) {
            ctx.error = error_code::syntax_error;
            return;
         }
         for_each<N>([&]<size_t I>() {
            if (ctx.error == error_code::none) {
               parse<MSGPACK>::template op<Opts>(glz::get<2 * I>(value.value), ctx, it, end);
               if (ctx.error == error_code::none) {
                  parse<MSGPACK>::template op<Opts>(glz::get<2 * I + 1>(value.value), ctx, it, end);
               }
            }
         });
      }
   };

   template <class T>
      requires is_includer<T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&&, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         // consume the string but ignore value
         std::string_view sv{};
         if (msgpack::detail::read_string_view(ctx, tag, it, end, sv)) {
            return;
         }
      }
   };

   template <read_supported<MSGPACK> T, is_buffer Buffer>
   [[nodiscard]] error_ctx read_msgpack(T& value, Buffer&& buffer)
   {
      context ctx{};
      return read<opts{.format = MSGPACK}>(value, std::forward<Buffer>(buffer), ctx);
   }

   template <read_supported<MSGPACK> T, is_buffer Buffer>
   [[nodiscard]] expected<T, error_ctx> read_msgpack(Buffer&& buffer)
   {
      T value{};
      context ctx{};
      const auto ec = read<opts{.format = MSGPACK}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return value;
   }

   template <auto Opts = opts{}, read_supported<MSGPACK> T, is_buffer Buffer>
   [[nodiscard]] error_ctx read_file_msgpack(T& value, const sv file_name, Buffer&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto file_error = file_to_buffer(buffer, ctx.current_file);

      if (bool(file_error)) [[unlikely]] {
         return error_ctx{0, file_error};
      }

      return read<set_msgpack<Opts>()>(value, buffer, ctx);
   }
}
