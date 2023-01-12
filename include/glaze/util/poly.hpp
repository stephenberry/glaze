// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <any>

#include "glaze/core/common.hpp"
#include "glaze/util/any.hpp"

namespace glz
{
   template <class Spec>
   inline constexpr auto fn_tuple() {
      return map_tuple([](auto& v){
         using mem_fn_t = std::decay_t<decltype(tuplet::get<1>(v))>;
         return typename function_signature<mem_fn_t>::type{};
      }, meta_v<Spec>);
   }
   
   template <class Spec>
   using fn_variant = typename detail::tuple_variant<decltype(fn_tuple<Spec>())>::type;
   
   template <class Spec, size_t... I>
   inline constexpr auto make_mem_fn_wrapper_map_impl(std::index_sequence<I...>) {
      constexpr auto N = std::tuple_size_v<meta_t<Spec>>;
      return frozen::make_unordered_map<frozen::string, fn_variant<Spec>, N>({
         std::make_pair<frozen::string, fn_variant<Spec>>(tuplet::get<0>(tuplet::get<I>(meta_v<Spec>)), get_argument<tuplet::get<1>(tuplet::get<I>(meta_v<Spec>))>())...
      });
   }
   
   template <class Spec>
   inline constexpr auto make_mem_fn_wrapper_map() {
      constexpr auto N = std::tuple_size_v<meta_t<Spec>>;
      return make_mem_fn_wrapper_map_impl<Spec>(std::make_index_sequence<N>{});
   }
   
   union void_union
   {
      void* ptr;
      void(*fptr)();
   };
   
   template <class Spec>
   struct poly
   {
      template <class T>
      poly(T&& v) : anything(std::forward<T>(v)) {
         
         static constexpr auto N = std::tuple_size_v<meta_t<Spec>>;
         
         for_each<N>([&](auto I) {
            // TODO: move "m" and "frozen_map" out of for_each when MSVC 2019 is fixed or deprecated
            static constexpr auto m = meta_v<Spec>;
            static constexpr auto frozen_map = detail::make_map<T, false>();
            
            static constexpr sv key = tuplet::get<0>(tuplet::get<I>(m));
            static constexpr auto member_it = frozen_map.find(key);
            if constexpr (member_it != frozen_map.end()) {
               /*if constexpr (std::holds_alternative<decltype(tuplet::get<1>(tuplet::get<I>(m)))>(member_it->second)) {
               }
               else {
                  throw std::runtime_error("invalid");
               }*/
               
               static constexpr auto member_ptr = std::get<member_it->second.index()>(member_it->second);
               static constexpr auto index = cmap.table_lookup(key);
               using X = std::decay_t<decltype(member_ptr)>;
               if constexpr (std::is_member_object_pointer_v<X>) {
                  map[index].ptr = &((*static_cast<T*>(anything.data())).*member_ptr);
               }
               else {
                  map[index].fptr = reinterpret_cast<void(*)()>(arguments<member_ptr, X>::op);
               }
            }
            else {
               throw std::runtime_error("spec key does not match meta");
            }
         });
      }
      
      glz::any anything;
      static constexpr auto cmap = make_mem_fn_wrapper_map<Spec>();
      //std::decay_t<decltype(cmap)> map = cmap;
      std::array<void_union, cmap.size()> map;
      
      template <string_literal name, class... Args>
      decltype(auto) call(Args&&... args) {
         static constexpr sv key = chars<name>;
         static constexpr auto member_it = cmap.find(key);
         
         if constexpr (member_it != cmap.end()) {
            static constexpr auto index = cmap.table_lookup(key);
            //auto& v = std::get<member_it->second.index()>(map.unsafe_value_access(index));
            using X = std::decay_t<decltype(std::get<member_it->second.index()>(cmap.at(key)))>;
            auto* v = reinterpret_cast<X>(map[index].fptr);
            using V = std::decay_t<decltype(v)>;
            if constexpr (std::is_invocable_v<V, void*, Args...>) {
               return v(anything.data(), std::forward<Args>(args)...);
            }
            else {
               throw std::runtime_error("call: invalid arguments to call");
            }
         }
         else {
            throw std::runtime_error("call: invalid name");
         }
      }
      
      template <string_literal name>
      decltype(auto) get() {
         static constexpr sv key = chars<name>;
         static constexpr auto member_it = cmap.find(key);
         
         if constexpr (member_it != cmap.end()) {
            static constexpr auto index = cmap.table_lookup(key);
            using X = decltype(std::get<member_it->second.index()>(cmap.at(key)));
            auto* v = reinterpret_cast<X>(map[index].ptr);
            return *v;
         }
         else {
            throw std::runtime_error("call: invalid name");
         }
      }
   };
}
