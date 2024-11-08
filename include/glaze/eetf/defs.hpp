#pragma once

#include "glaze/core/common.hpp"

#include "types.hpp"

namespace glz
{
   template <class T>
   concept atom_t = detail::string_t<T> && std::same_as<typename T::tag, eetf::tag_atom>;

   // template <class T>
   // concept erl_str_t = detail::string_t<T> && std::same_as<typename T::tag, eetf::tag_string>;

} // namespace glz
