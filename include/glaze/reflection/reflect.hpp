// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>
#include <tuple>

namespace glz
{
   template <class T, std::size_t N>
   struct static_vector {
      constexpr auto push_back(const T& elem) { elems[size++] = elem; }
      constexpr auto& operator[](auto i) const { return elems[i]; }

      T elems[N + 1]{};
      std::size_t size{};
   };

   constexpr auto to_names(auto& out, auto, auto... args) {
      if constexpr (sizeof...(args) > 1) {
         out.push_back(std::get<2>(std::tuple{args...}));
      }
   }

   struct any_t {
      template <class T>
      constexpr operator T();
   };

   template <class T, class... Args> requires (std::is_aggregate_v<std::remove_cvref_t<T>>)
   consteval auto count_members() {
      if constexpr (requires { T{{Args{}}..., {any_t{}}}; } == false) {
         return sizeof...(Args);
      } else {
         return count_members<T, Args..., any_t>();
      }
   }

   template <class T>
   consteval auto member_names() {
      static constexpr auto N = count_members<T>();
      static_vector<std::string_view, N> v{};
      T t{};
      __builtin_dump_struct(&t, to_names, v);
      return v;
   }

   template <class T>
   constexpr auto to_tuple(T&& t) {
      static constexpr auto N = count_members<T>();
      if constexpr (N == 0) {
         return std::tuple{};
      } else if constexpr (N == 1) {
         auto&& [p] = t;
         return std::tuple{p};
      } else if constexpr (N == 2) {
         auto&& [p0, p1] = t;
         return std::tuple{p0, p1};
      } else if constexpr (N == 3) {
         auto&& [p0, p1, p2] = t;
         return std::tuple{p0, p1, p2};
      }
   }
}
