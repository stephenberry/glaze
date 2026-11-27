// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Reversed, 10, 0
// TRACE: {"omicron"}
//
// The member with no writer is declared second and listed first in glz::meta. The report has to name
// the member by the index and key the meta gives -- index 0, key "omicron" -- because that is where
// the writer reaches it; a report built from declaration order names the wrong member.
#include "shapes.hpp"

#include <glaze/json.hpp>

int main()
{
   Reversed v{};
   std::string b;
   (void)glz::write_json(v, b);
}
