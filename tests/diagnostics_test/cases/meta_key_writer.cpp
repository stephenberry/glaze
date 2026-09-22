// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Renamed, 10, 1
// TRACE: {"omicron"}
//
// The key the diagnostic reports has to be the one the output uses, not the member's declared name.
#include "shapes.hpp"

#include <glaze/json.hpp>

int main()
{
   Renamed v{};
   std::string b;
   (void)glz::write_json(v, b);
}
