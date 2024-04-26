// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/util/any.hpp"

namespace glz
{
   template <class Spec>
   inline constexpr auto fn_tuple()
   {
      return map_tuple(
         [](auto& v) {
            using mem_fn_t = std::decay_t<decltype(get<1>(v))>;
            return typename function_signature<mem_fn_t>::type{};
         },
         meta_v<Spec>);
   }

   template <class Spec>
   using fn_variant = typename detail::tuple_variant<decltype(fn_tuple<Spec>())>::type;

   template <class Spec, size_t... I>
   inline constexpr auto make_mem_fn_wrapper_map_impl(std::index_sequence<I...>)
   {
      constexpr auto N = glz::tuple_size_v<meta_t<Spec>>;
      return detail::normal_map<sv, fn_variant<Spec>, N>({std::make_pair<sv, fn_variant<Spec>>(
         sv(get<0>(get<I>(meta_v<Spec>))), get_argument<get<1>(get<I>(meta_v<Spec>))>())...});
   }

   template <class Spec>
   inline constexpr auto make_mem_fn_wrapper_map()
   {
      constexpr auto N = glz::tuple_size_v<meta_t<Spec>>;
      return make_mem_fn_wrapper_map_impl<Spec>(std::make_index_sequence<N>{});
   }

   union void_union {
      void* ptr;
      void (*fptr)();
   };

   template <class = void, std::size_t... Is>
   constexpr auto indexer(std::index_sequence<Is...>) noexcept
   {
      return []<class F>(F&& f) noexcept -> decltype(auto) {
         return std::forward<F>(f)(std::integral_constant<std::size_t, Is>{}...);
      };
   }

   // takes a number N
   // returns a function object that, when passed a function object f
   // passes it compile-time values from 0 to N-1 inclusive.
   template <size_t N>
   constexpr auto indexer() noexcept
   {
      return indexer(std::make_index_sequence<N>{});
   }

   template <size_t N, class Func>
   constexpr auto for_each_poly(Func&& f) noexcept
   {
      return indexer<N>()([&](auto&&... i) noexcept { (std::forward<Func>(f)(i), ...); });
   }

   template <class Spec>
   struct poly
   {
      template <class T>
      poly(T&& v) : anything(std::forward<T>(v))
      {
         raw_ptr = anything.data();

         static constexpr auto N = glz::tuple_size_v<meta_t<Spec>>;
         static constexpr auto frozen_map = detail::make_map<std::remove_pointer_t<T>, false>();

         for_each_poly<N>([&](auto I) {
            static constexpr auto spec = meta_v<Spec>;

            static constexpr sv key = glz::get<0>(glz::get<I>(spec));
            static constexpr auto member_it = frozen_map.find(key);
            if constexpr (member_it != frozen_map.end()) {
               static constexpr auto index = cmap.index(key);
               static constexpr auto member_ptr = std::get<member_it->second.index()>(member_it->second);

               using SpecElement = std::decay_t<decltype(glz::get<1>(glz::get<I>(spec)))>;

               using X = std::decay_t<decltype(member_ptr)>;
               if constexpr (std::is_member_object_pointer_v<X>) {
                  using SpecMember = typename member_value<SpecElement>::type;
                  using XMember = typename member_value<X>::type;
                  if constexpr (!std::same_as<SpecMember, XMember>) {
                     static_assert(false_v<SpecMember, XMember>, "spec and type do not match for member object");
                  }

                  if constexpr (std::is_pointer_v<T>) {
                     map[index].ptr = &((*static_cast<T>(raw_ptr)).*member_ptr);
                  }
                  else {
                     map[index].ptr = &((*static_cast<T*>(raw_ptr)).*member_ptr);
                  }
               }
               else {
                  using SpecF = typename std_function_signature<SpecElement>::type;
                  using F = typename std_function_signature<std::decay_t<decltype(member_ptr)>>::type;

                  if constexpr (!std::same_as<SpecF, F>) {
                     static_assert(false_v<SpecF, F>, "spec and type function signatures do not match");
                  }

                  map[index].fptr = reinterpret_cast<void (*)()>(arguments<member_ptr, X>::op);
               }
            }
            else {
               static_assert(false_v<decltype(member_it)>, "spec key does not match meta");
            }
         });
      }

      glz::any anything;
      static constexpr auto cmap = make_mem_fn_wrapper_map<Spec>();
      std::array<void_union, cmap.size()> map;

      template <string_literal name, class... Args>
      decltype(auto) call(Args&&... args)
      {
         static constexpr sv key = chars<name>;
         static constexpr auto member_it = cmap.find(key);

         if constexpr (member_it != cmap.end()) {
            static constexpr auto index = cmap.index(key);
            using X = std::decay_t<decltype(std::get<member_it->second.index()>(cmap.find(key)->second))>;
            auto* v = reinterpret_cast<X>(map[index].fptr);
            using V = std::decay_t<decltype(v)>;
            if constexpr (std::invocable<V, void*, Args...>) {
               return v(raw_ptr, std::forward<Args>(args)...);
            }
            else {
               static_assert(false_v<decltype(name)>, "call: invalid arguments to call");
            }
         }
         else {
            static_assert(false_v<decltype(name)>, "call: invalid name");
         }
      }

      template <string_literal name>
      decltype(auto) get()
      {
         static constexpr sv key = chars<name>;
         static constexpr auto member_it = cmap.find(key);

         if constexpr (member_it != cmap.end()) {
            static constexpr auto index = cmap.index(key);
            using X = decltype(std::get<member_it->second.index()>(cmap.find(key)->second));
            auto* v = reinterpret_cast<X>(map[index].ptr);
            return *v;
         }
         else {
            static_assert(false_v<decltype(name)>, "call: invalid name");
         }
      }

     private:
      void* raw_ptr;
   };
}
