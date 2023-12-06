// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

namespace glz
{
   namespace detail
   {
      struct any_t final
      {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
         template <class T>
         [[maybe_unused]] constexpr operator T();
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
         template <class T>
         [[maybe_unused]] constexpr operator T();
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"
         template <class T>
         [[maybe_unused]] constexpr operator T();
#pragma GCC diagnostic pop
#endif
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
            return std::tuple{};
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

      template <class T>
      struct ptr_t final
      {
         const T* ptr;
      };

      template <size_t N, class T, size_t M = count_members<T>()>
         requires(M <= 26)
      constexpr auto get_ptr(T&& t) noexcept
      {
         if constexpr (M == 26) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22,
                    p23, p24, p25, p26] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
            if constexpr (N == 19) return ptr_t<decltype(p20)>{&p20};
            if constexpr (N == 20) return ptr_t<decltype(p21)>{&p21};
            if constexpr (N == 21) return ptr_t<decltype(p22)>{&p22};
            if constexpr (N == 22) return ptr_t<decltype(p23)>{&p23};
            if constexpr (N == 23) return ptr_t<decltype(p24)>{&p24};
            if constexpr (N == 24) return ptr_t<decltype(p25)>{&p25};
            if constexpr (N == 25) return ptr_t<decltype(p26)>{&p26};
         }
         else if constexpr (M == 25) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22,
                    p23, p24, p25] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
            if constexpr (N == 19) return ptr_t<decltype(p20)>{&p20};
            if constexpr (N == 20) return ptr_t<decltype(p21)>{&p21};
            if constexpr (N == 21) return ptr_t<decltype(p22)>{&p22};
            if constexpr (N == 22) return ptr_t<decltype(p23)>{&p23};
            if constexpr (N == 23) return ptr_t<decltype(p24)>{&p24};
            if constexpr (N == 24) return ptr_t<decltype(p25)>{&p25};
         }
         else if constexpr (M == 24) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22,
                    p23, p24] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
            if constexpr (N == 19) return ptr_t<decltype(p20)>{&p20};
            if constexpr (N == 20) return ptr_t<decltype(p21)>{&p21};
            if constexpr (N == 21) return ptr_t<decltype(p22)>{&p22};
            if constexpr (N == 22) return ptr_t<decltype(p23)>{&p23};
            if constexpr (N == 23) return ptr_t<decltype(p24)>{&p24};
         }
         else if constexpr (M == 23) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21, p22,
                    p23] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
            if constexpr (N == 19) return ptr_t<decltype(p20)>{&p20};
            if constexpr (N == 20) return ptr_t<decltype(p21)>{&p21};
            if constexpr (N == 21) return ptr_t<decltype(p22)>{&p22};
            if constexpr (N == 22) return ptr_t<decltype(p23)>{&p23};
         }
         else if constexpr (M == 22) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21,
                    p22] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
            if constexpr (N == 19) return ptr_t<decltype(p20)>{&p20};
            if constexpr (N == 20) return ptr_t<decltype(p21)>{&p21};
            if constexpr (N == 21) return ptr_t<decltype(p22)>{&p22};
         }
         else if constexpr (M == 21) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, p21] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
            if constexpr (N == 19) return ptr_t<decltype(p20)>{&p20};
            if constexpr (N == 20) return ptr_t<decltype(p21)>{&p21};
         }
         else if constexpr (M == 20) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
            if constexpr (N == 19) return ptr_t<decltype(p20)>{&p20};
         }
         else if constexpr (M == 19) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
            if constexpr (N == 18) return ptr_t<decltype(p19)>{&p19};
         }
         else if constexpr (M == 18) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
            if constexpr (N == 17) return ptr_t<decltype(p18)>{&p18};
         }
         else if constexpr (M == 17) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
            if constexpr (N == 16) return ptr_t<decltype(p17)>{&p17};
         }
         else if constexpr (M == 16) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
            if constexpr (N == 15) return ptr_t<decltype(p16)>{&p16};
         }
         else if constexpr (M == 15) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
            if constexpr (N == 14) return ptr_t<decltype(p15)>{&p15};
         }
         else if constexpr (M == 14) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
            if constexpr (N == 13) return ptr_t<decltype(p14)>{&p14};
         }
         else if constexpr (M == 13) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
            if constexpr (N == 12) return ptr_t<decltype(p13)>{&p13};
         }
         else if constexpr (M == 12) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
            if constexpr (N == 11) return ptr_t<decltype(p12)>{&p12};
         }
         else if constexpr (M == 11) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
            if constexpr (N == 10) return ptr_t<decltype(p11)>{&p11};
         }
         else if constexpr (M == 10) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
            if constexpr (N == 9) return ptr_t<decltype(p10)>{&p10};
         }
         else if constexpr (M == 9) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8, p9] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
            if constexpr (N == 8) return ptr_t<decltype(p9)>{&p9};
         }
         else if constexpr (M == 8) {
            auto&& [p1, p2, p3, p4, p5, p6, p7, p8] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
            if constexpr (N == 7) return ptr_t<decltype(p8)>{&p8};
         }
         else if constexpr (M == 7) {
            auto&& [p1, p2, p3, p4, p5, p6, p7] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
            if constexpr (N == 6) return ptr_t<decltype(p7)>{&p7};
         }
         else if constexpr (M == 6) {
            auto&& [p1, p2, p3, p4, p5, p6] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
            if constexpr (N == 5) return ptr_t<decltype(p6)>{&p6};
         }
         else if constexpr (M == 5) {
            auto&& [p1, p2, p3, p4, p5] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
            if constexpr (N == 4) return ptr_t<decltype(p5)>{&p5};
         }
         else if constexpr (M == 4) {
            auto&& [p1, p2, p3, p4] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
            if constexpr (N == 3) return ptr_t<decltype(p4)>{&p4};
         }
         else if constexpr (M == 3) {
            auto&& [p1, p2, p3] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
            if constexpr (N == 2) return ptr_t<decltype(p3)>{&p3};
         }
         else if constexpr (M == 2) {
            auto&& [p1, p2] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
            if constexpr (N == 1) return ptr_t<decltype(p2)>{&p2};
         }
         else if constexpr (M == 1) {
            auto&& [p1] = t;
            // structure bindings is not constexpr :/
            if constexpr (N == 0) return ptr_t<decltype(p1)>{&p1};
         }
      }
   }
}
