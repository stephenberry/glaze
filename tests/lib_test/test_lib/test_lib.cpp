// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/api/impl.hpp"

#include "../interface.hpp"

DLL_EXPORT glz::interface* create_api() noexcept
{
   return new glz::interface{{"my_api", glz::make_api<my_api>}};
}
