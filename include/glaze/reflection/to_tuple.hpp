// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#if defined(__has_builtin)
#if __has_builtin(__builtin_dump_struct)

#include <tuple>
#include <type_traits>
#include <utility>

namespace glz
{
   namespace detail
   {
      struct any_t final
      {
         template <class T>
         constexpr operator T() {
            if constexpr (std::is_default_constructible_v<T>) {
               return T{};
            }
            else {
               static_assert(false_v<T>, "Your type must be default constructible");
            }
         }
      };

      template <class T, class... Args>
         requires(std::is_aggregate_v<std::remove_cvref_t<T>>)
      consteval auto count_members()
      {
         if constexpr (requires { T{{Args{}}..., {any_t{}}}; } == false) {
            return sizeof...(Args);
         }
         else {
            return count_members<T, Args..., any_t>();
         }
      }

      template <class T>
      constexpr decltype(auto) to_tuple(T&& t)
      {
         constexpr auto N = count_members<std::decay_t<T>>();
         if constexpr (N == 0) {
            return tuplet::tuple{};
         }
         else if constexpr (N == 1) {
            auto&& [p] = t;
            return std::tie(p);
         }
         else if constexpr (N == 2) {
            auto&& [p0, p1] = t;
            return std::tie(p0, p1);
         }
         else if constexpr (N == 3) {
            auto&& [p0, p1, p2] = t;
            return std::tie(p0, p1, p2);
         }
         else if constexpr (N == 4) {
            auto&& [p0, p1, p2, p3] = t;
            return std::tie(p0, p1, p2, p3);
         }
         else if constexpr (N == 5) {
            auto&& [p0, p1, p2, p3, p4] = t;
            return std::tie(p0, p1, p2, p3, p4);
         }
         else if constexpr (N == 6) {
            auto&& [p0, p1, p2, p3, p4, p5] = t;
            return std::tie(p0, p1, p2, p3, p4, p5);
         }
         else if constexpr (N == 7) {
            auto&& [p0, p1, p2, p3, p4, p5, p6] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6);
         }
         else if constexpr (N == 8) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7);
         }
         else if constexpr (N == 9) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8);
         }
         else if constexpr (N == 10) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9);
         }
         else if constexpr (N == 11) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
         }
         else if constexpr (N == 12) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
         }
         else if constexpr (N == 13) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
         }
         else if constexpr (N == 14) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);
         }
         else if constexpr (N == 15) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);
         }
         else if constexpr (N == 16) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15);
         }
         else if constexpr (N == 17) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16);
         }
         else if constexpr (N == 18) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17);
         }
      }
   }
}

#endif
#endif
