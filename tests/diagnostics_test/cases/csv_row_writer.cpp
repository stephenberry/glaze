// EXPECT: one of this object's members has no writer for this format
// TRACE: unsupported_writer<Inner, 10000, 1
// TRACE: {"o"}
//
// A vector of objects is written row by row, each field dispatched straight from the loop in
// `to<CSV, std::vector<U>>::op`; `to<CSV, U>::op` never runs for the row type, so the row writer
// carries its own check.
#include "shapes.hpp"

#include <glaze/csv.hpp>

int main()
{
   std::vector<Inner> rows{};
   (void)glz::write_csv(rows);
}
