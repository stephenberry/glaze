// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/rpc/read_and_commit.hpp"
// glz:header std=<concepts>
// glz:header std=<cstddef>
// glz:header std=<optional>
// glz:header std=<tuple>
// glz:header std=<type_traits>
// glz:header std=<utility>
// glz:header std=<variant>
export module glaze.rpc.read_and_commit;

import std;

import glaze.concepts.container_concepts;
import glaze.core.common;
import glaze.core.reflect;
import glaze.util.type_traits;

namespace glz::detail
{
   template <class... Ts>
   struct type_list
   {};

   template <class T, class Visited>
   struct deep_copy_constructible;

   template <class T, class Visited>
   inline constexpr bool deep_copy_constructible_v = deep_copy_constructible<std::remove_cv_t<T>, Visited>::value;

   template <class Visited, class... Ts>
   inline constexpr bool all_deep_copy_constructible_v = (deep_copy_constructible_v<Ts, Visited> && ...);

   // std::is_copy_constructible_v is true for a container of move-only elements (std::vector<std::unique_ptr<T>>),
   // because the container declares its copy constructor without constraining it on the element type, and so for
   // any wrapper or aggregate that holds one. Copying such a type fails to compile, so the check has to look inside:
   // into the elements of a range, the alternatives of a wrapper, and the reflected members of an object. A type that
   // is already being checked further up is assumed copyable, so a recursive type (a node holding a std::vector of
   // nodes) is decided by its other members.
   template <class T, class... Visited>
   struct deep_copy_constructible<T, type_list<Visited...>>
   {
      using visited = type_list<Visited..., T>;

      static consteval bool check()
      {
         if constexpr ((std::same_as<T, Visited> || ...)) {
            return true;
         }
         else if constexpr (!std::is_copy_constructible_v<T>) {
            return false;
         }
         else if constexpr (is_specialization_v<T, std::optional>) {
            return deep_copy_constructible_v<typename T::value_type, visited>;
         }
         else if constexpr (is_specialization_v<T, std::variant> || is_specialization_v<T, std::tuple>) {
            return []<template <class...> class Tmpl, class... Ts>(std::type_identity<Tmpl<Ts...>>) {
               return all_deep_copy_constructible_v<visited, Ts...>;
            }(std::type_identity<T>{});
         }
         else if constexpr (pair_t<T>) {
            return all_deep_copy_constructible_v<visited, typename T::first_type, typename T::second_type>;
         }
         else if constexpr (glaze_object_t<T> || reflectable<T>) {
            return []<std::size_t... I>(std::index_sequence<I...>) {
               return all_deep_copy_constructible_v<visited, field_t<T, I>...>;
            }(std::make_index_sequence<reflect<T>::size>{});
         }
         else if constexpr (range<T> && requires { typename T::value_type; }) {
            return deep_copy_constructible_v<typename T::value_type, visited>;
         }
         else {
            return true;
         }
      }

      static constexpr bool value = check();
   };

   // A registry write reads the request into the registered value, and a read that fails part way (a parse error, a
   // read_constraint, a self_constraint) has already assigned every member read before the failure. The caller is
   // told the write was rejected, so none of it may stick: read into a copy and move it back only on success. The
   // move replaces the storage of every member, so pointers and views into the value do not survive a successful
   // write. A value that cannot be copied (std::atomic, a type holding a mutex or a container of std::unique_ptr) is
   // read in place, and a rejected write can then leave it partly updated.
   export template <class T>
   inline constexpr bool stageable_v = deep_copy_constructible_v<T, type_list<>> && std::is_move_assignable_v<T>;

   export template <class T, class ReadInto>
   bool read_and_commit(T& value, ReadInto&& read_into)
   {
      if constexpr (stageable_v<T>) {
         T staged = value;
         if (!read_into(staged)) {
            return false;
         }
         value = std::move(staged);
         return true;
      }
      else {
         return read_into(value);
      }
   }
}
