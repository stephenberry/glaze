#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "cmp.hpp"
#include "tags.hpp"

namespace glz
{
   namespace eetf
   {
      struct tag_atom
      {};

      struct tag_string
      {};

      template <typename Tag>
      struct tagged_string : std::string
      {
         using tag = Tag;
      };

      using atom = tagged_string<tag_atom>;
      constexpr atom operator""_atom(const char* str, std::size_t sz)
      {
         // TODO check if valid atom
         return atom(std::string(str, sz));
      }

      template <typename Tag>
      [[nodiscard]] bool is_atom(const Tag& tag)
      {
         return cmp::is<in, eetf_tags::ATOM, eetf_tags::SMALL_ATOM, eetf_tags::ATOM_UTF8, eetf_tags::SMALL_ATOM_UTF8>(
            tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_integer(const Tag& tag)
      {
         return cmp::is<in, eetf_tags::INTEGER, eetf_tags::SMALL_INTEGER, eetf_tags::SMALL_BIG, eetf_tags::LARGE_BIG>(
            tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_floating_point(const Tag& tag)
      {
         return cmp::is<in, eetf_tags::FLOAT, eetf_tags::FLOAT_NEW>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_string(const Tag& tag)
      {
         return cmp::is<in, eetf_tags::STRING, eetf_tags::NIL>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_tuple(const Tag& tag)
      {
         return cmp::is<in, eetf_tags::SMALL_TUPLE, eetf_tags::LARGE_TUPLE>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_list(const Tag& tag)
      {
         return cmp::is<in, eetf_tags::LIST, eetf_tags::STRING, eetf_tags::NIL>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_map(const Tag& tag)
      {
         return cmp::is<in, eetf_tags::MAP>(tag);
      }

   } // namespace eetf

   template <class T>
   concept atom_t = string_t<T> && std::same_as<typename T::tag, eetf::tag_atom>;
} // namespace glz
