// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/api/impl.hpp"

#include "../interface.hpp"

DLL_EXPORT glaze::interface* create_api() noexcept
{
   return new glaze::interface{{"my_api", glaze::make_api<my_api>}};
}
