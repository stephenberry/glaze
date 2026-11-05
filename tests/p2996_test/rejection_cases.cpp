// Each GLZ_P2996_REJECT_* case must fail to compile. The ctest entry passes only when the guard's
// message appears in the compiler output (see CMakeLists.txt).

#include <string>

#include "glaze/glaze.hpp"

#if defined(GLZ_P2996_REJECT_REPEATED_BASE)
// A base that holds members, inherited twice non-virtually. Its second subobject cannot be named
// through the derived type, so the count refuses rather than hand out a number that leaves one of the
// two subobjects out.
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
   constexpr auto names = glz::member_names<Diamond>;
   static_assert(names.size() == 4);
   return int(names.size());
}

#elif defined(GLZ_P2996_REJECT_KEYED_LOOKUP) || defined(GLZ_P2996_REJECT_KEYED_LOOKUP_BEVE) || \
   defined(GLZ_P2996_REJECT_KEYED_WRITE)
// Two members of a hierarchy answer to one name, so the keyed reader has no lookup it could build: the
// key hash cannot be told apart for the two of them. The names are still reflected, which is why the
// refusal is at the keyed reader and not in `member_names`. The same guard sits in the shared hash
// lookup that the keyed formats use, so JSON and BEVE are both built here to pin that the refusal is
// not a JSON-only accident.
struct HasId1
{
   int id{1};
};
struct HasId2
{
   int id{2};
};
struct Both : HasId1, HasId2
{
   int z{3};
};

int main()
{
   Both obj{};
   std::string buffer = R"({"id":1,"z":2})";
#if defined(GLZ_P2996_REJECT_KEYED_LOOKUP)
   const auto ec = glz::read_json(obj, buffer);
#elif defined(GLZ_P2996_REJECT_KEYED_LOOKUP_BEVE)
   const auto ec = glz::read_beve(obj, buffer);
#else
   const auto ec = glz::write_json(obj, buffer);
#endif
   return int(bool(ec));
}

#elif defined(GLZ_P2996_REJECT_MODIFY_NAME)
// A `modify` entry with a key but no member pointer names its member by name alone, and a member that
// hides a base member gives that name two answers. The lambda is what keeps the entry pointerless: a
// key plus a payload that is not a member pointer is exactly the shape the guard refuses.
struct ModNameBase
{
   int x{1};
};
struct ModNameDerived : ModNameBase
{
   int x{2};
};

template <>
struct glz::meta<ModNameDerived>
{
   static constexpr auto modify = glz::object("x", [](auto& self) -> auto& { return self.x; });
};

int main()
{
   ModNameDerived obj{};
   std::string buffer{};
   const auto ec = glz::write_json(obj, buffer);
   return int(bool(ec));
}

#else
int main() { return 0; }
#endif
