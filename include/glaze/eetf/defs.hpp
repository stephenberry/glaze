#pragma once

#include <glaze/core/common.hpp>

#include "types.hpp"

namespace glz
{
   // This is magic number not exposed in erlang sources.
   // If this will be changed in future - need to not forget to change it here.
   constexpr uint8_t version_magic = 131u;

   constexpr auto max_atom_len = 256u;
   constexpr auto max_utf8_atom_len = 255u * 4 + 1;

} // namespace glz
