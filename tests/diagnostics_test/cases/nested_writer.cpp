// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Inner, 10, 1
//
// Nested is writable: every member of it has a writer, and Inner's writer is the one that reports.
// The diagnostic has to name Inner and the member inside it, not Nested.
#include "shapes.hpp"

#include <glaze/json.hpp>

int main()
{
   Nested v{};
   std::string b;
   (void)glz::write_json(v, b);
}
