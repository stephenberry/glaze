// Each GLZ_P2996_REJECT_* case must fail to compile. The ctest entry passes only when the guard's
// message appears in the compiler output (see CMakeLists.txt).

#include <string>

#include "glaze/glaze.hpp"

#if defined(GLZ_P2996_REJECT_DUPLICATE_NAME)
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

#elif defined(GLZ_P2996_REJECT_BASE_WITH_META)
// A base whose keys are shaped by its own glz::meta
struct Base
{
   int x{1};
};
struct Derived : Base
{
   int own{2};
};

template <>
struct glz::meta<Base>
{
   using T = Base;
   static constexpr auto value = glz::object("renamed", &T::x);
};

int main()
{
   std::string buffer{};
   return int(bool(glz::write_json(Derived{}, buffer)));
}

#else
int main() { return 0; }
#endif
