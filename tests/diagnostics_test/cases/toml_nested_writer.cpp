// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Inner, 400, 1
// TRACE: {"o"}
//
// TOML writes a nested struct as a table through `write_toml_object_with_path`, not through
// `to<TOML, Inner>::op`, so the check has to be in the table writer itself. With only the entry
// point checked, this struct failed from inside the table writer with the old incomplete-type error.
// No other case here reaches a nested object in a format other than JSON.
#include "shapes.hpp"

#include <glaze/toml.hpp>

int main()
{
   Nested v{};
   std::string b;
   (void)glz::write_toml(v, b);
}
