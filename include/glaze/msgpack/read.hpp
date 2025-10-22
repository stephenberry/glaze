// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstring>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "glaze/core/common.hpp"
#include "glaze/core/meta.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/msgpack/common.hpp"
#include "glaze/msgpack/skip.hpp"
#include "glaze/util/bit_array.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/string_literal.hpp"
#include "glaze/util/variant.hpp"

namespace glz::msgpack::detail
{
   template <class It>
   GLZ_ALWAYS_INLINE bool read_integer_value(is_context auto& ctx, uint8_t tag, It& it, const It& end,
                                             bool& is_signed, int64_t& signed_value,
                                             uint64_t& unsigned_value) noexcept
   {
      if (is_positive_fixint(tag)) {
         is_signed = false;
         unsigned_value = tag;
         return true;
      }
      if (is_negative_fixint(tag)) {
         is_signed = true;
         signed_value = static_cast<int8_t>(static_cast<int8_t>(tag));
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
      if ((it + len) > end) [[unlikely]] {
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
      if ((it + len) > end) [[unlikely]] {
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
      if constexpr (std::same_as<T, hidden> || std::same_as<T, skip>) {
         return true;
      }
      else {
         return false;
      }
   }

   template <class T>
   GLZ_ALWAYS_INLINE size_t find_key_index_runtime(const std::string_view key) noexcept
   {
      constexpr auto N = reflect<T>::size;
      for (size_t i = 0; i < N; ++i) {
         if (key == reflect<T>::keys[i]) {
            return i;
         }
      }
      return N;
   }
}

namespace glz
{
   template <>
   struct parse<MSGPACK>
   {
      template <auto Opts, class T, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if constexpr (const_value_v<T>) {
            if constexpr (Opts.error_on_const_read) {
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
         from<MSGPACK, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), tag, ctx, it, end);
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
         from<MSGPACK, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>), tag, ctx, it,
                                             end);
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
      requires((glaze_enum_t<T> || (meta_keys<T> && std::is_enum_v<std::decay_t<T>>)) && !custom_read<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         std::string_view sv{};
         if (!msgpack::detail::read_string_view(ctx, tag, it, end, sv)) {
            return;
         }
         static constexpr auto frozen_map = make_string_to_enum_map<T>();
         auto it_map = frozen_map.find(sv);
         if (it_map != frozen_map.end()) {
            value = static_cast<T>(it_map->second);
         }
         else {
            ctx.error = error_code::unexpected_enum;
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
            bool is_signed{};
            int64_t signed_value{};
            uint64_t unsigned_value{};
            if (!msgpack::detail::read_integer_value(ctx, tag, it, end, is_signed, signed_value, unsigned_value)) {
               return;
            }

            if constexpr (std::is_signed_v<V>) {
               int64_t temp = is_signed ? signed_value : static_cast<int64_t>(unsigned_value);
               if (temp < std::numeric_limits<V>::min() || temp > std::numeric_limits<V>::max()) {
                  ctx.error = error_code::dump_int_error;
                  return;
               }
               value = static_cast<V>(temp);
            }
            else {
               if (is_signed) {
                  if (signed_value < 0) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  if (static_cast<uint64_t>(signed_value) > std::numeric_limits<V>::max()) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  value = static_cast<V>(signed_value);
               }
               else {
                  if (unsigned_value > std::numeric_limits<V>::max()) {
                     ctx.error = error_code::dump_int_error;
                     return;
                  }
                  value = static_cast<V>(unsigned_value);
               }
            }
         }
      }
   };

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

   template <glaze_object_t T>
      requires(!custom_read<T>)
   struct from<MSGPACK, T>
   {
      static constexpr auto N = reflect<T>::size;

      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         if constexpr (Opts.structs_as_arrays) {
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
                  parse<MSGPACK>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
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
                  for (size_t i = 0; i < N; ++i) {
                     if constexpr (!msgpack::detail::should_skip_field<field_t<T, i>>()) {
                        arr[i] = true;
                     }
                  }
               }
               return arr;
            }();

            auto fields = [&]() -> decltype(auto) {
               if constexpr (Opts.partial_read) {
                  return bit_array<N>{};
               }
               else {
                  return nullptr;
               }
            }();

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

               const auto index = msgpack::detail::find_key_index_runtime<T>(key);
               if (index >= N) {
                  if constexpr (Opts.error_on_unknown_keys) {
                     ctx.error = error_code::unknown_key;
                     return;
                  }
                  else {
                     skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
                     continue;
                  }
               }

               bool handled = false;
               for_each<N>([&]<size_t I>() {
                  if (handled || index != I) {
                     return;
                  }
                  handled = true;
                  if constexpr (msgpack::detail::should_skip_field<field_t<T, I>>()) {
                     skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
                  }
                  else {
                     parse<MSGPACK>::template op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
                     if constexpr (Opts.partial_read) {
                        fields[I] = true;
                     }
                  }
               });

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
         }
      }
   };

   template <reflectable T>
      requires(!custom_read<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         from<MSGPACK, decltype(to_tie(value))>::template op<Opts>(to_tie(value), tag, ctx, it, end);
      }
   };

   template <writable_map_t T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         size_t len{};
         if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
            return;
         }

         if constexpr (!Opts.partial_read) {
            value.clear();
            if constexpr (has_reserve<std::decay_t<Value>>) {
               value.reserve(len);
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

   template <writable_array_t T>
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

         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }

         if constexpr (resizable<std::decay_t<Value>>) {
            value.clear();
            if constexpr (has_reserve<std::decay_t<Value>>) {
               value.reserve(len);
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
         if (!msgpack::detail::read_ext_header(ctx, tag, it, end, len, type)) {
            return;
         }
         value.type = type;
         value.data.resize(len);
         if (len > 0) {
            std::memcpy(value.data.data(), it, len);
            it += len;
         }
      }
   };

   template <glaze_array_t T>
      requires(!custom_read<T>)
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
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

   template <is_variant T>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         size_t len{};
         if (!msgpack::read_array_length(ctx, tag, it, end, len)) {
            return;
         }
         if (len != 2) {
            ctx.error = error_code::invalid_variant_array;
            for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
               skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
            }
            return;
         }

         if (it >= end) {
            ctx.error = error_code::unexpected_end;
            return;
         }
         const uint8_t key_tag = static_cast<uint8_t>(*it++);
         std::string_view type_sv{};
         if (!msgpack::detail::read_string_view(ctx, key_tag, it, end, type_sv)) {
            return;
         }

         static constexpr auto ids = ids_v<T>;
         size_t variant_index = static_cast<size_t>(-1);
         for (size_t i = 0; i < ids.size(); ++i) {
            if (type_sv == ids[i]) {
               variant_index = i;
               break;
            }
         }

         if (variant_index >= ids.size()) {
            ctx.error = error_code::no_matching_variant_type;
            skip_value<MSGPACK>::template op<Opts>(ctx, it, end);
            return;
         }

         bool parsed = false;
         auto try_parse = [&](auto index_constant) {
            constexpr size_t I = decltype(index_constant)::value;
            if (variant_index == I) {
               value.template emplace<I>();
               parse<MSGPACK>::template op<Opts>(std::get<I>(value), ctx, it, end);
               parsed = true;
            }
         };
         [&]<size_t... I>(std::index_sequence<I...>) { (try_parse(std::integral_constant<size_t, I>{}), ...); }(
            std::make_index_sequence<std::variant_size_v<T>>{});

         if (!parsed && ctx.error == error_code::none) {
            ctx.error = error_code::no_matching_variant_type;
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
         size_t len{};
         if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
            return;
         }
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V> / 2;
         for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
            parse<MSGPACK>::template op<Opts>(glz::get<2 * i>(value.value), ctx, it, end);
            if (ctx.error != error_code::none) {
               return;
            }
            parse<MSGPACK>::template op<Opts>(glz::get<2 * i + 1>(value.value), ctx, it, end);
         }
      }
   };

   template <class T>
      requires is_specialization_v<T, obj_copy>
   struct from<MSGPACK, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It, class End>
      GLZ_ALWAYS_INLINE static void op(Value&& value, uint8_t tag, Ctx&& ctx, It& it, const End& end) noexcept
      {
         size_t len{};
         if (!msgpack::read_map_length(ctx, tag, it, end, len)) {
            return;
         }
         using V = std::decay_t<decltype(value.value)>;
         static constexpr auto N = glz::tuple_size_v<V> / 2;
         for (size_t i = 0; i < len && ctx.error == error_code::none; ++i) {
            parse<MSGPACK>::template op<Opts>(glz::get<2 * i>(value.value), ctx, it, end);
            if (ctx.error != error_code::none) {
               return;
            }
            parse<MSGPACK>::template op<Opts>(glz::get<2 * i + 1>(value.value), ctx, it, end);
         }
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
         return error_ctx{file_error};
      }

      return read<set_msgpack<Opts>()>(value, buffer, ctx);
   }
}
