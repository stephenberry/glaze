// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/beve/header.hpp"
#include "glaze/beve/key_traits.hpp"
#include "glaze/beve/skip.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/reflect.hpp"
#include "glaze/file/file_ops.hpp"
#include "glaze/util/dump.hpp"

// To handle invalid inputs we must check if (it >= end) at the beginning of each function
// This way we can always call a function after incrementing the iterator without needed to do a tail check
// If we know the first function called has an end check, we don't need a guard at the top of the function
// Also, after almost every function call we need to check if an error was produced

namespace glz
{
   template <>
   struct parse<BEVE>
   {
      template <auto Opts, class T, class Tag, is_context Ctx, class It0, class It1>
         requires(check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(T&& value, Tag&& tag, Ctx&& ctx, It0&& it, It1 end)
      {
         if constexpr (const_value_v<T>) {
            if constexpr (check_error_on_const_read(Opts)) {
               ctx.error = error_code::attempt_const_read;
            }
            else {
               // do not read anything into the const value
               skip_value<BEVE>::op<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), end);
            }
         }
         else {
            using V = std::remove_cvref_t<T>;
            from<BEVE, V>::template op<Opts>(std::forward<T>(value), std::forward<Tag>(tag), std::forward<Ctx>(ctx),
                                             std::forward<It0>(it), end);
         }
      }

