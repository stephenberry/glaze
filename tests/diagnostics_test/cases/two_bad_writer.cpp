// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<TwoBad, 10, 0
// TRACE: {"first_bad"}
//
// Two members of this struct have no writer for this format, at index 0 and index 2, and the report
// names the first one. The index is only observable in the trace, so this case is what pins the
// selection: every other shape here has exactly one unsupported member, and a fold that kept the
// last index instead of the first would satisfy all of them.
#include "shapes.hpp"

#include <glaze/json.hpp>

int main()
{
   TwoBad v{};
   std::string b;
   (void)glz::write_json(v, b);
}
