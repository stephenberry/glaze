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
         constexpr operator T()
         {
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

      template <class T, size_t N = count_members<std::decay_t<T>>()>
         requires(N <= 32)
      constexpr decltype(auto) to_tuple(T&& t)
      {
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
         else if constexpr (N == 19) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18);
         }
         else if constexpr (N == 20) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19);
         }
         else if constexpr (N == 21) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20);
         }
         else if constexpr (N == 22) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20,
                    p21] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21);
         }
         else if constexpr (N == 23) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22);
         }
         else if constexpr (N == 24) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23);
         }
         else if constexpr (N == 25) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24);
         }
         else if constexpr (N == 26) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24, p25] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24, p25);
         }
         else if constexpr (N == 27) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24, p25, p26] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24, p25, p26);
         }
         else if constexpr (N == 28) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24, p25, p26, p27] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24, p25, p26, p27);
         }
         else if constexpr (N == 29) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24, p25, p26, p27, p28] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24, p25, p26, p27, p28);
         }
         else if constexpr (N == 30) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24, p25, p26, p27, p28, p29] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24, p25, p26, p27, p28, p29);
         }
         else if constexpr (N == 31) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24, p25, p26, p27, p28, p29, p30] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24, p25, p26, p27, p28, p29, p30);
         }
         else if constexpr (N == 32) {
            auto&& [p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22, p23, p24, p25, p26, p27, p28, p29, p30, p31] = t;
            return std::tie(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
                            p20, p21, p22, p23, p24, p25, p26, p27, p28, p29, p30, p31);
         }
      }
   }
}

#endif
#endif
