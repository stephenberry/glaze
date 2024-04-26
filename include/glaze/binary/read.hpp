// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/header.hpp"
#include "glaze/binary/skip.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/reflection/reflect.hpp"
#include "glaze/util/dump.hpp"

namespace glz
{
   namespace detail
   {
      template <>
      struct read<binary>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
               if constexpr (Opts.error_on_const_read) {
                  ctx.error = error_code::attempt_const_read;
               }
               else {
                  // do not read anything into the const value
                  skip_value_binary<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
               }
            }
            else {
               using V = std::remove_cvref_t<T>;
               from_binary<V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                                 std::forward<It1>(end));
            }
         }
      };

      template <glaze_value_t T>
      struct from_binary<T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end)
         {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            from_binary<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx),
                                              std::forward<It0>(it), std::forward<It1>(end));
         }
      };

      template <is_bitset T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            const auto tag = uint8_t(*it);

            constexpr uint8_t type = uint8_t(3) << 3;
            constexpr uint8_t header = tag::typed_array | type;

            if (tag != header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it;
            if (it >= end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            const auto num_bytes = (value.size() + 7) / 8;
            for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i, ++it) {
               uint8_t byte;
               std::memcpy(&byte, it, 1);
               for (size_t bit_i = 0; bit_i < 8 && i < n; ++bit_i, ++i) {
                  value[i] = byte >> bit_i & uint8_t(1);
               }
            }
         }
      };

      template <>
      struct from_binary<skip>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
         {
            skip_value_binary<Opts>(ctx, args...);
         }
      };

      template <glaze_flags_t T>
      struct from_binary<T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&&, It0&& it, It1&&)
         {
            constexpr auto N = glz::tuple_size_v<meta_t<T>>;

            constexpr auto Length = byte_length<T>();
            uint8_t data[Length];

            std::memcpy(data, &(*it), Length);
            it += Length;

            for_each<N>([&](auto I) {
               static constexpr auto item = glz::get<I>(meta_v<T>);

               get_member(value, glz::get<1>(item)) = data[I / 8] & (uint8_t{1} << (7 - (I % 8)));
            });
         }
      };

      template <class T>
         requires(num_t<T> || char_t<T> || glaze_enum_t<T>)
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&&) noexcept
         {
            if constexpr (Opts.no_header) {
               using V = std::decay_t<T>;
               std::memcpy(&value, &(*it), sizeof(V));
               it += sizeof(V);
            }
            else {
               constexpr uint8_t type =
                  std::floating_point<T> ? 0 : (std::is_signed_v<T> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t header = tag::number | type | (byte_count<T> << 5);

               const auto tag = uint8_t(*it);
               if (tag != header) {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               using V = std::decay_t<decltype(value)>;
               std::memcpy(&value, &(*it), sizeof(V));
               it += sizeof(V);
            }
         }
      };

      template <class T>
         requires(std::is_enum_v<T> && !glaze_enum_t<T>)
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&&) noexcept
         {
            using V = std::underlying_type_t<std::decay_t<T>>;

            if constexpr (Opts.no_header) {
               std::memcpy(&value, &(*it), sizeof(V));
               it += sizeof(V);
            }
            else {
               constexpr uint8_t type =
                  std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t header = tag::number | type | (byte_count<V> << 5);

               const auto tag = uint8_t(*it);
               if (tag != header) {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               std::memcpy(&value, &(*it), sizeof(V));
               it += sizeof(V);
            }
         }
      };

      template <class T>
         requires complex_t<T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&&) noexcept
         {
            if constexpr (Opts.no_header) {
               using V = std::decay_t<T>;
               std::memcpy(&value, &(*it), sizeof(V));
               it += sizeof(V);
            }
            else {
               constexpr uint8_t header = tag::extensions | 0b00011'000;

               const auto tag = uint8_t(*it);
               if (tag != header) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               using V = typename T::value_type;
               constexpr uint8_t type =
                  std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t complex_number = 0;
               constexpr uint8_t complex_header = complex_number | type | (byte_count<V> << 5);

               const auto complex_tag = uint8_t(*it);
               if (complex_tag != complex_header) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               std::memcpy(&value, &(*it), 2 * sizeof(V));
               it += 2 * sizeof(V);
            }
         }
      };

      template <boolean_like T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& /* end */) noexcept
         {
            const auto tag = uint8_t(*it);
            if ((tag & 0b0000'1111) != tag::boolean) {
               ctx.error = error_code::syntax_error;
               return;
            }

            value = tag >> 4;
            ++it;
         }
      };

      template <is_member_function_pointer T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&& /*ctx*/, auto&& /*it*/,
                                          auto&& /*end*/) noexcept
         {}
      };

      template <func_t T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&& /*ctx*/, auto&& /*it*/,
                                          auto&& /*end*/) noexcept
         {}
      };

      template <class T>
      struct from_binary<basic_raw_json<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            read<binary>::op<Opts>(value.str, ctx, it, end);
         }
      };

      template <class T>
      struct from_binary<basic_text<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            read<binary>::op<Opts>(value.str, ctx, it, end);
         }
      };

      template <is_variant T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            constexpr uint8_t header = tag::extensions | 0b00001'000;
            const auto tag = uint8_t(*it);
            if (tag != header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;

            const auto type_index = int_from_compressed(ctx, it, end);
            if (value.index() != type_index) {
               value = runtime_variant_map<T>()[type_index];
            }
            std::visit([&](auto&& v) { read<binary>::op<Opts>(v, ctx, it, end); }, value);
         }
      };

      template <str_t T>
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = typename std::decay_t<T>::value_type;
            static_assert(sizeof(V) == 1);

            if constexpr (Opts.no_header) {
               const auto n = int_from_compressed(ctx, it, end);
               if ((it + n) > end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               value.resize(n);
               std::memcpy(value.data(), &(*it), n);
               it += n;
            }
            else {
               constexpr uint8_t header = tag::string;

               const auto tag = uint8_t(*it);
               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if ((it + n) > end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               value.resize(n);
               std::memcpy(value.data(), &(*it), n);
               it += n;
            }
         }
      };

      // for set types
      template <class T>
         requires(readable_array_t<T> && !emplace_backable<T> && !resizable<T> && emplaceable<T>)
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = range_value_t<std::decay_t<T>>;

            const auto tag = uint8_t(*it);

            if constexpr (boolean_like<V>) {
               constexpr uint8_t type = uint8_t(3) << 3;
               constexpr uint8_t header = tag::typed_array | type;

               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               const auto n = int_from_compressed(ctx, it, end);

               value.clear();

               const auto num_bytes = (value.size() + 7) / 8;
               for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i, ++it) {
                  uint8_t byte;
                  std::memcpy(&byte, it, 1);
                  for (size_t bit_i = 7; bit_i < 8 && i < n; --bit_i, ++i) {
                     bool x = byte >> bit_i & uint8_t(1);
                     value.emplace(x);
                  }
               }
            }
            else if constexpr (num_t<V>) {
               constexpr uint8_t type =
                  std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t header = tag::typed_array | type | (byte_count<V> << 5);

               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               value.clear();

               for (size_t i = 0; i < n; ++i) {
                  V x;
                  std::memcpy(&x, it, sizeof(V));
                  it += sizeof(V);
                  value.emplace(x);
               }
            }
            else if constexpr (str_t<V>) {
               constexpr uint8_t type = uint8_t(3) << 3;
               constexpr uint8_t string_indicator = uint8_t(1) << 5;
               constexpr uint8_t header = tag::typed_array | type | string_indicator;

               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               value.clear();

               for (size_t i = 0; i < n; ++i) {
                  const auto length = int_from_compressed(ctx, it, end);
                  if ((it + length) > end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }
                  V str;
                  str.resize(length);
                  std::memcpy(str.data(), it, length);
                  it += length;
                  value.emplace(std::move(str));
               }
            }
            else if constexpr (complex_t<V>) {
               static_assert(false_v<T>, "TODO");
            }
            else {
               // generic array
               if ((tag & 0b00000'111) != tag::generic_array) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               value.clear();

               for (size_t i = 0; i < n; ++i) {
                  V v;
                  read<binary>::op<Opts>(v, ctx, it, end);
                  value.emplace(std::move(v));
               }
            }
         }
      };

      template <readable_array_t T>
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = range_value_t<std::decay_t<T>>;

            const auto tag = uint8_t(*it);

            if constexpr (boolean_like<V>) {
               constexpr uint8_t type = uint8_t(3) << 3;
               constexpr uint8_t header = tag::typed_array | type;

               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               if constexpr (resizable<T>) {
                  value.resize(n);

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }

               const auto num_bytes = (value.size() + 7) / 8;
               for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i, ++it) {
                  uint8_t byte;
                  std::memcpy(&byte, it, 1);
                  for (size_t bit_i = 7; bit_i < 8 && i < n; --bit_i, ++i) {
                     value[i] = byte >> bit_i & uint8_t(1);
                  }
               }
            }
            else if constexpr (num_t<V>) {
               constexpr uint8_t type =
                  std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t header = tag::typed_array | type | (byte_count<V> << 5);

               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               if ((it + n * sizeof(V)) > end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (resizable<T>) {
                  value.resize(n);

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }

               if constexpr (contiguous<T>) {
                  std::memcpy(value.data(), it, n * sizeof(V));
                  it += n * sizeof(V);
               }
               else {
                  for (auto&& x : value) {
                     std::memcpy(&x, it, sizeof(V));
                     it += sizeof(V);
                  }
               }
            }
            else if constexpr (str_t<V>) {
               constexpr uint8_t type = uint8_t(3) << 3;
               constexpr uint8_t string_indicator = uint8_t(1) << 5;
               constexpr uint8_t header = tag::typed_array | type | string_indicator;

               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               if constexpr (resizable<T>) {
                  value.resize(n);

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }

               for (auto&& x : value) {
                  const auto length = int_from_compressed(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
                  x.resize(length);

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }

                  std::memcpy(x.data(), it, length);
                  it += length;
               }
            }
            else if constexpr (complex_t<V>) {
               constexpr uint8_t header = tag::extensions | 0b00011'000;
               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               using X = typename V::value_type;
               constexpr uint8_t complex_array = 1;
               constexpr uint8_t type =
                  std::floating_point<X> ? 0 : (std::is_signed_v<X> ? 0b000'01'000 : 0b000'10'000);
               constexpr uint8_t complex_header = complex_array | type | (byte_count<X> << 5);
               const auto complex_tag = uint8_t(*it);
               if (complex_tag != complex_header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               if ((it + n * sizeof(V)) > end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (resizable<T>) {
                  value.resize(n);

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }

               if constexpr (contiguous<T>) {
                  std::memcpy(value.data(), it, n * sizeof(V));
                  it += n * sizeof(V);
               }
               else {
                  for (auto&& x : value) {
                     std::memcpy(&x, it, sizeof(V));
                     it += sizeof(V);
                  }
               }
            }
            else {
               if ((tag & 0b00000'111) != tag::generic_array) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               if constexpr (resizable<T>) {
                  value.resize(n);

                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }

               for (auto&& item : value) {
                  read<binary>::op<Opts>(item, ctx, it, end);
               }
            }
         }
      };

      template <pair_t T>
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using Key = typename T::first_type;

            constexpr uint8_t type = str_t<Key> ? 0 : (std::is_signed_v<Key> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t byte_cnt = str_t<Key> ? 0 : byte_count<Key>;
            constexpr uint8_t header = tag::object | type | (byte_cnt << 5);

            const auto tag = uint8_t(*it);
            if (tag != header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it;

            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (n != 1) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            read<binary>::op<opt_true<Opts, &opts::no_header>>(value.first, ctx, it, end);
            read<binary>::op<Opts>(value.second, ctx, it, end);
         }
      };

      template <readable_map_t T>
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using Key = typename T::key_type;

            constexpr uint8_t type = str_t<Key> ? 0 : (std::is_signed_v<Key> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t byte_cnt = str_t<Key> ? 0 : byte_count<Key>;
            constexpr uint8_t header = tag::object | type | (byte_cnt << 5);

            const auto tag = uint8_t(*it);
            if (tag != header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it;

            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (std::is_arithmetic_v<std::decay_t<Key>>) {
               Key key;
               for (size_t i = 0; i < n; ++i) {
                  read<binary>::op<opt_true<Opts, &opts::no_header>>(key, ctx, it, end);
                  read<binary>::op<Opts>(value[key], ctx, it, end);
               }
            }
            else {
               static thread_local Key key;
               for (size_t i = 0; i < n; ++i) {
                  read<binary>::op<opt_true<Opts, &opts::no_header>>(key, ctx, it, end);
                  read<binary>::op<Opts>(value[key], ctx, it, end);
               }
            }
         }
      };

      template <nullable_t T>
         requires(std::is_array_v<T>)
      struct from_binary<T> final
      {
         template <auto Opts, class V, size_t N>
         GLZ_ALWAYS_INLINE static void op(V (&value)[N], is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            read<binary>::op<Opts>(std::span{value, N}, ctx, it, end);
         }
      };

      template <nullable_t T>
         requires(!std::is_array_v<T>)
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            const auto tag = uint8_t(*it);

            if (tag == tag::null) {
               ++it;
               if constexpr (is_specialization_v<T, std::optional>)
                  value = std::nullopt;
               else if constexpr (is_specialization_v<T, std::unique_ptr>)
                  value = nullptr;
               else if constexpr (is_specialization_v<T, std::shared_ptr>)
                  value = nullptr;
            }
            else {
               if (!value) {
                  if constexpr (is_specialization_v<T, std::optional>)
                     value = std::make_optional<typename T::value_type>();
                  else if constexpr (is_specialization_v<T, std::unique_ptr>)
                     value = std::make_unique<typename T::element_type>();
                  else if constexpr (is_specialization_v<T, std::shared_ptr>)
                     value = std::make_shared<typename T::element_type>();
                  else if constexpr (constructible<T>) {
                     value = meta_construct_v<T>();
                  }
                  else {
                     ctx.error = error_code::invalid_nullable_read;
                     return;
                     // Cannot read into unset nullable that is not std::optional, std::unique_ptr, or std::shared_ptr
                  }
               }
               read<binary>::op<Opts>(*value, ctx, it, end);
            }
         }
      };

      template <is_includer T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (Opts.no_header) {
               skip_compressed_int(ctx, it, end);
            }
            else {
               constexpr uint8_t header = tag::string;

               const auto tag = uint8_t(*it);
               if (tag != header) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               ++it;

               skip_compressed_int(ctx, it, end);
            }
         }
      };

      template <class T>
         requires(glaze_object_t<T> || reflectable<T>)
      struct from_binary<T> final
      {
         template <auto Opts>
            requires(Opts.structs_as_arrays == true)
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (reflectable<T>) {
               auto t = to_tuple(value);
               read<binary>::op<Opts>(t, ctx, it, end);
            }
            else {
               const auto tag = uint8_t(*it);
               if (tag != tag::generic_array) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;

               using V = std::decay_t<T>;
               constexpr auto N = glz::tuple_size_v<meta_t<V>>;
               if (int_from_compressed(ctx, it, end) != N) {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               for_each<N>([&](auto I) {
                  static constexpr auto item = get<I>(meta_v<V>);
                  using T0 = std::decay_t<decltype(get<0>(item))>;
                  constexpr bool use_reflection = std::is_member_pointer_v<T0>;
                  constexpr auto member_index = use_reflection ? 0 : 1;
                  read<binary>::op<Opts>(get_member(value, get<member_index>(item)), ctx, it, end);
               });
            }
         }

         template <auto Opts>
            requires(Opts.structs_as_arrays == false)
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            constexpr uint8_t type = 0; // string key
            constexpr uint8_t header = tag::object | type;

            const auto tag = uint8_t(*it);
            if (tag != header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it;

            constexpr auto N = reflection_count<T>;

            const auto n_keys = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            decltype(auto) storage = [&]() -> decltype(auto) {
               if constexpr (reflectable<T>) {
#if ((defined _MSC_VER) && (!defined __clang__))
                  static thread_local auto cmap = make_map<T, Opts.use_hash_comparison>();
#else
                  static thread_local constinit auto cmap = make_map<T, Opts.use_hash_comparison>();
#endif
                  populate_map(value, cmap); // Function required for MSVC to build
                  return cmap;
               }
               else {
                  static constexpr auto cmap = make_map<T, Opts.use_hash_comparison>();
                  return cmap;
               }
            }();

            for (size_t i = 0; i < n_keys; ++i) {
               const auto length = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               const std::string_view key{it, length};

               it += length;

               if constexpr (N > 0) {
                  if (const auto& p = storage.find(key); p != storage.end()) [[likely]] {
                     std::visit(
                        [&](auto&& member_ptr) { read<binary>::op<Opts>(get_member(value, member_ptr), ctx, it, end); },
                        p->second);

                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                  }
                  else [[unlikely]] {
                     if constexpr (Opts.error_on_unknown_keys) {
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                     else {
                        skip_value_binary<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
               }
               else if constexpr (Opts.error_on_unknown_keys) {
                  ctx.error = error_code::unknown_key;
                  return;
               }
               else {
                  skip_value_binary<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]]
                     return;
               }
            }
         }
      };

      template <class T>
         requires glaze_array_t<T>
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            const auto tag = uint8_t(*it);
            if (tag != tag::generic_array) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;

            using V = std::decay_t<T>;
            constexpr auto N = glz::tuple_size_v<meta_t<V>>;
            if (int_from_compressed(ctx, it, end) != N) {
               ctx.error = error_code::syntax_error;
               return;
            }

            for_each<N>(
               [&](auto I) { read<binary>::op<Opts>(get_member(value, glz::get<I>(meta_v<V>)), ctx, it, end); });
         }
      };

      template <class T>
         requires(tuple_t<T> || is_std_tuple<T>)
      struct from_binary<T> final
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            const auto tag = uint8_t(*it);
            if (tag != tag::generic_array) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;

            using V = std::decay_t<T>;
            constexpr auto N = glz::tuple_size_v<V>;
            if (int_from_compressed(ctx, it, end) != N) {
               ctx.error = error_code::syntax_error;
               return;
            }

            if constexpr (is_std_tuple<T>) {
               for_each<N>([&](auto I) { read<binary>::op<Opts>(std::get<I>(value), ctx, it, end); });
            }
            else {
               for_each<N>([&](auto I) { read<binary>::op<Opts>(glz::get<I>(value), ctx, it, end); });
            }
         }
      };

      template <filesystem_path T>
      struct from_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            static thread_local std::string buffer{};
            read<binary>::op<Opts>(buffer, ctx, args...);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            value = buffer;
         }
      };
   }

   template <read_binary_supported T, class Buffer>
   [[nodiscard]] inline parse_error read_binary(T&& value, Buffer&& buffer) noexcept
   {
      return read<opts{.format = binary}>(value, std::forward<Buffer>(buffer));
   }

   template <read_binary_supported T, class Buffer>
   [[nodiscard]] inline expected<T, parse_error> read_binary(Buffer&& buffer) noexcept
   {
      T value{};
      const auto pe = read<opts{.format = binary}>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
      }
      return value;
   }

   template <opts Opts = opts{}, read_binary_supported T>
   [[nodiscard]] inline parse_error read_file_binary(T& value, const sv file_name, auto&& buffer) noexcept
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto file_error = file_to_buffer(buffer, ctx.current_file);

      if (bool(file_error)) [[unlikely]] {
         return parse_error{file_error};
      }

      return read<set_binary<Opts>()>(value, buffer, ctx);
   }

   template <read_binary_supported T, class Buffer>
   [[nodiscard]] inline parse_error read_binary_untagged(T&& value, Buffer&& buffer) noexcept
   {
      return read<opts{.format = binary, .structs_as_arrays = true}>(std::forward<T>(value),
                                                                     std::forward<Buffer>(buffer));
   }

   template <read_binary_supported T, class Buffer>
   [[nodiscard]] inline expected<T, parse_error> read_binary_untagged(Buffer&& buffer) noexcept
   {
      T value{};
      const auto pe = read<opts{.format = binary, .structs_as_arrays = true}>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
      }
      return value;
   }

   template <opts Opts = opts{}, read_binary_supported T>
   [[nodiscard]] inline parse_error read_file_binary_untagged(T& value, const std::string& file_name,
                                                              auto&& buffer) noexcept
   {
      return read_file_binary<opt_true<Opts, &opts::structs_as_arrays>>(value, file_name, buffer);
   }
}
