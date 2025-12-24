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
         return cmp::is<in, eetf_tag::ATOM, eetf_tag::SMALL_ATOM, eetf_tag::ATOM_UTF8, eetf_tag::SMALL_ATOM_UTF8>(
            tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_integer(const Tag& tag)
      {
         return cmp::is<in, eetf_tag::INTEGER, eetf_tag::SMALL_INTEGER, eetf_tag::SMALL_BIG, eetf_tag::LARGE_BIG>(
            tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_floating_point(const Tag& tag)
      {
         return cmp::is<in, eetf_tag::FLOAT, eetf_tag::FLOAT_NEW>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_string(const Tag& tag)
      {
         return cmp::is<in, eetf_tag::STRING, eetf_tag::NIL>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_tuple(const Tag& tag)
      {
         return cmp::is<in, eetf_tag::SMALL_TUPLE, eetf_tag::LARGE_TUPLE>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_list(const Tag& tag)
      {
         return cmp::is<in, eetf_tag::LIST, eetf_tag::STRING, eetf_tag::NIL>(tag);
      }

      template <typename Tag>
      [[nodiscard]] bool is_map(const Tag& tag)
      {
         return cmp::is<in, eetf_tag::MAP>(tag);
      }

   } // namespace eetf

   template <class T>
   concept atom_t = string_t<T> && std::same_as<typename T::tag, eetf::tag_atom>;
} // namespace glz
