// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <string>

#include "glaze/network/core.hpp"
#include "glaze/reflection/enum_macro.hpp"

namespace glz
{   
   GLZ_ENUM(poll_op, read, write, read_write);
   
   GLZ_ENUM(poll_status, event, timeout, error, closed);
}
