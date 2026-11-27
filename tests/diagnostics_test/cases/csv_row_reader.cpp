// EXPECT: one of this object's members has no reader for this format
// TRACE: unsupported_reader<Inner, 10000, 1
// TRACE: {"o"}
//
// The row-wise reader fills the row type's members from its own loop and never instantiates
// `from<CSV, U>::op`, so the row reader carries its own check.
#include "shapes.hpp"

#include <glaze/csv.hpp>

int main()
{
   std::vector<Inner> rows{};
   std::string s = "a,o\n1,x\n";
   (void)glz::read_csv(rows, s);
}
