// Glaze Library
// For the license information refer to glaze.hpp

#include "../interface.hpp"
#include "glaze/api/impl.hpp"

glz::iface_fn glz_iface() noexcept { return glz::make_iface<my_api>(); }
