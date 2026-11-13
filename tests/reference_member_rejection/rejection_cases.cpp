// Each case below must FAIL to compile, with the message its ctest entry expects.
//
// A member name comes from the address of the member, and a reference member has no address of its own. Before the
// guard a non-const reference made the struct reflect as empty: writing produced {} and returned success.

#include <string>

#include "glaze/json.hpp"

#if defined(GLZ_REJECT_HUGE_ARRAY)
struct holds_huge_array
{
   char buffer[65536]{};
};

int main()
{
   holds_huge_array value{};
   std::string buffer{};
   [[maybe_unused]] auto ec = glz::write_json(value, buffer);
   return 0;
}
#else
struct holds_reference
{
   int a{};
#if defined(GLZ_REJECT_LVALUE)
   int& r;
#elif defined(GLZ_REJECT_CONST_LVALUE)
   const int& r;
#endif
};

int main()
{
   int x{};
   holds_reference value{1, x};
   std::string buffer{};
   [[maybe_unused]] auto ec = glz::write_json(value, buffer);
   return 0;
}
#endif
