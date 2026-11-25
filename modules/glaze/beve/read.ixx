// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/beve/read.hpp"
// glz:header std=<bit>
// glz:header std=<concepts>
// glz:header std=<cstddef>
// glz:header std=<cstdint>
// glz:header std=<cstring>
// glz:header std=<optional>
// glz:header std=<span>
// glz:header std=<string>
// glz:header std=<tuple>
// glz:header std=<type_traits>
// glz:header std=<utility>
// glz:header std=<variant>
// glz:header std=<vector>
export module glaze.beve.read;

import std;

import glaze.beve.header;
import glaze.beve.key_traits;
import glaze.beve.skip;

import glaze.util.expected;
import glaze.util.compare;
import glaze.util.for_each;
import glaze.util.variant;
import glaze.util.type_traits;
import glaze.util.bit_array;
import glaze.util.tuple;
import glaze.core.common;
import glaze.core.context;
import glaze.core.opts;
import glaze.core.read;
import glaze.core.reflect;
import glaze.core.meta;
import glaze.core.streaming_state;
import glaze.reflection.to_tuple;
import glaze.tuplet;

import glaze.concepts.container_concepts;

import glaze.util.dump;
import glaze.util.string_literal;

import glaze.file.file_ops;

#include "glaze/util/inline.hpp"

// To handle invalid inputs we must check if (it >= end) at the beginning of each function
// This way we can always call a function after incrementing the iterator without needed to do a tail check
// If we know the first function called has an end check, we don't need a guard at the top of the function
// Also, after almost every function call we need to check if an error was produced

