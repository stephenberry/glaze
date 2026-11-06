// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/feature_test.hpp"

#if GLZ_REFLECTION26

#include <array>
#include <concepts>
#include <meta>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "glaze/core/common.hpp"
#include "glaze/reflection/to_tuple.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   namespace detail
   {
      // A generated type only has the C++ member names, so a type whose keys are shaped by a glz::meta or a local
      // glaze struct (renames, modify, skip, ...) would silently read and write different keys.
      template <class T>
      concept has_glaze_customization = requires { sizeof(meta<T>); } || requires { typename T::glaze; };

      // std::optional cannot hold a reference or an array, a bit-field or an unnamed member has nothing Glaze could
      // address, and glz::skip and glz::hidden cannot be written from inside an optional. Base class members are not
      // visited by count_members or to_tie, so they could not be applied either.
      consteval bool has_optionalizable_members(std::meta::info type)
      {
         if (!std::meta::bases_of(type, reflection_access_ctx()).empty()) {
            return false;
         }
         for (const auto member : std::meta::nonstatic_data_members_of(type, reflection_access_ctx())) {
            const auto member_type = std::meta::type_of(member);
            if (!std::meta::has_identifier(member) || std::meta::is_bit_field(member) ||
                std::meta::is_reference_type(member_type) || std::meta::is_array_type(member_type) ||
                std::meta::is_const_type(member_type) || std::meta::is_volatile_type(member_type) ||
                std::meta::dealias(member_type) == ^^skip || std::meta::dealias(member_type) == ^^hidden) {
               return false;
            }
         }
         return true;
      }

      template <class T>
      concept optionalizable = std::same_as<T, std::remove_cv_t<T>> && reflectable<T> && std::is_aggregate_v<T> &&
                               !has_glaze_customization<T> && has_optionalizable_members(^^T);

      template <class T, bool Recursive>
      struct optionalized;

      template <class Member, bool Recursive>
      struct optionalized_member
      {
         using type = std::optional<Member>;
      };

      template <class Value, bool Recursive>
      struct optionalized_member<std::optional<Value>, Recursive>
      {
         using type = std::optional<Value>;
      };

      template <optionalizable Member>
      struct optionalized_member<Member, true>
      {
         using type = std::optional<typename optionalized<Member, true>::type>;
      };

      template <class Member, bool Recursive>
      using optionalized_member_t = typename optionalized_member<Member, Recursive>::type;

      // define_aggregate may only complete a class from a consteval block when no class or function parameter scope
      // encloses just one of the two ([expr.const.reflect]). That rules out completing a namespace scope template
      // specialization from here, but a member class declared right next to the block is allowed.
      template <class T, bool Recursive>
      struct optionalized
      {
         struct type;

         consteval
         {
            std::vector<std::meta::info> members;
            for (const auto member : std::meta::nonstatic_data_members_of(^^T, reflection_access_ctx())) {
               const std::array arguments{std::meta::type_of(member), std::meta::reflect_constant(Recursive)};
               const auto member_type = std::meta::dealias(std::meta::substitute(^^optionalized_member_t, arguments));
               members.push_back(std::meta::data_member_spec(member_type, {.name = std::meta::identifier_of(member)}));
            }
            std::meta::define_aggregate(^^type, members);
         }
      };
   }

   // T with every member wrapped in std::optional, for partial documents such as configuration overrides.
   // Members that are already std::optional are kept as they are and nested aggregates are optionalized too.
   template <class T>
      requires detail::optionalizable<T>
   using optionalize = typename detail::optionalized<T, true>::type;

   // As optionalize, except that a nested aggregate is wrapped whole and so replaced whole by apply_partial.
   template <class T>
      requires detail::optionalizable<T>
   using optionalize_shallow = typename detail::optionalized<T, false>::type;

   // Assigns every engaged member of a partial onto target and recurses into optionalized nested aggregates. A
   // disengaged member leaves target untouched, so a partial cannot reset a std::optional member of target to nullopt.
   template <class T, class Partial>
      requires(std::same_as<std::remove_cvref_t<Partial>, optionalize<T>> ||
               std::same_as<std::remove_cvref_t<Partial>, optionalize_shallow<T>>)
   constexpr void apply_partial(T& target, Partial&& partial)
   {
      const auto apply_member = [](auto& member, auto&& value) {
         using Member = std::remove_cvref_t<decltype(member)>;
         using Value = typename std::remove_cvref_t<decltype(value)>::value_type;
         if (value) {
            if constexpr (std::same_as<Value, Member> || is_specialization_v<Member, std::optional>) {
               member = *std::forward<decltype(value)>(value);
            }
            else {
               apply_partial(member, *std::forward<decltype(value)>(value));
            }
         }
      };
      [&]<size_t... I>(std::index_sequence<I...>) {
         (apply_member(detail::get_member_ref<I>(target),
                       std::forward_like<Partial>(detail::get_member_ref<I>(partial))),
          ...);
      }(std::make_index_sequence<detail::count_members<T>>{});
   }
}

#endif
