// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/api/impl.hpp"

#include "../interface.hpp"

DLL_EXPORT std::shared_ptr<glz::iface> glaze_interface() noexcept
{
   return std::shared_ptr<glz::iface>{ new glz::iface{{"my_api", glz::make_api<my_api>}}, [](auto* ptr){ delete ptr; } };
}
