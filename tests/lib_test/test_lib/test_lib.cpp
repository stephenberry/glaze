// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/api/impl.hpp"

#include "../interface.hpp"

glz::iface_fn glz_iface() noexcept
{
   return glz::make_iface<my_api>();
}
