// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>
#include <variant>

#include "glaze/core/common.hpp"

// Variants with more alternatives than fit in one 64-bit word, for exercising the readers' per-alternative
// candidate bit sets beyond their first word. Alternative I is an object whose single key "k<I>" belongs to
// no other alternative, so each document names exactly one alternative.
namespace wide_variant
{
   constexpr size_t n_digits(size_t v)
   {
      size_t n = 1;
      for (; v >= 10; v /= 10) {
         ++n;
      }
      return n;
   }

   // "k" followed by the decimal digits of I
   template <size_t I>
   struct key
   {
      static constexpr auto chars = [] {
         std::array<char, 1 + n_digits(I)> s{'k'};
         size_t v = I;
         for (size_t d = s.size() - 1; d > 0; --d) {
            s[d] = char('0' + v % 10);
            v /= 10;
         }
         return s;
      }();
      static constexpr std::string_view value{chars.data(), chars.size()};
   };

   template <size_t I>
   struct alt
   {
      int value{};
   };

   // Same shape as alt, but its variant is internally tagged (see the glz::meta below)
   template <size_t I>
   struct tagged_alt
   {
      int value{};
   };

   template <template <size_t> class Alt, class Indices>
   struct make_variant;

   template <template <size_t> class Alt, size_t... I>
   struct make_variant<Alt, std::index_sequence<I...>>
   {
      using type = std::variant<Alt<I>...>;
   };

   template <size_t N>
   using variant = typename make_variant<alt, std::make_index_sequence<N>>::type;

   template <size_t N>
   using tagged_variant = typename make_variant<tagged_alt, std::make_index_sequence<N>>::type;

   // The tests index alternatives at runtime: a compile-time loop over this many alternatives inflates
   // the test's build time far more than it adds coverage.

   // Alternative `index` holding `value`
   template <class V>
   V make(const size_t index, const int value)
   {
      V v{};
      glz::emplace_runtime_variant(v, index);
      std::visit([&](auto& alternative) { alternative.value = value; }, v);
      return v;
   }

   template <class V>
   int value_of(const V& v)
   {
      return std::visit([](const auto& alternative) { return alternative.value; }, v);
   }
}

template <size_t I>
struct glz::meta<wide_variant::alt<I>>
{
   static constexpr auto value = object(wide_variant::key<I>::value, &wide_variant::alt<I>::value);
};

template <size_t I>
struct glz::meta<wide_variant::tagged_alt<I>>
{
   static constexpr auto value = object(wide_variant::key<I>::value, &wide_variant::tagged_alt<I>::value);
};

template <size_t... I>
struct glz::meta<std::variant<wide_variant::tagged_alt<I>...>>
{
   static constexpr std::string_view tag = "type";
   static constexpr std::array<std::string_view, sizeof...(I)> ids{wide_variant::key<I>::value...};
};