using std::int8_t;
using std::uint8_t;
using std::int16_t;
using std::uint16_t;
using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;
using std::size_t;
// Recursion depth: BEVE spends two bytes per nesting level, so input alone can drive the reader
// arbitrarily deep and overflow the stack. Every reader that consumes an object or generic-array
// header and then descends into the members takes one level with glz::depth_guard, and
// skip_value<BEVE> does the same, which bounds the descent at max_recursive_depth_limit and reports
// exceeded_max_recursive_depth instead of crashing. A reader that hands its header off to another
// reader (a reflectable struct read positionally defers to the tuple reader) leaves the level to that
// reader, so one wire level is counted once.

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

         if (size_t(end - it) < Length) [[unlikely]] {
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
                     if (size_t(end - it) < sizeof(i)) [[unlikely]] {
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

         if (size_t(end - it) < sizeof(V)) [[unlikely]] {
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
            if (size_t(end - it) < sizeof(V)) [[unlikely]] {
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
            if (size_t(end - it) < sizeof(V)) [[unlikely]] {
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
            if (size_t(end - it) < sizeof(V)) [[unlikely]] {
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

            if (size_t(end - it) < 2 * sizeof(V)) [[unlikely]] {
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

   // An alternative that may encode as an object but exposes no key set the deducer can use: a
   // nested variant (its wire shape depends on its own active alternative), a nullable wrapping an
   // object, or a type with a custom reader. These behave like maps for deduction purposes -- they
   // can neither be confirmed nor eliminated by the keys present.
   template <class V>
   inline constexpr bool beve_variant_is_opaque_object_alt = [] {
      using X = std::decay_t<V>;
      if constexpr (glaze_object_t<X> || reflectable<X> || is_memory_object<X> || readable_map_t<X> || pair_t<X>) {
         return false; // has a usable key set, or is already a recognized object category
      }
      else if constexpr (is_variant<X> || custom_read<X>) {
         return true;
      }
      else if constexpr (nullable_t<X>) {
         using E = std::decay_t<decltype(*std::declval<X&>())>;
         return glaze_object_t<E> || reflectable<E> || readable_map_t<E> || pair_t<E> || is_variant<E>;
      }
      else {
         return false;
      }
   }();

   // A variant alternative whose BEVE wire form is (or may be) an object. Used to constrain V2
   // object deduction: an alternative outside this set is never considered for object data.
   template <class V>
   inline constexpr bool beve_variant_is_object_alt = [] {
      using X = std::decay_t<V>;
      return glaze_object_t<X> || reflectable<X> || is_memory_object<X> || readable_map_t<X> || pair_t<X> ||
             beve_variant_is_opaque_object_alt<X>;
   }();

   // An object alternative with no compile-time key set: a map or pair accepts *any* keys, so it can
   // neither be confirmed nor eliminated by structural deduction. `variant_deduction_bits` only sets
   // bits for struct-like alternatives, so these must be excluded from key narrowing explicitly or a
   // single recognized key would wrongly rule them out.
   template <class V>
   inline constexpr bool beve_variant_is_open_ended_object_alt = [] {
      using X = std::decay_t<V>;
      if constexpr (glaze_object_t<X> || reflectable<X> || is_memory_object<X>) {
         return false;
      }
      else {
         return readable_map_t<X> || pair_t<X> || beve_variant_is_opaque_object_alt<X>;
      }
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

      // The tagging representation is a property of the variant, decided once for every alternative.
      static constexpr auto tagging = variant_tagging_v<T>;

      // Try each alternative in declaration order, rewinding the iterator on failure. Returns true
      // when an alternative parsed cleanly, leaving `value` holding it. `input_error` records a
      // failure that is a property of the input rather than of the alternative set -- a buffer that
      // ran out or nesting past the depth limit -- which no other alternative could have done better
      // with.
      template <auto Opts>
      static bool try_each_pass(auto&& value, is_context auto&& ctx, auto&& it, auto end,
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
            it = start;
            if (value.index() != I) {
               emplace_runtime_variant(value, I);
            }
            ctx.error = error_code::none;
            ctx.custom_error_message = {}; // else a rejected alternative's message outlives its error
            parse<BEVE>::op<Opts>(std::get<I>(value), ctx, it, end);
            // Charge only what a REJECTED attempt parsed. That is the wasted work the bound is
            // about; charging the successful alternative too would bill every enclosing level for
            // the same bytes and make a valid nest look exponential.
            const bool budget_left = bool(ctx.error) ? charge_speculation(ctx, size_t(it - start)) : true;
            if (!bool(ctx.error)) {
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
         error_code input_error{};
         if constexpr (check_allow_conversions(Opts)) {
            if (try_each_pass<opt_false<Opts, allow_conversions_opt_tag{}>>(value, ctx, it, end, input_error)) {
               return;
            }
         }
         // A lenient retry cannot make over-nested input readable, and running it doubles the work at
         // every level of a deep nest.
         if (input_error != error_code::exceeded_max_recursive_depth &&
             try_each_pass<Opts>(value, ctx, it, end, input_error)) {
            return;
         }
         it = start;
         // An incomplete or over-nested buffer is a property of the input, not of the alternative set,
         // so report it as such instead of blaming variant resolution.
         ctx.error = input_error != error_code::none ? input_error : error_code::no_matching_variant_type;
      }

      // Parse the object at `it` as alternative I, handling the merged discriminator key when the
      // variant is tagged. `value` must already hold alternative I.
      template <auto Opts, bool tagged, size_t I>
      static void parse_object_alt(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         using V = std::variant_alternative_t<I, T>;
         using X = std::conditional_t<is_memory_object<V>, memory_type<V>, V>;

         // reflect<X>::size may only be named for struct-like X (a std::pair/map alternative has no
         // reflect specialization); beve_is_empty_struct<X> guards that at namespace scope.
         constexpr bool struct_like = (glaze_object_t<X> || reflectable<X>) && (not custom_read<X>);
         // A unit alternative reads the same way an empty struct does: the object it was written as
         // holds nothing but the discriminator. (A bare `null`, which is how untagged variants and
         // pre-Version-2 data spell it, never reaches here -- it is not an object.)
         constexpr bool empty_tagged_struct = tagged && (beve_is_empty_struct<X> || variant_unit_alternative<V>);

         if constexpr (empty_tagged_struct) {
            // Carries only the discriminator: skip the whole {tag:id} object.
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
      }

      // Parse the object at `obj_start` as `resolved`; if that fails and `try_others` is set, rewind
      // and try each of the remaining object alternatives. Returns true once one parses cleanly.
      //
      // Structural deduction is a guess, so a failure means "probably the wrong alternative" -- but
      // only for failures that say the shape does not fit. A missing_key failure is the caller's own
      // error_on_missing_keys strictness being enforced; retrying past it would answer a strict read
      // with a different alternative instead of the error the caller asked for.
      template <auto AltOpts, bool tagged, class It>
      static bool attempt_object_alts(auto&& value, is_context auto&& ctx, auto&& it, auto end, const It& obj_start,
                                      const size_t resolved, const bool try_others) noexcept
      {
         it = obj_start;
         ctx.error = error_code::none;
         ctx.custom_error_message = {};
         if (value.index() != resolved) {
            emplace_runtime_variant(value, resolved);
         }
         visit<variant_size>([&]<size_t I>() { parse_object_alt<AltOpts, tagged, I>(value, ctx, it, end); }, resolved);
         if (!bool(ctx.error)) {
            return true;
         }
         // The missing_key exclusion is the caller's own strictness (see below). Nesting past the depth
         // limit is excluded for a different reason: it is a property of the input rather than of the
         // alternative, so retrying re-parses the whole subtree for nothing -- once per alternative,
         // at every level of the nest, which is exponential in the depth.
         if (!try_others || ctx.error == error_code::missing_key ||
             ctx.error == error_code::exceeded_max_recursive_depth) {
            return false;
         }

         bool recovered = false;
         bool exhausted = false;
         for_each<variant_size>([&]<size_t I>() {
            if (exhausted) {
               return;
            }
            if constexpr (beve_variant_is_object_alt<std::variant_alternative_t<I, T>>) {
               using V = std::variant_alternative_t<I, T>;
               using X = std::conditional_t<is_memory_object<V>, memory_type<V>, V>;
               // A tagged empty-struct or unit alternative is read by skipping the whole object, so
               // it "succeeds" on anything. As the deduced choice that is intended; as a fallback it
               // would be a wildcard that silently discards the payload.
               if constexpr (tagged && (beve_is_empty_struct<X> || variant_unit_alternative<V>)) {
                  return;
               }
               else {
                  if (recovered || exhausted || I == resolved) {
                     return;
                  }
                  it = obj_start;
                  ctx.error = error_code::none;
                  ctx.custom_error_message = {};
                  if (value.index() != I) {
                     emplace_runtime_variant(value, I);
                  }
                  parse_object_alt<AltOpts, tagged, I>(value, ctx, it, end);
                  recovered = !bool(ctx.error);
                  if (!recovered && !charge_speculation(ctx, size_t(it - obj_start))) {
                     // Out of budget: an ambiguous nest re-parses the same subtree per alternative at
                     // every level, which is exponential. Keep this error and stop retrying.
                     exhausted = true;
                  }
               }
            }
         });
         return recovered;
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
            // No alternative is a recognized object category, but one may still read an object
            // through a custom reader, so try each rather than rejecting outright.
            try_each<Opts>(value, ctx, it, end);
            return;
         }
         else if constexpr (n_obj == 1 && tag_v<T>.empty()) {
            constexpr size_t I = [] {
               size_t r = variant_size;
               [&]<size_t... J>(std::index_sequence<J...>) {
                  ((beve_variant_is_object_alt<std::variant_alternative_t<J, T>> && r == variant_size ? (void)(r = J)
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

            // Open-ended (map/pair) alternatives accept any key set, so they must survive key
            // narrowing. Pre-setting their bits in a copy of the deduction table keeps the scan loop
            // a single AND and confines the adjustment to compile time.
            static constexpr auto open_ended = [] {
               auto m = bit_array<variant_size>{};
               [&]<size_t... I>(std::index_sequence<I...>) {
                  ((m[I] = beve_variant_is_open_ended_object_alt<std::variant_alternative_t<I, T>>), ...);
               }(std::make_index_sequence<variant_size>{});
               return m;
            }();
            static constexpr auto narrowing_bits = [] {
               auto b = variant_deduction_bits<T>;
               for (size_t k = 0; k < b.size(); ++k) {
                  for (size_t i = 0; i < variant_size; ++i) {
                     if (open_ended[i]) {
                        b[k][i] = true;
                     }
                  }
               }
               return b;
            }();

            // Set when a key belongs to no alternative's declared field set. No struct alternative
            // can have written such a key, so an open-ended alternative is the better explanation of
            // the data even though the struct might tolerate the key under error_on_unknown_keys.
            bool foreign_key = false;

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
                        tag_index =
                           variant_id_to_index<T>::op(id_view.data(), id_view.data() + id_view.size(), id_view.size());
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
                     possible = possible & narrowing_bits[di];
                     if constexpr (not tagged) {
                        // Once a single candidate remains there is nothing left to learn, and the
                        // remaining values do not need to be skipped -- pass 2 re-reads the object
                        // from the untouched cursor. Without this the scan walks every nested
                        // subtree, making a variant nested N deep cost O(N^2) to read.
                        //
                        // Safe with respect to foreign_key: open-ended alternatives are pre-set in
                        // narrowing_bits and so are never narrowed away. A lone survivor therefore
                        // means either no open-ended candidate existed (foreign_key could not have
                        // changed the outcome) or the survivor is itself the open-ended one.
                        if (possible.popcount() == 1) {
                           break;
                        }
                     }
                  }
                  else {
                     foreign_key = true;
                  }
               }
               else {
                  // No alternative declares any field, so every key is foreign.
                  foreign_key = true;
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
               if (foreign_key) {
                  // Restrict to the open-ended alternatives when there are any: a key outside every
                  // declared field set rules out the struct alternatives as the writer of this data.
                  auto open_possible = possible & open_ended;
                  if (open_possible.popcount() > 0) {
                     possible = open_possible;
                  }
               }
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
            //
            // Like try_each, this runs strict first (allow_conversions off) so an alternative is
            // taken only when its members' type headers match exactly. That is required for
            // correctness, not fidelity: struct alternatives sharing a key set are separated by
            // nothing but their member types, so a lenient-only pass lets
            // variant<{int32_t a}, {int64_t a}> read its own output back into the int32_t
            // alternative and silently truncate. Only if nothing matches exactly do we retry
            // leniently, which keeps reading data whose numeric widths no longer match.
            const auto obj_start = it;
            const bool authoritative = tagged && tag_decoded;

            if constexpr (check_allow_conversions(Opts)) {
               if (not authoritative) {
                  if (attempt_object_alts<opt_false<Opts, allow_conversions_opt_tag{}>, tagged>(
                         value, ctx, it, end, obj_start, resolved, true)) {
                     return;
                  }
               }
            }
            if (attempt_object_alts<Opts, tagged>(value, ctx, it, end, obj_start, resolved, not authoritative)) {
               return;
            }

            // The missing_key exclusion applies here too: the last resort must not answer a strict
            // read with some other alternative that happens to tolerate the absent key. Over-nested
            // input is likewise not worth another parse of the whole subtree.
            if (not authoritative && ctx.error != error_code::missing_key &&
                ctx.error != error_code::exceeded_max_recursive_depth) {
               const auto first_error = ctx.error;
               const auto first_error_it = it;
               // Last resort: an alternative outside the recognized object categories (a custom
               // reader, say) may still consume this object.
               it = obj_start;
               ctx.error = error_code::none;
               ctx.custom_error_message = {};
               try_each<Opts>(value, ctx, it, end);
               if (bool(ctx.error)) {
                  // Report the deduced alternative's failure: it is the most specific diagnosis.
                  ctx.error = first_error;
                  ctx.custom_error_message = {};
                  it = first_error_it;
               }
            }
         }
      }

      // Decode the discriminator VALUE at `it` and map it to an alternative index, advancing past the
      // value. Returns variant_size when the id names no alternative and there is no unlabeled
      // default, having set no error; the caller decides whether that is fatal.
      template <auto Opts>
      static size_t resolve_id(is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         using id_type = std::decay_t<decltype(ids_v<T>[0])>;
         size_t index = ids_v<T>.size();
         if constexpr (std::integral<id_type>) {
            id_type id{};
            from<BEVE, id_type>::template op<Opts>(id, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return variant_size;
            }
            index = variant_id_to_index<T>::op(id);
         }
         else {
            if (invalid_end(ctx, it, end)) {
               return variant_size;
            }
            if (uint8_t(*it) != tag::string) [[unlikely]] {
               ctx.error = error_code::syntax_error;
               return variant_size;
            }
            ++it;
            const auto slen = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return variant_size;
            }
            if (uint64_t(end - it) < slen) [[unlikely]] {
               ctx.error = error_code::unexpected_end;
               return variant_size;
            }
            const sv id_view{it, size_t(slen)};
            it += slen;
            index = variant_id_to_index<T>::op(id_view.data(), id_view.data() + id_view.size(), id_view.size());
         }

         if (index < ids_v<T>.size()) [[likely]] {
            return index;
         }
         if constexpr (ids_v<T>.size() < variant_size) {
            // Fewer ids than alternatives: the first unlabeled alternative is the default for an
            // unrecognized id, matching the keyed reader.
            return ids_v<T>.size();
         }
         return variant_size;
      }

      // Read the keyed adjacent form { tag : id, content : value }. The discriminator names the
      // alternative outright, so no structural deduction is involved and alternatives of any shape --
      // including several sharing one -- are separated cleanly.
      template <auto Opts>
      static void read_adjacent(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         if (uint8_t(*it) != tag::object) [[unlikely]] {
            // Adjacent tagging has exactly one wire shape; anything else is not this variant's data.
            ctx.error = error_code::syntax_error;
            return;
         }

         // The content of one adjacently tagged variant may be another, and this reader recurses
         // without going through an object reader that would count the nesting for it.
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         const auto obj_start = it;

         // Walk the members, handing each key to `on_key`, which must consume that key's value.
         // Returns false once an error has been set.
         const auto walk = [&](auto&& on_key) {
            it = obj_start;
            ++it; // object tag byte
            const auto n_keys = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return false;
            }
            for (size_t k = 0; k < n_keys; ++k) {
               const auto klen = int_from_compressed(ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return false;
               }
               if (uint64_t(end - it) < klen) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return false;
               }
               const sv key{it, size_t(klen)};
               it += klen;
               if (!on_key(key)) {
                  return false;
               }
            }
            return true;
         };

         size_t resolved = variant_size;
         bool tag_seen = false;
         bool content_scanned = false;

         if (!walk([&](const sv key) {
                if (!tag_seen && key == tag_v<T>) {
                   resolved = resolve_id<Opts>(ctx, it, end);
                   tag_seen = true;
                   return !bool(ctx.error);
                }
                if (!content_scanned && key == content_v<T>) {
                   content_scanned = true;
                   skip_value<BEVE>::op<Opts>(ctx, it, end);
                   return !bool(ctx.error);
                }
                if constexpr (requires { Opts.error_on_unknown_keys; }) {
                   if constexpr (Opts.error_on_unknown_keys) {
                      // Only the two declared keys belong in an adjacently tagged object, and each
                      // only once.
                      ctx.error = error_code::unknown_key;
                      return false;
                   }
                }
                skip_value<BEVE>::op<Opts>(ctx, it, end);
                return !bool(ctx.error);
             })) {
            return;
         }

         if (!tag_seen) [[unlikely]] {
            ctx.error = error_code::missing_key;
            ctx.custom_error_message = variant_ids_string_v<T>;
            return;
         }
         if (resolved >= variant_size) [[unlikely]] {
            ctx.error = error_code::no_matching_variant_type;
            ctx.custom_error_message = variant_ids_string_v<T>;
            return;
         }

         if (value.index() != resolved) {
            emplace_runtime_variant(value, resolved);
         }

         bool content_seen = false;
         if (!walk([&](const sv key) {
                if (!content_seen && key == content_v<T>) {
                   content_seen = true;
                   std::visit([&](auto&& v) { parse<BEVE>::op<Opts>(v, ctx, it, end); }, value);
                   return !bool(ctx.error);
                }
                skip_value<BEVE>::op<Opts>(ctx, it, end);
                return !bool(ctx.error);
             })) {
            return;
         }

         if (!content_seen) [[unlikely]] {
            ctx.error = error_code::missing_key;
            ctx.custom_error_message = content_v<T>;
         }
      }

      // Read the positional projection of adjacent tagging, the two element array [id, value].
      // Positional data has no keys, so there is nothing to deduce from if the discriminator does not
      // resolve: a bad id is an error rather than a fallback to try_each, which would be free to pick
      // a same-shaped alternative and silently return the wrong type.
      template <auto Opts>
      static void read_positional_tagged(auto&& value, is_context auto&& ctx, auto&& it, auto end) noexcept
      {
         if (invalid_end(ctx, it, end)) {
            return;
         }
         if (uint8_t(*it) != tag::generic_array) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         // As with read_adjacent: the value of one positionally tagged variant may be another, and
         // this reader consumes the [id, value] array itself rather than through a guarded reader.
         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         ++it;
         const auto n = int_from_compressed(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         if (n != 2) [[unlikely]] {
            ctx.error = error_code::syntax_error;
            return;
         }

         const size_t resolved = resolve_id<Opts>(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         if (resolved >= variant_size) [[unlikely]] {
            ctx.error = error_code::no_matching_variant_type;
            ctx.custom_error_message = variant_ids_string_v<T>;
            return;
         }

         if (value.index() != resolved) {
            emplace_runtime_variant(value, resolved);
         }
         std::visit([&](auto&& v) { parse<BEVE>::op<Opts>(v, ctx, it, end); }, value);
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

         constexpr bool tagged = check_write_type_info(Opts) && tagging != variant_tagging_kind::none;

         // (B) Positional (structs-as-arrays) data carries no keys, so an adjacently tagged variant
         // is written as [id, value] and an untagged one is resolved by trying alternatives.
         // This must be an if/else: `if constexpr (...) { return; }` does not discard the statements
         // that follow it, so without the `else` the (C) block below would still be instantiated in
         // positional mode, where read_object's call into the keyed-object reader does not compile.
         if constexpr (check_structs_as_arrays(Opts)) {
            if constexpr (tagged) {
               static_assert(
                  tagging == variant_tagging_kind::adjacent || detail::beve_positional_tagging_needs_content<T>::value,
                  "Positional reads (structs_as_arrays) have no keys, so there is nothing "
                  "for internal tagging to have merged a discriminator into. Declare "
                  "`content` beside `tag` in glz::meta to select adjacent tagging, which "
                  "projects positionally to the two element array [id, value].");
               read_positional_tagged<Opts>(value, ctx, it, end);
            }
            else {
               try_each<Opts>(value, ctx, it, end);
            }
         }
         else if constexpr (tagged && tagging == variant_tagging_kind::adjacent) {
            // (B2) Adjacent tagging has a single, fully determined shape -- no deduction needed.
            read_adjacent<Opts>(value, ctx, it, end);
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
            if (exceeds_capacity(value, n, ctx)) [[unlikely]] {
               return;
            }
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
         else if constexpr (beve_num_array_t<V>) {
            // NV is the arithmetic wire type: V for plain numbers, the rep for chrono
            // durations / count-based time points (which are bit-compatible with it).
            using NV = beve_num_array_value_t<V>;
            constexpr uint8_t type = std::floating_point<NV> ? 0 : (std::is_signed_v<NV> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t header = tag::typed_array | type | (byte_count<NV> << 5);

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
               if (size_t(end - it) < sizeof(V)) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               NV temp;
               std::memcpy(&temp, it, sizeof(NV));
               if constexpr (std::endian::native == std::endian::big) {
                  byteswap_le(temp);
               }
               it += sizeof(NV);
               value.emplace(beve_num_array_construct<V>(temp));
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

            depth_guard guard{ctx};
            if (!guard) [[unlikely]] {
               return;
            }

            ++it;
            const auto n = int_from_compressed(ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            // Each element needs at least 1 byte for its tag, so a count larger than the remaining
            // input cannot be honored. Without this (and the error check in the loop) an element count
            // of 2^48 in a 3 byte buffer spins for hours on reads that error instantly.
            if (uint64_t(end - it) < n) [[unlikely]] {
               ctx.error = error_code::invalid_length;
               return;
            }

            value.clear();

            for (size_t i = 0; i < n; ++i) {
               V v;
               parse<BEVE>::op<Opts>(v, ctx, it, end);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
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
      requires(std::is_const_v<T> && beve_num_array_t<std::remove_const_t<T>>)
   struct from<BEVE, std::span<T, Extent>> final
   {
      using V = std::remove_const_t<T>;
      // Wire arithmetic type: V for plain numbers, the rep for chrono durations / count-based
      // time points. The header bits must be computed from NV (e.g. std::is_signed_v<duration>
      // is false, but a duration's rep can be signed), while the zero-copy view still points
      // at V objects (which are bit-identical to NV, so sizeof and layout match).
      using NV = beve_num_array_value_t<V>;

      template <auto Opts>
      static void op(std::span<T, Extent>& value, is_context auto&& ctx, auto&& it, auto end)
      {
         assert_owns_its_bytes<decltype(ctx)>();

         if (invalid_end(ctx, it, end)) {
            return;
         }
         const auto tag = uint8_t(*it);

         if constexpr (sizeof(V) == 1) {
            // Single-byte types: accept standard typed arrays (no alignment or endian concerns)
            constexpr uint8_t type = std::floating_point<NV> ? 0 : (std::is_signed_v<NV> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t expected_header = tag::typed_array | type | (byte_count<NV> << 5);
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
            constexpr uint8_t type = std::floating_point<NV> ? 0 : (std::is_signed_v<NV> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t expected_header = tag::typed_array | type | (byte_count<NV> << 5);
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
               if (exceeds_capacity(value, n, ctx)) [[unlikely]] {
                  return;
               }
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
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
         else if constexpr (beve_num_array_t<V>) {
            // NV is the arithmetic wire type: V for plain numbers, the rep for chrono
            // durations / count-based time points (which are bit-compatible with it).
            using NV = beve_num_array_value_t<V>;
            constexpr uint8_t type = std::floating_point<NV> ? 0 : (std::is_signed_v<NV> ? 0b000'01'000 : 0b000'10'000);
            constexpr uint8_t header = tag::typed_array | type | (byte_count<NV> << 5);

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
                  if (exceeds_capacity(value, n, ctx)) [[unlikely]] {
                     return false;
                  }
                  value.resize(n);

                  if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
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
                     if (size_t(end - it) < sizeof(V)) [[unlikely]] {
                        ctx.error = error_code::unexpected_end;
                        return;
                     }

                     NV temp;
                     std::memcpy(&temp, it, sizeof(NV));
                     if constexpr (std::endian::native == std::endian::big) {
                        byteswap_le(temp);
                     }
                     value[i] = beve_num_array_construct<V>(temp);
                     it += sizeof(NV);
                  }
               }
               else if constexpr (std::endian::native == std::endian::big && sizeof(V) > 1) {
                  // On big endian, read and swap each element (through the rep so a chrono
                  // element is byteswapped as its underlying number).
                  if (typed_array_out_of_bounds(ctx, it, end, n, sizeof(V))) return;
                  for (size_t i = 0; i < n; ++i) {
                     NV temp;
                     std::memcpy(&temp, it, sizeof(NV));
                     byteswap_le(temp);
                     value[i] = beve_num_array_construct<V>(temp);
                     it += sizeof(NV);
                  }
               }
               else {
                  // Little endian or single-byte: bulk memcpy. A chrono element is
                  // bit-identical to its rep, so the raw wire bytes populate it directly.
                  if (typed_array_out_of_bounds(ctx, it, end, n, sizeof(V))) return;
                  if (n) {
                     std::memcpy(value.data(), it, n * sizeof(V));
                     it += n * sizeof(V);
                  }
               }
            }
            else {
               for (auto&& x : value) {
                  if (size_t(end - it) < sizeof(V)) [[unlikely]] {
                     ctx.error = error_code::unexpected_end;
                     return;
                  }

                  NV temp;
                  std::memcpy(&temp, it, sizeof(NV));
                  if constexpr (std::endian::native == std::endian::big) {
                     byteswap_le(temp);
                  }
                  x = beve_num_array_construct<V>(temp);
                  it += sizeof(NV);
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
               if (exceeds_capacity(value, n, ctx)) [[unlikely]] {
                  return;
               }
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
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

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<decltype(x)>) {
                  x.shrink_to_fit();
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
               if (exceeds_capacity(value, n, ctx)) [[unlikely]] {
                  return;
               }
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
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

            depth_guard guard{ctx};
            if (!guard) [[unlikely]] {
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
               if (exceeds_capacity(value, n, ctx)) [[unlikely]] {
                  return;
               }
               value.resize(n);

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
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

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
            return;
         }

         ++it;
         const size_t n = int_from_compressed(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         // A pair costs at least one byte of input, so a count exceeding what remains cannot be
         // honored -- the same bound the map reader applies. Bailing here (and on error in the loop)
         // keeps a bogus count from emplacing its way through memory on a read that errors on the
         // first element.
         if (n > size_t(end - it)) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
            return;
         }

         value.clear();

         constexpr uint8_t key_tag = beve_key_traits<Key>::key_tag;
         for (size_t i = 0; i < n; ++i) {
            if (exceeds_capacity(value, i + 1, ctx)) [[unlikely]] {
               return;
            }
            auto& item = value.emplace_back();
            parse<BEVE>::op<no_header_on<Opts>()>(item.first, key_tag, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
            parse<BEVE>::op<Opts>(item.second, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }
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

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
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

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
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

            // The reflectable branch above delegates to the tuple reader, which takes the level there.
            depth_guard guard{ctx};
            if (!guard) [[unlikely]] {
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

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
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

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
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

         depth_guard guard{ctx};
         if (!guard) [[unlikely]] {
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

   export template <read_supported<BEVE> T, class Buffer>
   [[nodiscard]] inline error_ctx read_beve(T&& value, Buffer&& buffer)
   {
      return read<opts{.format = BEVE}>(value, std::forward<Buffer>(buffer));
   }

   export template <read_supported<BEVE> T, class Buffer>
   [[nodiscard]] inline expected<T, error_ctx> read_beve(Buffer&& buffer)
   {
      T value{};
      const auto pe = read<opts{.format = BEVE}>(value, std::forward<Buffer>(buffer));
      if (pe) [[unlikely]] {
         return unexpected(pe);
      }
      return value;
   }

   export template <auto Opts = opts{}, read_supported<BEVE> T>
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

   export template <read_supported<BEVE> T, class Buffer>
   [[deprecated("Use read_beve_untagged instead")]] [[nodiscard]] inline error_ctx read_binary_untagged(T&& value,
                                                                                                         Buffer&& buffer)
   {
      return read<opt_true<opts{.format = BEVE}, structs_as_arrays_opt_tag{}>>(std::forward<T>(value),
                                                                               std::forward<Buffer>(buffer));
   }

   export template <read_supported<BEVE> T, class Buffer>
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
   export template <read_supported<BEVE> T, class Buffer>
   [[nodiscard]] inline error_ctx read_beve_untagged(T&& value, Buffer&& buffer)
   {
      return read<opt_true<opts{.format = BEVE}, structs_as_arrays_opt_tag{}>>(std::forward<T>(value),
                                                                               std::forward<Buffer>(buffer));
   }

   export template <read_supported<BEVE> T, class Buffer>
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

   export template <auto Opts = opts{}, read_supported<BEVE> T>
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
   export template <auto Opts = opts{}, class Container, class Buffer>
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
            if (exceeds_capacity(values, index + 1, ctx)) [[unlikely]] {
               break;
            }
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
   export template <class Container, auto Opts = opts{}, class Buffer>
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
   //   std::size_t offset = 0;
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
   export template <auto Opts = opts{}, read_supported<BEVE> T, class Buffer>
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

   export template <auto Opts = opts{}, read_supported<BEVE> T, class Buffer>
   [[nodiscard]] glz::expected<size_t, error_ctx> read_beve_at(T& value, Buffer&& buffer, size_t offset = 0)
   {
      context ctx{};
      return read_beve_at<Opts>(value, std::forward<Buffer>(buffer), offset, ctx);
   }

}
