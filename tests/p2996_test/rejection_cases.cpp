// Each GLZ_P2996_REJECT_* case must fail to compile. The ctest entry passes only when the guard's
// message appears in the compiler output (see CMakeLists.txt).

#include <string>

#include "glaze/glaze.hpp"

#if defined(GLZ_P2996_REJECT_REPEATED_BASE)
// A base with members inherited twice non-virtually
struct Root
{
   int root{1};
};
struct Left : Root
{
   int left{2};
};
struct Right : Root
{
   int right{3};
};
struct Diamond : Left, Right
{
   int bottom{4};
};

int main()
{
   std::string buffer{};
   return int(bool(glz::write_json(Diamond{}, buffer)));
}

#elif defined(GLZ_P2996_REJECT_DUPLICATE_NAME)
// A member that hides a base member
struct Base
{
   int x{1};
};
struct Derived : Base
{
   int x{2};
};

int main()
{
   std::string buffer{};
   return int(bool(glz::write_json(Derived{}, buffer)));
}

#else
int main() { return 0; }
#endif