      template <auto Opts, class T, is_context Ctx, class It0, class It1>
         requires(not check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         if constexpr (const_value_v<T>) {
            if constexpr (check_error_on_const_read(Opts)) {
               ctx.error = error_code::attempt_const_read;
            }
            else {
               // do not read anything into the const value
               skip_value<BEVE>::op<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), end);
            }
         }
         else {
            using V = std::remove_cvref_t<T>;
            from<BEVE, V>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                             end);
         }
      }
   };

   template <class T>
      requires(glaze_value_t<T> && !custom_read<T>)
   struct from<BEVE, T>
   {
      template <auto Opts, class Value, is_context Ctx, class It0, class It1>
      GLZ_ALWAYS_INLINE static void op(Value&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         from<BEVE, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                          std::forward<Ctx>(ctx), std::forward<It0>(it), end);
      }

      template <auto Opts, class Value, class Tag, is_context Ctx, class It0, class It1>
         requires(check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(Value&& value, Tag&& tag, Ctx&& ctx, It0&& it, It1 end)
      {
         using V = std::decay_t<decltype(get_member(std::declval<Value>(), meta_wrapper_v<T>))>;
         from<BEVE, V>::template op<Opts>(get_member(std::forward<Value>(value), meta_wrapper_v<T>),
                                          std::forward<Tag>(tag), std::forward<Ctx>(ctx), std::forward<It0>(it), end);
      }
   };

   template <always_null_t T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         if (uint8_t(*it)) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         ++it;
      }
   };

   template <>
   struct from<BEVE, hidden>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&...) noexcept
      {
         ctx.error = error_code::attempt_read_hidden;
      }
   };

   template <is_bitset T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);

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

         const auto num_bytes = (value.size() + 7) / 8;
         for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i, ++it) {
            if (invalid_end(ctx, it, end)) {
               return;
            }
            uint8_t byte;
            std::memcpy(&byte, it, 1);
            for (size_t bit_i = 0; bit_i < 8 && i < n; ++bit_i, ++i) {
               value[i] = byte >> bit_i & uint8_t(1);
            }
         }
      }
   };

   template <>
   struct from<BEVE, skip>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&&... args) noexcept
      {
         skip_value<BEVE>::op<Opts>(ctx, args...);
      }
   };

   template <glaze_flags_t T>
   struct from<BEVE, T>
   {
      template <auto Opts, is_context Ctx, class It0, class It1>
      static void op(auto&& value, Ctx&& ctx, It0&& it, It1 end)
      {
         constexpr auto N = reflect<T>::size;

         constexpr auto Length = byte_length<T>();
         uint8_t data[Length];

         if ((it + Length) > end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         std::memcpy(data, it, Length);
         it += Length;

         for_each<N>([&]<size_t I>() {
            get_member(value, get<I>(reflect<T>::values)) = data[I / 8] & (uint8_t{1} << (7 - (I % 8)));
         });
      }
   };

   template <class T>
      requires(num_t<T> || char_t<T> || glaze_enum_t<T>)
   struct from<BEVE, T>
   {
      static constexpr uint8_t type = std::floating_point<T> ? 0 : (std::is_signed_v<T> ? 0b000'01'000 : 0b000'10'000);
      static constexpr uint8_t header = tag::number | type | (byte_count<T> << 5);

      template <auto Opts>
         requires(check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(auto&& value, const uint8_t tag, is_context auto&& ctx, auto&& it,
                                       auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }

         using V = std::decay_t<decltype(value)>;

         constexpr auto is_volatile = std::is_volatile_v<std::remove_reference_t<decltype(value)>>;

         if (tag != header) {
            if constexpr (check_allow_conversions(Opts)) {
               if constexpr (num_t<T>) {
                  if ((tag & 0b00000111) != tag::number) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }

                  auto decode = [&](auto&& i) {
                     if ((it + sizeof(i)) > end) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }
                     using I = std::decay_t<decltype(i)>;
                     std::memcpy(&i, it, sizeof(I));
                     if constexpr (std::endian::native == std::endian::big && sizeof(I) > 1) {
                        byteswap_le(i);
                     }
                     value = static_cast<V>(i);
                     it += sizeof(i);
                  };

                  switch (tag) {
                  case tag::f32: {
                     if constexpr (std::integral<V>) {
                        // We do not allow cross conversions from floats to integral types
                        ctx.error = error_code::syntax_error;
                     }
                     else {
                        static_assert(sizeof(float) == 4);
                        // TODO: use float32_t in C++23
                        decode(float{});
                     }
                     return;
                  }
                  case tag::f64: {
                     if constexpr (std::integral<V>) {
                        // We do not allow cross conversions from floats to integral types
                        ctx.error = error_code::syntax_error;
                     }
                     else {
                        static_assert(sizeof(double) == 8);
                        // TODO: use float64_t in C++23
                        decode(double{});
                     }
                     return;
                  }
                  case tag::i8: {
                     decode(int8_t{});
                     return;
                  }
                  case tag::i16: {
                     decode(int16_t{});
                     return;
                  }
                  case tag::i32: {
                     decode(int32_t{});
                     return;
                  }
                  case tag::i64: {
                     decode(int64_t{});
                     return;
                  }
                  case tag::u8: {
                     decode(uint8_t{});
                     return;
                  }
                  case tag::u16: {
                     decode(uint16_t{});
                     return;
                  }
                  case tag::u32: {
                     decode(uint32_t{});
                     return;
                  }
                  case tag::u64: {
                     decode(uint64_t{});
                     return;
                  }
                  default: {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  }
               }
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         if ((it + sizeof(V)) > end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         if constexpr (is_volatile) {
            V temp;
            std::memcpy(&temp, it, sizeof(V));
            if constexpr (std::endian::native == std::endian::big && sizeof(V) > 1) {
               byteswap_le(temp);
            }
            value = temp;
         }
         else {
            std::memcpy(&value, it, sizeof(V));
            if constexpr (std::endian::native == std::endian::big && sizeof(V) > 1) {
               byteswap_le(value);
            }
         }
         it += sizeof(V);
      }

      template <auto Opts>
         requires(not check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);
         ++it;
         op<no_header_on<Opts>()>(value, tag, ctx, it, end);
      }
   };

   template <class T>
      requires(std::is_enum_v<T> && !glaze_enum_t<T>)
   struct from<BEVE, T>
   {
      // Tagged overload: the type tag has already been read and is supplied by the caller
      // (used by the typed-array conversion paths). Delegate to the underlying integer's
      // reader so enums reuse the same numeric decoding and conversion handling, then cast
      // back. This makes std::byte (a byte-valued enum) readable as a u8 typed-array element.
      template <auto Opts, class Value, class Tag, is_context Ctx, class It0, class It1>
         requires(check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(Value&& value, Tag&& tag, Ctx&& ctx, It0&& it, It1 end) noexcept
      {
         using U = std::underlying_type_t<std::decay_t<T>>;
         U underlying{};
         from<BEVE, U>::template op<Opts>(underlying, std::forward<Tag>(tag), std::forward<Ctx>(ctx),
                                          std::forward<It0>(it), end);
         value = static_cast<std::decay_t<T>>(underlying);
      }

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         using V = std::underlying_type_t<std::decay_t<T>>;

         if constexpr (check_no_header(Opts)) {
            if ((it + sizeof(V)) > end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if constexpr (std::endian::native == std::endian::big && sizeof(V) > 1) {
               V temp;
               std::memcpy(&temp, it, sizeof(V));
               byteswap_le(temp);
               value = static_cast<std::decay_t<T>>(temp);
            }
            else {
               std::memcpy(&value, it, sizeof(V));
            }
            it += sizeof(V);
         }
         else {
            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t header = tag::number | type | (byte_count<V> << 5);

            if (invalid_end(ctx, it, end)) {
               return;
            }
            const auto tag = uint8_t(*it);
            if (tag != header) {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it;
            if ((it + sizeof(V)) > end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if constexpr (std::endian::native == std::endian::big && sizeof(V) > 1) {
               V temp;
               std::memcpy(&temp, it, sizeof(V));
               byteswap_le(temp);
               value = static_cast<std::decay_t<T>>(temp);
            }
            else {
               std::memcpy(&value, it, sizeof(V));
            }
            it += sizeof(V);
         }
      }
   };

   template <class T>
      requires complex_t<T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if constexpr (check_no_header(Opts)) {
            using V = std::decay_t<T>;
            if ((it + sizeof(V)) > end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if constexpr (std::endian::native == std::endian::big) {
               using X = typename V::value_type;
               X real_part, imag_part;
               std::memcpy(&real_part, it, sizeof(X));
               it += sizeof(X);
               std::memcpy(&imag_part, it, sizeof(X));
               it += sizeof(X);
               byteswap_le(real_part);
               byteswap_le(imag_part);
               value = V{real_part, imag_part};
            }
            else {
               std::memcpy(&value, it, sizeof(V));
               it += sizeof(V);
            }
         }
         else {
            constexpr uint8_t header = tag::extensions | 0b00011'000;

            if (invalid_end(ctx, it, end)) {
               return;
            }
            const auto tag = uint8_t(*it);
            if (tag != header) {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;

            using V = typename T::value_type;
            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t complex_number = 0;
            constexpr uint8_t complex_header = complex_number | type | (byte_count<V> << 5);

            if (invalid_end(ctx, it, end)) {
               return;
            }
            const auto complex_tag = uint8_t(*it);
            if (complex_tag != complex_header) {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;

            if ((it + 2 * sizeof(V)) > end) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            if constexpr (std::endian::native == std::endian::big) {
               V real_part, imag_part;
               std::memcpy(&real_part, it, sizeof(V));
               it += sizeof(V);
               std::memcpy(&imag_part, it, sizeof(V));
               it += sizeof(V);
               byteswap_le(real_part);
               byteswap_le(imag_part);
               value = std::decay_t<T>{real_part, imag_part};
            }
            else {
               std::memcpy(&value, it, 2 * sizeof(V));
               it += 2 * sizeof(V);
            }
         }
      }
   };

   template <boolean_like T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
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
   struct from<BEVE, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&& /*ctx*/, auto&& /*it*/,
                                       auto&& /*end*/) noexcept
      {}
   };

   template <func_t T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         skip_string_beve(ctx, it, end);
      }
   };

   template <class T>
   struct from<BEVE, basic_raw_json<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         parse<BEVE>::op<Opts>(value.str, ctx, it, end);
      }
   };

   template <class T>
   struct from<BEVE, basic_text<T>>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         parse<BEVE>::op<Opts>(value.str, ctx, it, end);
      }
   };

   // A variant alternative whose BEVE wire form is an object (used to constrain V2 object deduction).
   template <class V>
   inline constexpr bool beve_variant_is_object_alt = [] {
      using X = std::decay_t<V>;
      return glaze_object_t<X> || reflectable<X> || is_memory_object<X> || readable_map_t<X> || pair_t<X>;
   }();

   // True when X is a reflectable struct with no members. reflect<X>::size may only be named for
   // struct-like X, so the check is guarded here at namespace scope (naming reflect<X>::size from a
   // nested lambda inside the variant reader would force a capture of the enclosing constexpr flags
   // that GCC rejects).
   template <class X>
   inline constexpr bool beve_is_empty_struct = [] {
      if constexpr (glaze_object_t<X> || reflectable<X>) {
         return reflect<X>::size == 0;
      }
      else {
         return false;
      }
   }();

   // BEVE Version 2 recovers a std::variant alternative from an ordinary, self-describing value.
   // Legacy Version 1 data still begins with the type-tag extension byte (0x0E) and is decoded by
   // positional index. Otherwise the alternative is deduced from the value's category; for objects
   // it is resolved from the discriminator (tag_v/ids_v) when present, falling back to the set of
   // keys (structural deduction), exactly as the JSON reader does. Positional structs-as-arrays data
   // carries neither keys nor a tag, so each alternative is tried in turn. Every non-object category
   // is likewise resolved by trying alternatives, which is reliable because each BEVE value reader
   // validates its own type-tag byte and a mismatch fails cleanly.
   template <is_variant T>
      requires(not custom_read<T>)
   struct from<BEVE, T>
   {
      static constexpr size_t variant_size = std::variant_size_v<T>;

      // Try each alternative in declaration order, rewinding the iterator on failure. Returns true
      // when an alternative parsed cleanly, leaving `value` holding it.
      template <auto Opts>
      static bool try_each_pass(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         bool matched = false;
         const auto start = it;
         for_each<variant_size>([&]<size_t I>() {
            if (matched) {
               return;
            }
            it = start;
            if (value.index() != I) {
               emplace_runtime_variant(value, I);
            }
            ctx.error = error_code::none;
            parse<BEVE>::op<Opts>(std::get<I>(value), ctx, it, end);
            if (!bool(ctx.error)) {
               matched = true;
            }
         });
         if (!matched) {
            it = start;
         }
         return matched;
      }

      // Resolve a non-object alternative from the value's own self-describing type header.
      //
      // The first pass runs with allow_conversions off so an alternative is accepted only when its
      // BEVE type header matches exactly. This is required for correctness, not just fidelity: the
      // numeric and typed-array readers deliberately widen and narrow under allow_conversions (the
      // default), so a lenient first pass would let variant<int32_t, int64_t> capture an i64 value
      // in its int32_t alternative and silently truncate it. Only if nothing matches exactly do we
      // retry leniently, which keeps reads of data whose numeric widths no longer match any
      // alternative working.
      template <auto Opts>
      static void try_each(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         const auto start = it;
         if constexpr (check_allow_conversions(Opts)) {
            if (try_each_pass<opt_false<Opts, allow_conversions_opt_tag{}>>(value, ctx, it, end)) {
               return;
            }
         }
         if (try_each_pass<Opts>(value, ctx, it, end)) {
            return;
         }
         it = start;
         ctx.error = error_code::no_matching_variant_type;
      }

      // Object-category dispatch: single candidate is parsed directly; otherwise scan the keys once
      // to find the discriminator and/or narrow the candidate set, then rewind and parse in full.
      template <auto Opts>
      static void read_object(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         constexpr size_t n_obj = []<size_t... I>(std::index_sequence<I...>) {
            return (size_t(beve_variant_is_object_alt<std::variant_alternative_t<I, T>>) + ... + 0);
         }(std::make_index_sequence<variant_size>{});

         if constexpr (n_obj == 0) {
            ctx.error = error_code::no_matching_variant_type;
            return;
         }
         else if constexpr (n_obj == 1 && tag_v<T>.empty()) {
            constexpr size_t I = [] {
               size_t r = variant_size;
               [&]<size_t... J>(std::index_sequence<J...>) {
                  ((beve_variant_is_object_alt<std::variant_alternative_t<J, T>> && r == variant_size
                       ? (void)(r = J)
                       : (void)0),
                   ...);
               }(std::make_index_sequence<variant_size>{});
               return r;
            }();
            if (value.index() != I) {
               emplace_runtime_variant(value, I);
            }
            parse<BEVE>::op<Opts>(std::get<I>(value), ctx, it, end);
         }
         else {
            constexpr bool tagged = not tag_v<T>.empty();

            // Pass 1: scan keys on a separate cursor without disturbing `it`.
            auto scan = it;
            ++scan; // object tag byte
            const auto n_keys = int_from_compressed(ctx, scan, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            auto possible = bit_array<variant_size>{};
            for_each<variant_size>([&]<size_t I>() {
               if constexpr (beve_variant_is_object_alt<std::variant_alternative_t<I, T>>) {
                  possible[I] = true;
               }
            });

            // `tag_index` is whatever variant_id_to_index reported, which is ids_v<T>.size() when the
            // id is unknown; `tag_decoded` records that an id was actually read, so an unknown id can
            // be rejected instead of silently falling through to structural deduction.
            size_t tag_index = ids_v<T>.size();
            bool tag_decoded = false;

            for (size_t k = 0; k < n_keys; ++k) {
               const auto klen = int_from_compressed(ctx, scan, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               if (uint64_t(end - scan) < klen) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               const sv key{scan, size_t(klen)};
               scan += klen;

               if constexpr (tagged) {
                  if (key == tag_v<T>) {
                     using id_type = std::decay_t<decltype(ids_v<T>[0])>;
                     if constexpr (std::integral<id_type>) {
                        id_type id{};
                        from<BEVE, id_type>::template op<Opts>(id, ctx, scan, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        tag_index = variant_id_to_index<T>::op(id);
                        tag_decoded = true;
                     }
                     else {
                        // Read the string discriminator VALUE in place: [tag::string][len][bytes].
                        if (invalid_end(ctx, scan, end)) {
                           return;
                        }
                        if (uint8_t(*scan) != tag::string) [[unlikely]] {
                           // Discriminator value is not a plain string; leave it unresolved and let
                           // structural deduction decide, but still consume the value.
                           skip_value<BEVE>::op<Opts>(ctx, scan, end);
                           if (bool(ctx.error)) [[unlikely]] {
                              return;
                           }
                           continue;
                        }
                        ++scan;
                        const auto slen = int_from_compressed(ctx, scan, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        if (uint64_t(end - scan) < slen) [[unlikely]] {
                           ctx.error = error_code::unexpected_end;
                           return;
                        }
                        const sv id_view{scan, size_t(slen)};
                        scan += slen;
                        tag_index = variant_id_to_index<T>::op(id_view.data(), id_view.data() + id_view.size(),
                                                               id_view.size());
                        tag_decoded = true;
                     }
                     // The discriminator is authoritative, so the remaining keys carry no further
                     // information; stop scanning and let pass 2 read the object in full.
                     break;
                  }
               }

               if constexpr (variant_deduction_key_count<T> > 0) {
                  using dk = keys_wrapper<variant_deduction_keys<T>>;
                  static constexpr auto& H = hash_info<dk>;
                  const auto di =
                     decode_hash_with_size<JSON, dk, H, H.type>::op(key.data(), key.data() + klen, size_t(klen));
                  if (di < variant_deduction_key_count<T> && variant_deduction_keys<T>[di] == key) {
                     possible = possible & variant_deduction_bits<T>[di];
                  }
               }
               skip_value<BEVE>::op<Opts>(ctx, scan, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
            }

            // Resolve: an explicit discriminator wins; otherwise use the narrowed candidate set.
            size_t resolved = variant_size;
            if constexpr (tagged) {
               if (tag_decoded) {
                  if (tag_index < ids_v<T>.size()) [[likely]] {
                     resolved = tag_index;
                  }
                  else if constexpr (ids_v<T>.size() < variant_size) {
                     // Fewer ids than alternatives: the first unlabeled alternative is the default
                     // for an unrecognized id, matching the JSON reader.
                     resolved = ids_v<T>.size();
                  }
                  else {
                     // An id was present but names no alternative. Deducing from the keys instead
                     // would silently produce a different type than the sender labeled, so reject it
                     // exactly as the JSON reader does.
                     ctx.error = error_code::no_matching_variant_type;
                     ctx.custom_error_message = variant_ids_string_v<T>;
                     return;
                  }
               }
            }
            if (resolved >= variant_size) {
               const auto pc = possible.popcount();
               if (pc == 0) [[unlikely]] {
                  ctx.error = error_code::no_matching_variant_type;
                  return;
               }
               else if (pc == 1) {
                  resolved = size_t(possible.countr_zero());
               }
               else {
                  // Ambiguous: prefer the alternative with the fewest declared fields (JSON parity).
                  // The first still-possible alternative is the baseline so a winner is always chosen
                  // even when candidates are non-struct object types (maps/pairs) with no field count.
                  size_t best = variant_size;
                  size_t best_fields = (std::numeric_limits<size_t>::max)();
                  for_each<variant_size>([&]<size_t I>() {
                     if (possible[I]) {
                        using V = std::variant_alternative_t<I, T>;
                        using X = std::conditional_t<is_memory_object<V>, memory_type<V>, V>;
                        size_t f = (std::numeric_limits<size_t>::max)();
                        if constexpr (glaze_object_t<X> || reflectable<X>) {
                           f = reflect<X>::size;
                        }
                        if (best == variant_size || f < best_fields) {
                           best_fields = f;
                           best = I;
                        }
                     }
                  });
                  resolved = best;
               }
            }

            // Pass 2: parse the whole object (from the untouched `it`) as the resolved alternative.
            if (value.index() != resolved) {
               emplace_runtime_variant(value, resolved);
            }
            visit<variant_size>(
               [&]<size_t I>() {
                  using V = std::variant_alternative_t<I, T>;
                  using X = std::conditional_t<is_memory_object<V>, memory_type<V>, V>;

                  // reflect<X>::size may only be named for struct-like X (a std::pair/map alternative
                  // has no reflect specialization); beve_is_empty_struct<X> guards that at namespace
                  // scope. Both flags are constant-expression uses here, so no lambda capture occurs.
                  constexpr bool struct_like = glaze_object_t<X> || reflectable<X>;
                  constexpr bool empty_tagged_struct = tagged && beve_is_empty_struct<X>;

                  if constexpr (empty_tagged_struct) {
                     // Empty struct carrying only the discriminator: skip the whole {tag:id} object.
                     skip_value<BEVE>::op<Opts>(ctx, it, end);
                  }
                  else if constexpr (tagged && struct_like && (not is_memory_object<V>)) {
                     // Thread the discriminator key into the object reader so it is skipped without
                     // disabling unknown-key checking for the alternative's real fields (JSON parity).
                     // When the alternative genuinely declares a member of that name the reader keeps
                     // treating it as a member instead (see from<BEVE, T>::op's Tag handling), so the
                     // member still receives its value.
                     static constexpr auto tag_literal = string_literal_from_view<tag_v<T>.size()>(tag_v<T>);
                     from<BEVE, X>::template op<Opts, tag_literal>(std::get<I>(value), ctx, it, end);
                  }
                  else if constexpr (tagged && (requires { Opts.error_on_unknown_keys; })) {
                     // memory_object / map / pair alternative: tolerate the discriminator key here.
                     static constexpr auto AltOpts = [] {
                        auto o = Opts;
                        o.error_on_unknown_keys = false;
                        return o;
                     }();
                     parse<BEVE>::op<AltOpts>(std::get<I>(value), ctx, it, end);
                  }
                  else {
                     parse<BEVE>::op<Opts>(std::get<I>(value), ctx, it, end);
                  }
               },
               resolved);
         }
      }

      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         const uint8_t header = uint8_t(*it);

         // (A) Legacy Version 1 type-tag extension (0x0E): decode by positional index. Retained for
         // backward compatibility; no Version 2 value begins with this byte.
         if (header == (tag::extensions | 0b00001'000)) {
            ++it;
            const auto type_index = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (type_index >= variant_size) [[unlikely]] {
               ctx.error = error_code::no_matching_variant_type;
               return;
            }
            if (value.index() != type_index) {
               emplace_runtime_variant(value, type_index);
            }
            std::visit([&](auto&& v) { parse<BEVE>::op<Opts>(v, ctx, it, end); }, value);
            return;
         }

         // (B) Positional (structs-as-arrays) data carries no keys or discriminator.
         // This must be an if/else: `if constexpr (...) { return; }` does not discard the statements
         // that follow it, so without the `else` the (C) block below would still be instantiated in
         // positional mode, where read_object's call into the keyed-object reader does not compile.
         if constexpr (check_structs_as_arrays(Opts)) {
            try_each<Opts>(value, ctx, it, end);
         }
         else {
            // (C) Version 2: deduce from the value's self-describing category. Only a string-keyed
            // object (header == tag::object exactly) carries the struct field names / string
            // discriminator that read_object's key scan understands. A numeric-keyed object (a map or
            // pair with a non-string key) writes its keys as raw fixed-width numbers with no length
            // prefix, so it cannot be scanned as string keys; resolve it (and every non-object
            // category) by trying each alternative, whose reader validates its own full header.
            if (header == tag::object) {
               read_object<Opts>(value, ctx, it, end);
            }
            else {
               try_each<Opts>(value, ctx, it, end);
            }
         }
      }
   };

   template <str_t T>
   struct from<BEVE, T> final
   {
      using V = typename std::decay_t<T>::value_type;
      static_assert(sizeof(V) == 1);

      // Stores n decoded bytes (already length-validated by the caller) into the target.
      // Handles resizable strings, string views, and fixed-size std::array<char, N> uniformly
      // so the tagged and untagged code paths share identical storage semantics.
      GLZ_ALWAYS_INLINE static void store(auto&& value, is_context auto&& ctx, auto&& it, const size_t n)
      {
         if constexpr (string_view_t<T>) {
            value = {it, n};
         }
         else if constexpr (array_char_t<T>) {
            // Fixed-size std::array<char, N> cannot be resized, so the decoded payload must
            // fit within it. Any trailing bytes are zero-filled to keep the buffer
            // deterministic when a shorter payload is read into a larger array.
            if (n > value.size()) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            std::memcpy(value.data(), it, n);
            if (n < value.size()) {
               std::memset(value.data() + n, 0, value.size() - n);
            }
         }
         else {
            value.resize(n);
            std::memcpy(value.data(), it, n);
         }
      }

      template <auto Opts>
         requires(check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(auto&& value, const uint8_t, is_context auto&& ctx, auto&& it, auto end)
      {
         const auto n = int_from_compressed(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         if (uint64_t(end - it) < n) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         if constexpr (check_max_string_length(Opts) > 0) {
            if (n > check_max_string_length(Opts)) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
         }
         if constexpr (has_runtime_max_string_length<std::decay_t<decltype(ctx)>>) {
            if (ctx.max_string_length > 0 && n > ctx.max_string_length) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
         }
         store(value, ctx, it, n);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         it += n;
      }

      template <auto Opts>
         requires(not check_no_header(Opts))
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         constexpr uint8_t header = tag::string;

         if (invalid_end(ctx, it, end)) {
            return;
         }
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
         if (uint64_t(end - it) < n) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }
         if constexpr (check_max_string_length(Opts) > 0) {
            if (n > check_max_string_length(Opts)) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
         }
         if constexpr (has_runtime_max_string_length<std::decay_t<decltype(ctx)>>) {
            if (ctx.max_string_length > 0 && n > ctx.max_string_length) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
         }

         store(value, ctx, it, n);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         it += n;
      }
   };

   // for set types
   template <class T>
      requires(readable_array_t<T> && !emplace_backable<T> && !resizable<T> && emplaceable<T>)
   struct from<BEVE, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         using V = range_value_t<std::decay_t<T>>;

         if (invalid_end(ctx, it, end)) {
            return;
         }
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

            value.clear();

            const auto num_bytes = (n + 7) / 8;
            for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i, ++it) {
               if (invalid_end(ctx, it, end)) {
                  return;
               }
               uint8_t byte;
               std::memcpy(&byte, it, 1);
               for (size_t bit_i = 0; bit_i < 8 && i < n; ++bit_i, ++i) {
                  bool x = byte >> bit_i & uint8_t(1);
                  value.emplace(x);
               }
            }
         }
         else if constexpr (beve_num_t<V>) {
            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
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
               if ((it + sizeof(V)) > end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               V x;
               std::memcpy(&x, it, sizeof(V));
               if constexpr (std::endian::native == std::endian::big) {
                  byteswap_le(x);
               }
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
            if (uint64_t(end - it) < n) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            value.clear();

            for (size_t i = 0; i < n; ++i) {
               const auto length = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               if (uint64_t(end - it) < length) [[unlikely]] {
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
               parse<BEVE>::op<Opts>(v, ctx, it, end);
               value.emplace(std::move(v));
            }
         }
      }
   };

   // Zero-copy specialization for std::span<const T> with numeric typed arrays.
   // Instead of copying data, the span is set to point directly into the BEVE buffer.
   // The buffer must outlive the span.
   // For multi-byte types: requires an aligned typed array and little-endian host.
   // For single-byte types: accepts standard typed arrays on any endianness.
   template <class T, size_t Extent>
      requires(std::is_const_v<T> && num_t<std::remove_const_t<T>>)
   struct from<BEVE, std::span<T, Extent>> final
   {
      using V = std::remove_const_t<T>;

      template <auto Opts>
      static void op(std::span<T, Extent>& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);

         if constexpr (sizeof(V) == 1) {
            // Single-byte types: accept standard typed arrays (no alignment or endian concerns)
            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t expected_header = tag::typed_array | type | (byte_count<V> << 5);
            if (tag != expected_header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;

            const size_t n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (Extent != std::dynamic_extent) {
               if (n != Extent) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            if (uint64_t(end - it) < n) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            value = std::span<T, Extent>{reinterpret_cast<const V*>(&(*it)), n};
            it += n;
         }
         else {
            // Multi-byte types: require aligned typed array and little-endian
            if constexpr (std::endian::native != std::endian::little) {
               ctx.error = error_code::feature_not_supported;
               return;
            }

            if (tag != tag::aligned_typed_array) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it; // skip aligned header
            if (invalid_end(ctx, it, end)) {
               return;
            }
            const auto numeric_tag = uint8_t(*it);
            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t expected_header = tag::typed_array | type | (byte_count<V> << 5);
            if (numeric_tag != expected_header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it; // skip numeric header

            const size_t n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (Extent != std::dynamic_extent) {
               if (n != Extent) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            // Read padding length byte and skip padding
            if (invalid_end(ctx, it, end)) {
               return;
            }
            const uint8_t padding = uint8_t(*it);
            ++it;
            if (padding >= sizeof(V)) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            if (typed_array_out_of_bounds(ctx, it, end, n, sizeof(V), padding)) return;
            it += padding;

            value = std::span<T, Extent>{reinterpret_cast<const V*>(&(*it)), n};
            it += n * sizeof(V);
         }
      }
   };

   template <readable_array_t T>
   struct from<BEVE, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         using V = range_value_t<std::decay_t<T>>;

         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);

         if constexpr (boolean_like<V>) {
            constexpr uint8_t type = uint8_t(3) << 3;
            constexpr uint8_t header = tag::typed_array | type;

            if (tag != header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }

            ++it;
            std::conditional_t<Opts.partial_read, size_t, const size_t> n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (Opts.partial_read) {
               n = value.size();
            }

            const auto num_bytes = (n + 7) / 8;
            if (uint64_t(end - it) < num_bytes) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
            if constexpr (check_max_array_size(Opts) > 0) {
               if (n > check_max_array_size(Opts)) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
               if (ctx.max_array_size > 0 && n > ctx.max_array_size) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }

            if constexpr (resizable<T>) {
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts)) {
                  value.shrink_to_fit();
               }
            }
            else {
               if (n > value.size()) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            for (size_t byte_i{}, i{}; byte_i < num_bytes; ++byte_i, ++it) {
               if (invalid_end(ctx, it, end)) {
                  return;
               }
               uint8_t byte;
               std::memcpy(&byte, it, 1);
               for (size_t bit_i = 0; bit_i < 8 && i < n; ++bit_i, ++i) {
                  value[i] = byte >> bit_i & uint8_t(1);
               }
            }
         }
         else if constexpr (beve_num_t<V>) {
            constexpr uint8_t type = std::floating_point<V> ? 0 : (std::is_signed_v<V> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t header = tag::typed_array | type | (byte_count<V> << 5);

            auto validate_and_resize = [&](size_t n) -> bool {
               if constexpr (check_max_array_size(Opts) > 0) {
                  if (n > check_max_array_size(Opts)) [[unlikely]] {
                     ctx.error = error_code::invalid_length;
                     return false;
                  }
               }
               if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
                  if (ctx.max_array_size > 0 && n > ctx.max_array_size) [[unlikely]] {
                     ctx.error = error_code::invalid_length;
                     return false;
                  }
               }

               if constexpr (resizable<T>) {
                  value.resize(n);

                  if constexpr (check_shrink_to_fit(Opts)) {
                     value.shrink_to_fit();
                  }
               }
               else {
                  if (n > value.size()) {
                     ctx.error = error_code::syntax_error;
                     return false;
                  }
               }
               return true;
            };

            auto prepare = [&](const size_t element_size) -> size_t {
               ++it;
               if (invalid_end(ctx, it, end)) {
                  return 0;
               }

               std::conditional_t<Opts.partial_read, size_t, const size_t> n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return 0;
               }

               if constexpr (Opts.partial_read) {
                  n = value.size();
               }

               if (typed_array_out_of_bounds(ctx, it, end, n, element_size)) return 0;
               if (!validate_and_resize(n)) {
                  return 0;
               }

               return n;
            };

            size_t n = 0;

            if (tag == tag::aligned_typed_array) {
               // Aligned typed array: ALIGNED_HEADER | NUMERIC_HEADER | SIZE | PADDING_LENGTH | PADDING | DATA
               ++it; // skip aligned header byte
               if (invalid_end(ctx, it, end)) {
                  return;
               }
               const auto numeric_tag = uint8_t(*it);
               // Verify bits 0-2 are typed_array tag
               if ((numeric_tag & 0b00000'111) != tag::typed_array) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               // Verify it's a numeric type (category 0, 1, or 2, not 3)
               if (((numeric_tag >> 3) & 0b11) == 3) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               if (numeric_tag != header) {
                  if constexpr (check_allow_conversions(Opts)) {
                     const uint8_t elem_byte_count = byte_count_lookup[numeric_tag >> 5];
                     ++it; // skip numeric header
                     std::conditional_t<Opts.partial_read, size_t, const size_t> count =
                        int_from_compressed(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     if constexpr (Opts.partial_read) {
                        count = value.size();
                     }

                     // Read padding length byte and skip padding
                     if (invalid_end(ctx, it, end)) {
                        return;
                     }
                     const uint8_t padding = uint8_t(*it);
                     ++it;
                     if (padding >= elem_byte_count) [[unlikely]] {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                     if (typed_array_out_of_bounds(ctx, it, end, count, elem_byte_count, padding)) return;
                     it += padding;

                     if (!validate_and_resize(count)) {
                        return;
                     }

                     for (auto&& x : value) {
                        const uint8_t number_tag = tag::number | (numeric_tag & 0b11111000);
                        parse<BEVE>::op<no_header_on<Opts>()>(x, number_tag, ctx, it, end);
                     }
                     return;
                  }
                  else {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }

               ++it; // skip numeric header
               std::conditional_t<Opts.partial_read, size_t, const size_t> count = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               if constexpr (Opts.partial_read) {
                  count = value.size();
               }

               // Read padding length byte and skip padding
               if (invalid_end(ctx, it, end)) {
                  return;
               }
               const uint8_t padding = uint8_t(*it);
               ++it;
               if (padding >= sizeof(V)) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }

               if (typed_array_out_of_bounds(ctx, it, end, count, sizeof(V), padding)) return;
               it += padding;

               if (!validate_and_resize(count)) {
                  return;
               }
               n = count;
            }
            else if (tag != header) {
               if constexpr (check_allow_conversions(Opts)) {
                  if (tag != header) [[unlikely]] {
                     if constexpr (check_allow_conversions(Opts)) {
                        if ((tag & 0b00000111) != tag::typed_array) {
                           ctx.error = error_code::syntax_error;
                           return;
                        }

                        const uint8_t byte_count = byte_count_lookup[tag >> 5];
                        prepare(byte_count);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }

                        for (auto&& x : value) {
                           const uint8_t number_tag = tag::number | (tag & 0b11111000);
                           parse<BEVE>::op<no_header_on<Opts>()>(x, number_tag, ctx, it, end);
                        }
                        return;
                     }
                     else {
                        ctx.error = error_code::syntax_error;
                        return;
                     }
                  }
               }
               else {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            else {
               n = prepare(sizeof(V));
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
            }

            if constexpr (contiguous<T>) {
               constexpr auto is_volatile =
                  std::is_volatile_v<std::remove_reference_t<std::remove_pointer_t<decltype(value.data())>>>;

               if constexpr (is_volatile) {
                  for (size_t i = 0; i < n; ++i) {
                     if ((it + sizeof(V)) > end) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }

                     V temp;
                     std::memcpy(&temp, it, sizeof(V));
                     if constexpr (std::endian::native == std::endian::big) {
                        byteswap_le(temp);
                     }
                     value[i] = temp;
                     it += sizeof(V);
                  }
               }
               else if constexpr (std::endian::native == std::endian::big && sizeof(V) > 1) {
                  // On big endian, read and swap each element
                  if (typed_array_out_of_bounds(ctx, it, end, n, sizeof(V))) return;
                  for (size_t i = 0; i < n; ++i) {
                     std::memcpy(&value[i], it, sizeof(V));
                     byteswap_le(value[i]);
                     it += sizeof(V);
                  }
               }
               else {
                  // Little endian or single-byte: bulk memcpy
                  if (typed_array_out_of_bounds(ctx, it, end, n, sizeof(V))) return;
                  if (n) {
                     std::memcpy(value.data(), it, n * sizeof(V));
                     it += n * sizeof(V);
                  }
               }
            }
            else {
               for (auto&& x : value) {
                  if ((it + sizeof(V)) > end) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }

                  std::memcpy(&x, it, sizeof(V));
                  if constexpr (std::endian::native == std::endian::big) {
                     byteswap_le(x);
                  }
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
            std::conditional_t<Opts.partial_read, size_t, const size_t> n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (Opts.partial_read) {
               n = value.size();
            }

            // Each string needs at least 1 byte for length header
            if (uint64_t(end - it) < n) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
            if constexpr (check_max_array_size(Opts) > 0) {
               if (n > check_max_array_size(Opts)) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
               if (ctx.max_array_size > 0 && n > ctx.max_array_size) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }

            if constexpr (resizable<T>) {
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts)) {
                  value.shrink_to_fit();
               }
            }

            for (auto&& x : value) {
               const auto length = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               if (uint64_t(end - it) < length) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }
               if constexpr (check_max_string_length(Opts) > 0) {
                  if (length > check_max_string_length(Opts)) [[unlikely]] {
                     ctx.error = error_code::invalid_length;
                     return;
                  }
               }
               if constexpr (has_runtime_max_string_length<std::decay_t<decltype(ctx)>>) {
                  if (ctx.max_string_length > 0 && length > ctx.max_string_length) [[unlikely]] {
                     ctx.error = error_code::invalid_length;
                     return;
                  }
               }

               x.resize(length);

               if constexpr (check_shrink_to_fit(Opts)) {
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
            if (invalid_end(ctx, it, end)) {
               return;
            }

            using X = typename V::value_type;
            constexpr uint8_t complex_array = 1;
            constexpr uint8_t type = std::floating_point<X> ? 0 : (std::is_signed_v<X> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t complex_header = complex_array | type | (byte_count<X> << 5);
            const auto complex_tag = uint8_t(*it);
            if (complex_tag != complex_header) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
            std::conditional_t<Opts.partial_read, size_t, const size_t> n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (Opts.partial_read) {
               n = value.size();
            }

            if (typed_array_out_of_bounds(ctx, it, end, n, sizeof(V))) return;
            if constexpr (check_max_array_size(Opts) > 0) {
               if (n > check_max_array_size(Opts)) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
               if (ctx.max_array_size > 0 && n > ctx.max_array_size) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }

            if constexpr (resizable<T>) {
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts)) {
                  value.shrink_to_fit();
               }
            }
            else {
               if (n > value.size()) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }

            if constexpr (contiguous<T>) {
               if constexpr (std::endian::native == std::endian::big && sizeof(X) > 1) {
                  // On big endian, read and swap each complex element's components
                  for (size_t i = 0; i < n; ++i) {
                     std::memcpy(&value[i], it, sizeof(V));
                     X real_part = value[i].real();
                     X imag_part = value[i].imag();
                     byteswap_le(real_part);
                     byteswap_le(imag_part);
                     value[i] = V(real_part, imag_part);
                     it += sizeof(V);
                  }
               }
               else {
                  // Little endian or single-byte: bulk memcpy
                  if (n) {
                     std::memcpy(value.data(), it, n * sizeof(V));
                     it += n * sizeof(V);
                  }
               }
            }
            else {
               for (auto&& x : value) {
                  std::memcpy(&x, it, sizeof(V));
                  if constexpr (std::endian::native == std::endian::big && sizeof(X) > 1) {
                     X real_part = x.real();
                     X imag_part = x.imag();
                     byteswap_le(real_part);
                     byteswap_le(imag_part);
                     x = V(real_part, imag_part);
                  }
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
            std::conditional_t<check_partial_read(Opts), size_t, const size_t> n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (Opts.partial_read) {
               n = value.size();
            }

            // Each element needs at least 1 byte for its tag
            if (uint64_t(end - it) < n) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }
            if constexpr (check_max_array_size(Opts) > 0) {
               if (n > check_max_array_size(Opts)) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            if constexpr (has_runtime_max_array_size<std::decay_t<decltype(ctx)>>) {
               if (ctx.max_array_size > 0 && n > ctx.max_array_size) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }

            if constexpr (resizable<T>) {
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts)) {
                  value.shrink_to_fit();
               }
            }

            for (auto&& item : value) {
               parse<BEVE>::op<Opts>(item, ctx, it, end);
            }
         }
      }

      // for types like std::vector<std::pair...> that can't look up with operator[]
      // Instead of hashing or linear searching, we just clear the input and overwrite the entire contents
      template <auto Opts>
         requires(pair_t<range_value_t<T>> && check_concatenate(Opts) == true)
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         using Element = typename T::value_type;
         using Key = typename Element::first_type;

         constexpr uint8_t header = beve_key_traits<Key>::header;

         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);
         if (tag != header) [[unlikely]] {
            if constexpr (check_allow_conversions(Opts)) {
               // Every BEVE object/map has the object category in bits 0-2; only the key-type bits
               // (3-4) may differ under conversions. Reject any non-object category so a non-object
               // value (e.g. a typed array) is never mis-read as a map.
               if ((tag & 0b00000'111) != tag::object) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               const auto key_type = tag & 0b000'11'000;
               if constexpr (beve_key_traits<Key>::as_string) {
                  if (key_type != 0) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               else {
                  if (key_type == 0) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         ++it;
         const size_t n = int_from_compressed(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         value.clear();

         constexpr uint8_t key_tag = beve_key_traits<Key>::key_tag;
         for (size_t i = 0; i < n; ++i) {
            auto& item = value.emplace_back();
            parse<BEVE>::op<no_header_on<Opts>()>(item.first, key_tag, ctx, it, end);
            parse<BEVE>::op<Opts>(item.second, ctx, it, end);
         }
      }
   };

   template <pair_t T>
   struct from<BEVE, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(T& value, is_context auto&& ctx, auto&& it, auto end)
      {
         using Key = typename T::first_type;

         constexpr uint8_t header = beve_key_traits<Key>::header;

         if (invalid_end(ctx, it, end)) {
            return;
         }
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

         constexpr uint8_t key_tag = beve_key_traits<Key>::key_tag;
         parse<BEVE>::op<no_header_on<Opts>()>(value.first, key_tag, ctx, it, end);
         parse<BEVE>::op<Opts>(value.second, ctx, it, end);
      }
   };

   template <readable_map_t T>
   struct from<BEVE, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         using Key = typename T::key_type;

         constexpr uint8_t header = beve_key_traits<Key>::header;

         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);
         if (tag != header) [[unlikely]] {
            if constexpr (check_allow_conversions(Opts)) {
               // Every BEVE object/map has the object category in bits 0-2; only the key-type bits
               // (3-4) may differ under conversions. Reject any non-object category so a non-object
               // value (e.g. a typed array) is never mis-read as a map.
               if ((tag & 0b00000'111) != tag::object) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               const auto key_type = tag & 0b000'11'000;
               if constexpr (beve_key_traits<Key>::as_string) {
                  if (key_type != 0) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
               else {
                  if (key_type == 0) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
               }
            }
            else {
               ctx.error = error_code::syntax_error;
               return;
            }
         }

         ++it;
         std::conditional_t<Opts.partial_read, size_t, const size_t> n = int_from_compressed(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         if constexpr (Opts.partial_read) {
            n = value.size();
         }
         else {
            // Validate count against remaining buffer size (minimum 1 byte per key-value pair)
            if (n > size_t(end - it)) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return;
            }

            // Check user-configured map size limit
            if constexpr (check_max_map_size(Opts) > 0) {
               if (n > check_max_map_size(Opts)) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
            if constexpr (has_runtime_max_map_size<std::decay_t<decltype(ctx)>>) {
               if (ctx.max_map_size > 0 && n > ctx.max_map_size) [[unlikely]] {
                  ctx.error = error_code::invalid_length;
                  return;
               }
            }
         }

         constexpr uint8_t key_tag = beve_key_traits<Key>::key_tag;

         if constexpr (beve_key_traits<Key>::as_number) {
            Key key{}; // Value-initialize to silence false positive -Wmaybe-uninitialized
            for (size_t i = 0; i < n; ++i) {
               if constexpr (Opts.partial_read) {
                  parse<BEVE>::op<no_header_on<Opts>()>(key, key_tag, ctx, it, end);
                  if (auto element = value.find(key); element != value.end()) {
                     parse<BEVE>::op<Opts>(element->second, ctx, it, end);
                  }
               }
               else {
                  // convert the object tag to the key type tag
                  parse<BEVE>::op<no_header_on<Opts>()>(key, key_tag, ctx, it, end);
                  parse<BEVE>::op<Opts>(value[key], ctx, it, end);
               }
            }
         }
         else if constexpr (std::is_same_v<Key, std::string>) {
            for (size_t i = 0; i < n; ++i) {
               ctx.scratch.clear();
               if constexpr (Opts.partial_read) {
                  parse<BEVE>::op<no_header_on<Opts>()>(ctx.scratch, key_tag, ctx, it, end);
                  if (auto element = value.find(ctx.scratch); element != value.end()) {
                     parse<BEVE>::op<Opts>(element->second, ctx, it, end);
                  }
               }
               else {
                  parse<BEVE>::op<no_header_on<Opts>()>(ctx.scratch, key_tag, ctx, it, end);
                  parse<BEVE>::op<Opts>(value[ctx.scratch], ctx, it, end);
               }
            }
         }
         else {
            Key key;
            for (size_t i = 0; i < n; ++i) {
               if constexpr (Opts.partial_read) {
                  parse<BEVE>::op<no_header_on<Opts>()>(key, key_tag, ctx, it, end);
                  if (auto element = value.find(key); element != value.end()) {
                     parse<BEVE>::op<Opts>(element->second, ctx, it, end);
                  }
               }
               else {
                  parse<BEVE>::op<no_header_on<Opts>()>(key, key_tag, ctx, it, end);
                  parse<BEVE>::op<Opts>(value[key], ctx, it, end);
               }
            }
         }
      }
   };

   template <is_expected T>
   struct from<BEVE, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         constexpr uint8_t object_header = tag::object; // string keys

         if (invalid_end(ctx, it, end)) {
            return;
         }

         auto parse_val = [&] {
            if constexpr (not std::is_void_v<typename std::decay_t<T>::value_type>) {
               if (value) {
                  parse<BEVE>::op<Opts>(*value, ctx, it, end);
               }
               else {
                  value.emplace();
                  parse<BEVE>::op<Opts>(*value, ctx, it, end);
               }
            }
            else {
               value.emplace();
            }
         };

         const auto tag = uint8_t(*it);
         if ((tag & 0b111) == object_header) {
            auto start = it;
            ++it;

            const auto n_keys = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if (n_keys == 0) {
               // empty object
               if constexpr (std::is_void_v<typename std::decay_t<T>::value_type>) {
                  value.emplace();
               }
               else {
                  // rewind and parse as value (the value type might be an empty object)
                  it = start;
                  parse_val();
               }
            }
            else if (n_keys == 1) {
               // could be unexpected wrapper or a single-field object value
               const auto key_len = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               static constexpr sv unexpected_key = "unexpected";
               if (key_len == unexpected_key.size() && uint64_t(end - it) >= key_len) {
                  if (std::memcmp(it, unexpected_key.data(), key_len) == 0) {
                     // this is an unexpected wrapper
                     it += key_len;

                     using error_type = typename std::decay_t<T>::error_type;
                     if (!value) {
                        parse<BEVE>::op<Opts>(value.error(), ctx, it, end);
                     }
                     else {
                        std::decay_t<error_type> error{};
                        parse<BEVE>::op<Opts>(error, ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]] {
                           return;
                        }
                        value = glz::unexpected(std::move(error));
                     }
                     return;
                  }
               }
               // not an unexpected wrapper, rewind and parse as value
               it = start;
               parse_val();
            }
            else {
               // multiple keys, must be a value object
               it = start;
               parse_val();
            }
         }
         else {
            // not an object, parse as value directly
            parse_val();
         }
      }
   };

   template <nullable_t T>
      requires(std::is_array_v<T>)
   struct from<BEVE, T> final
   {
      template <auto Opts, class V, size_t N>
      GLZ_ALWAYS_INLINE static void op(V (&value)[N], is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         parse<BEVE>::op<Opts>(std::span{value, N}, ctx, it, end);
      }
   };

   template <nullable_like T>
   struct from<BEVE, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
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
               else if constexpr (std::is_pointer_v<T> && can_allocate_raw_pointer<Opts, std::decay_t<decltype(ctx)>>) {
                  if (!try_allocate_raw_pointer<Opts>(value, ctx)) {
                     return;
                  }
               }
               else {
                  ctx.error = error_code::invalid_nullable_read;
                  return;
                  // Cannot read into unset nullable that is not std::optional, std::unique_ptr, or std::shared_ptr
               }
            }
            parse<BEVE>::op<Opts>(*value, ctx, it, end);
         }
      }
   };

   template <class T>
      requires(nullable_value_t<T> && not nullable_like<T> && not is_expected<T> && not custom_read<T>)
   struct from<BEVE, T> final
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);

         if (tag == tag::null) {
            ++it;
            if constexpr (requires { value.reset(); }) {
               value.reset();
            }
         }
         else {
            if (not value.has_value()) {
               if constexpr (constructible<T>) {
                  value = meta_construct_v<T>();
               }
               else if constexpr (requires { value.emplace(); }) {
                  value.emplace();
               }
               else {
                  ctx.error = error_code::invalid_nullable_read;
                  return;
               }
            }
            parse<BEVE>::op<Opts>(value.value(), ctx, it, end);
         }
      }
   };

   template <is_includer T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if constexpr (check_no_header(Opts)) {
            skip_compressed_int(ctx, it, end);
         }
         else {
            constexpr uint8_t header = tag::string;

            if (invalid_end(ctx, it, end)) {
               return;
            }
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

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable code from if constexpr
#endif
   template <class T>
      requires((glaze_object_t<T> || reflectable<T>) && not custom_read<T>)
   struct from<BEVE, T> final
   {
      template <auto Opts>
         requires(check_structs_as_arrays(Opts) == true)
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if constexpr (reflectable<T>) {
            constexpr auto N = detail::count_members<T>;
            if constexpr (N == 0) {
               // Handle empty structs by just reading and validating the generic_array header
               if (invalid_end(ctx, it, end)) {
                  return;
               }
               const auto tag = uint8_t(*it);
               if (tag != tag::generic_array) [[unlikely]] {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               ++it;
               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               if (n != 0) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
            else {
               auto t = to_tie(value);
               parse<BEVE>::op<Opts>(t, ctx, it, end);
            }
         }
         else {
            if (invalid_end(ctx, it, end)) {
               return;
            }
            const auto tag = uint8_t(*it);
            if (tag != tag::generic_array) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return;
            }
            ++it;
            using V = std::decay_t<T>;
            constexpr auto N = reflect<V>::size;
            constexpr auto N_written = []<size_t... I>(std::index_sequence<I...>) consteval {
               return (size_t{} + ... + (always_skipped<field_t<V, I>> ? size_t{} : size_t{1}));
            }(std::make_index_sequence<N>{});
            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (n != N_written) {
               ctx.error = error_code::syntax_error;
               return;
            }

            for_each<N>([&]<size_t I>() {
               if constexpr (!always_skipped<field_t<V, I>>) {
                  parse<BEVE>::op<Opts>(get_member(value, get<I>(reflect<V>::values)), ctx, it, end);
               }
            });
         }
      }

      // `Tag`, when non-empty, names a discriminator key to skip silently (used by the variant
      // reader so a merged discriminator is tolerated without disabling unknown-key checking for the
      // object's real fields). Default empty => no discriminator, unchanged behavior.
      // The skip is suppressed when T declares a member of that name: the writer then emits the key
      // twice (once as the discriminator, once as the member) and skipping both would drop the
      // member's value, so it is read as an ordinary member and the real value wins. This mirrors
      // the JSON reader's `contains_tag` guard.
      template <auto Opts, string_literal Tag = "">
         requires(check_structs_as_arrays(Opts) == false)
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         constexpr uint8_t type = 0; // string key
         constexpr uint8_t header = tag::object | type;

         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);
         if (tag != header) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         ++it;

         static constexpr auto N = reflect<T>::size;
         if constexpr (N == 0) {
            (void)value;
         }

         static constexpr bit_array<N> all_fields = [] {
            bit_array<N> arr{};
            for (size_t i = 0; i < N; ++i) {
               arr[i] = true;
            }
            return arr;
         }();

         decltype(auto) fields = [&]() -> decltype(auto) {
            if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
               return bit_array<N>{};
            }
            else {
               return nullptr;
            }
         }();

         const auto n_keys = int_from_compressed(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         for (size_t i = 0; i < n_keys; ++i) {
            if constexpr (Opts.partial_read) {
               if ((all_fields & fields) == all_fields) {
                  return;
               }
            }

            if constexpr (N > 0) {
               static constexpr auto HashInfo = hash_info<T>;

               const auto n = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               if (uint64_t(end - it) < n || it == end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               if constexpr (not Tag.sv().empty() && not has_member_with_name<T>(Tag.sv())) {
                  // Discriminator key injected by the variant reader: skip it (and its value) without
                  // consulting error_on_unknown_keys, while the alternative's real keys stay checked.
                  if (sv{it, n} == Tag.sv()) {
                     it += n;
                     skip_value<BEVE>::op<Opts>(ctx, it, end);
                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                     continue;
                  }
               }

               const auto index = decode_hash_with_size<BEVE, T, HashInfo, HashInfo.type>::op(it, end, n);

               if (index < N) [[likely]] {
                  if constexpr (Opts.error_on_missing_keys || Opts.partial_read) {
                     fields[index] = true;
                  }

                  const sv key{it, n};
                  it += n;

                  visit<N>(
                     [&]<size_t I>() {
                        static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                        static constexpr auto Length = TargetKey.size();
                        if ((Length == n) && compare<Length>(TargetKey.data(), key.data())) [[likely]] {
                           // Check for null value skipping on read
                           if constexpr (check_skip_null_members_on_read(Opts)) {
                              if (invalid_end(ctx, it, end)) {
                                 return;
                              }
                              if (uint8_t(*it) == tag::null) {
                                 ++it; // Skip the null tag
                                 return;
                              }
                           }

                           if constexpr (reflectable<T>) {
                              parse<BEVE>::op<Opts>(get_member(value, get<I>(to_tie(value))), ctx, it, end);
                           }
                           else {
                              parse<BEVE>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
                           }
                        }
                        else {
                           if constexpr (Opts.error_on_unknown_keys) {
                              ctx.error = error_code::unknown_key;
                              return;
                           }
                           else {
                              skip_value<BEVE>::op<Opts>(ctx, it, end);
                              if (bool(ctx.error)) [[unlikely]]
                                 return;
                           }
                        }
                     },
                     index);

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
                     it += n;
                     skip_value<BEVE>::op<Opts>(ctx, it, end);
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
               skip_value<BEVE>::op<Opts>(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]]
                  return;
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
   };
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

   template <class T>
      requires glaze_array_t<T>
   struct from<BEVE, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);
         if (tag != tag::generic_array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         ++it;

         constexpr auto N = reflect<T>::size;
         const auto n = int_from_compressed(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         if (n != N) {
            ctx.error = error_code::syntax_error;
            return;
         }

         for_each<N>(
            [&]<size_t I>() { parse<BEVE>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end); });
      }
   };

   template <class T>
      requires(tuple_t<T> || is_std_tuple<T>)
   struct from<BEVE, T> final
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);
         if (tag != tag::generic_array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }
         ++it;

         using V = std::decay_t<T>;
         constexpr auto N = glz::tuple_size_v<V>;
         if constexpr (Opts.partial_read) {
            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (is_std_tuple<T>) {
               for_each_short_circuit<N>([&]<auto I>() {
                  if (I < n) {
                     parse<BEVE>::op<Opts>(std::get<I>(value), ctx, it, end);
                     return false; // continue
                  }
                  return true; // short circuit
               });
            }
            else {
               for_each_short_circuit<N>([&]<auto I>() {
                  if (I < n) {
                     parse<BEVE>::op<Opts>(glz::get<I>(value), ctx, it, end);
                     return false; // continue
                  }
                  return true; // short circuit
               });
            }
         }
         else {
            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            if (n != N) {
               ctx.error = error_code::syntax_error;
               return;
            }

            if constexpr (is_std_tuple<T>) {
               for_each<N>([&]<size_t I>() { parse<BEVE>::op<Opts>(std::get<I>(value), ctx, it, end); });
            }
            else {
               for_each<N>([&]<size_t I>() { parse<BEVE>::op<Opts>(glz::get<I>(value), ctx, it, end); });
            }
         }
      }
   };

   template <filesystem_path T>
   struct from<BEVE, T>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args)
      {
         std::string buffer{};
         parse<BEVE>::op<Opts>(buffer, ctx, args...);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         value = buffer;
      }
   };

   template <read_supported<BEVE> T, class Buffer>
   [[nodiscard]] inline error_ctx read_beve(T&& value, Buffer&& buffer)
   {
      return read<opts{.format = BEVE}>(value, std::forward<Buffer>(buffer));
   }

   template <read_supported<BEVE> T, class Buffer>
   [[nodiscard]] inline expected<T, error_ctx> read_beve(Buffer&& buffer)
   {
      T value{};
      const auto pe = read<opts{.format = BEVE}>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
      }
      return value;
   }

   template <auto Opts = opts{}, read_supported<BEVE> T>
   [[nodiscard]] inline error_ctx read_file_beve(T& value, const sv file_name, auto&& buffer)
   {
      context ctx{};
      ctx.current_file = file_name;

      const auto file_error = file_to_buffer(buffer, ctx.current_file);

      if (bool(file_error)) [[unlikely]] {
         return error_ctx{0, file_error};
      }

      return read<set_beve<Opts>()>(value, buffer, ctx);
   }

   template <read_supported<BEVE> T, class Buffer>
   [[deprecated("Use read_beve_untagged instead")]] [[nodiscard]] inline error_ctx read_binary_untagged(T&& value,
                                                                                                        Buffer&& buffer)
   {
      return read<opt_true<opts{.format = BEVE}, structs_as_arrays_opt_tag{}>>(std::forward<T>(value),
                                                                               std::forward<Buffer>(buffer));
   }

   template <read_supported<BEVE> T, class Buffer>
   [[deprecated("Use read_beve_untagged instead")]] [[nodiscard]] inline expected<T, error_ctx> read_binary_untagged(
      Buffer&& buffer)
   {
      T value{};
      const auto pe =
         read<opt_true<opts{.format = BEVE}, structs_as_arrays_opt_tag{}>>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
      }
      return value;
   }

   // read_beve_untagged aliases for naming consistency with write_beve_untagged
   template <read_supported<BEVE> T, class Buffer>
   [[nodiscard]] inline error_ctx read_beve_untagged(T&& value, Buffer&& buffer)
   {
      return read<opt_true<opts{.format = BEVE}, structs_as_arrays_opt_tag{}>>(std::forward<T>(value),
                                                                               std::forward<Buffer>(buffer));
   }

   template <read_supported<BEVE> T, class Buffer>
   [[nodiscard]] inline expected<T, error_ctx> read_beve_untagged(Buffer&& buffer)
   {
      T value{};
      const auto pe =
         read<opt_true<opts{.format = BEVE}, structs_as_arrays_opt_tag{}>>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
      }
      return value;
   }

   template <auto Opts = opts{}, read_supported<BEVE> T>
   [[nodiscard]] inline error_ctx read_file_beve_untagged(T& value, const std::string& file_name, auto&& buffer)
   {
      return read_file_beve<opt_true<Opts, structs_as_arrays_opt_tag{}>>(value, file_name, buffer);
   }

   // ===== Delimited BEVE support for multiple objects in one buffer =====

   // Skip a delimiter byte if present at the current position
   // Returns true if a delimiter was skipped, false otherwise
   GLZ_ALWAYS_INLINE bool skip_beve_delimiter(auto&& it, auto end) noexcept
   {
      if (it < end && uint8_t(*it) == tag::delimiter) {
         ++it;
         return true;
      }
      return false;
   }

   // Read multiple delimiter-separated BEVE values from a buffer into a container
   template <auto Opts = opts{}, class Container, class Buffer>
      requires readable_array_t<Container> && (emplace_backable<Container> || !resizable<Container>)
   [[nodiscard]] error_ctx read_beve_delimited(Container& values, Buffer&& buffer)
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      if (buffer.empty()) {
         if constexpr (resizable<Container>) {
            values.clear();
         }
         return {};
      }

      context ctx{};
      auto it = reinterpret_cast<const char*>(buffer.data());
      auto end = it + buffer.size();
      auto start = it;

      if constexpr (resizable<Container>) {
         values.clear();
      }

      size_t index = 0;
      const size_t container_size = values.size();

      while (it < end) {
         // Skip delimiter if present (except before first value)
         if (index > 0) {
            skip_beve_delimiter(it, end);
         }

         if (it >= end) {
            break;
         }

         if constexpr (emplace_backable<Container>) {
            auto& value = values.emplace_back();
            parse<BEVE>::template op<set_beve<Opts>()>(value, ctx, it, end);
         }
         else {
            if (index >= container_size) {
               ctx.error = error_code::exceeded_static_array_size;
               break;
            }
            parse<BEVE>::template op<set_beve<Opts>()>(values[index], ctx, it, end);
         }

         if (bool(ctx.error)) [[unlikely]] {
            return {size_t(it - start), ctx.error, ctx.custom_error_message};
         }

         ++index;
      }

      return {size_t(it - start), ctx.error, ctx.custom_error_message};
   }

   // Read multiple delimiter-separated BEVE values, returning the container
   template <class Container, auto Opts = opts{}, class Buffer>
      requires readable_array_t<Container> && (emplace_backable<Container> || !resizable<Container>)
   [[nodiscard]] expected<Container, error_ctx> read_beve_delimited(Buffer&& buffer)
   {
      Container values{};
      const auto ec = read_beve_delimited<Opts>(values, std::forward<Buffer>(buffer));
      if (bool(ec)) [[unlikely]] {
         return unexpected(ec);
      }
      return values;
   }

   // Read a single BEVE value from a buffer at a given offset.
   // If a delimiter byte (0x06) is present at the offset, it is automatically skipped.
   // Returns the total bytes consumed from offset (including any skipped delimiter),
   // so the next read offset is simply: offset + *result
   //
   // Example:
   //   size_t offset = 0;
   //   while (offset < buffer.size()) {
   //      auto result = glz::read_beve_at(value, buffer, offset);
   //      if (!result) break;
   //      offset += *result;  // correctly advances past delimiter + value
   //   }
   //
   // Overload with context for runtime options (e.g., max_string_length, max_array_size):
   //   my_context ctx{};
   //   ctx.max_string_length = 1024;
   //   auto result = glz::read_beve_at<glz::opts{}>(value, buffer, offset, ctx);
   template <auto Opts = opts{}, read_supported<BEVE> T, class Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> read_beve_at(T& value, Buffer&& buffer, size_t offset,
                                                               is_context auto&& ctx)
   {
      static_assert(sizeof(decltype(*buffer.data())) == 1);

      if (offset >= buffer.size()) {
         return glz::unexpected(error_ctx{0, error_code::unexpected_end});
      }

      auto it = reinterpret_cast<const char*>(buffer.data()) + offset;
      auto end = reinterpret_cast<const char*>(buffer.data()) + buffer.size();
      auto start = it;

      // Skip leading delimiter if present (included in returned byte count)
      skip_beve_delimiter(it, end);

      if (it >= end) {
         return glz::unexpected(error_ctx{0, error_code::unexpected_end});
      }

      parse<BEVE>::template op<set_beve<Opts>()>(value, ctx, it, end);

      if (bool(ctx.error)) [[unlikely]] {
         return glz::unexpected(error_ctx{size_t(it - start), ctx.error, ctx.custom_error_message});
      }

      return size_t(it - start); // total bytes consumed from offset (delimiter + value)
   }

   template <auto Opts = opts{}, read_supported<BEVE> T, class Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> read_beve_at(T& value, Buffer&& buffer, size_t offset = 0)
   {
      context ctx{};
      return read_beve_at<Opts>(value, std::forward<Buffer>(buffer), offset, ctx);
   }

}
