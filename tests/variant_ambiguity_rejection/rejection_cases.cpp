// Each GLZ_REJECT_CASE_* below must FAIL to compile. The build target for each case is driven by a
// ctest entry, so a guard that stops firing turns into a failing test rather than into a read that
// silently produces the wrong alternative.

#include <array>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "glaze/glaze.hpp"

struct a_t
{
   int i{};
   std::string s{};
};

struct b_t
{
   int i{};
   std::string s{};
};

struct reversed_t
{
   std::string s{};
   int i{};
};

struct renamed_t
{
   int number{};
   std::string text{};
};

template <>
struct glz::meta<renamed_t>
{
   using T = renamed_t;
   static constexpr auto value = glz::object("i", &T::number, "s", &T::text);
};

struct other_t
{
   double d{};
};

#if defined(GLZ_REJECT_CASE_IDENTICAL)
using tested_t = std::variant<a_t, b_t>;
#elif defined(GLZ_REJECT_CASE_REORDERED)
using tested_t = std::variant<a_t, reversed_t>;
#elif defined(GLZ_REJECT_CASE_RENAMED_BY_META)
using tested_t = std::variant<a_t, renamed_t>;
#elif defined(GLZ_REJECT_CASE_DUPLICATE_TYPE)
using tested_t = std::variant<a_t, a_t>;
#elif defined(GLZ_REJECT_CASE_THROUGH_POINTER)
using tested_t = std::variant<a_t, std::unique_ptr<b_t> >;
#elif defined(GLZ_REJECT_CASE_IDS_WITHOUT_TAG)
using tested_t = std::variant<a_t, b_t>;
template <>
struct glz::meta<tested_t>
{
   static constexpr auto ids = std::array{"a", "b"};
};
#elif defined(GLZ_REJECT_CASE_AMONG_OTHER_ALTERNATIVES)
using tested_t = std::variant<int, other_t, a_t, std::string, b_t>;
#elif defined(GLZ_REJECT_CASE_BESIDE_A_MAP)
using tested_t = std::variant<a_t, b_t, std::map<std::string, int> >;
#elif defined(GLZ_REJECT_CASE_MEMBER_OF_STRUCT)
struct tested_t
{
   std::variant<a_t, b_t> v{};
};
#endif

int main()
{
   tested_t value{};
   return int(glz::read_json(value, std::string_view{"{}"}).ec);
}
