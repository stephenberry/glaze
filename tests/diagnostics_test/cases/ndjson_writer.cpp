// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Outer, 10, 1
//
// NDJSON writes each record through the JSON writer, so the report comes from JSON and carries the
// JSON format id (10), not an NDJSON id of its own. docs/compile-time-diagnostics.md states this;
// nothing else in these cases reaches NDJSON.
#include "shapes.hpp"

#include <glaze/json/ndjson.hpp>

int main()
{
   std::vector<Outer> v{};
   std::string b;
   (void)glz::write_ndjson(v, b);
}
