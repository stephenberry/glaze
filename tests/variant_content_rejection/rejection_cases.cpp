// Each GLZ_REJECT_CASE_* below must FAIL to compile. The build target for each case is driven by a
// ctest entry, so a guard that stops firing turns into a failing test.

#include <array>
#include <string_view>
#include <variant>

#include "glaze/glaze.hpp"

struct a_t
{
   int a{};
};

struct b_t
{
   int b{};
};

using two_t = std::variant<a_t, b_t>;
using three_t = std::variant<a_t, b_t, int>;

#if defined(GLZ_REJECT_CASE_TOO_FEW)
using tested_t = two_t;
template <>
struct glz::meta<tested_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::array ids{"a", "b"};
   static constexpr std::array content{"a"};
};
#elif defined(GLZ_REJECT_CASE_TOO_MANY)
using tested_t = two_t;
template <>
struct glz::meta<tested_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::array ids{"a", "b"};
   static constexpr std::array content{"a", "b", "c"};
};
#elif defined(GLZ_REJECT_CASE_DEFAULT_ALTERNATIVE)
using tested_t = three_t;
template <>
struct glz::meta<tested_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::array ids{"a", "b"};
   static constexpr std::array content{"a", "b"};
};
#elif defined(GLZ_REJECT_CASE_EMPTY_KEY)
using tested_t = two_t;
template <>
struct glz::meta<tested_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::array ids{"a", "b"};
   static constexpr std::array content{"a", ""};
};
#elif defined(GLZ_REJECT_CASE_KEY_IS_TAG)
using tested_t = two_t;
template <>
struct glz::meta<tested_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::array ids{"a", "b"};
   static constexpr std::array content{"a", "t"};
};
#elif defined(GLZ_REJECT_CASE_NOT_STRINGS)
using tested_t = two_t;
template <>
struct glz::meta<tested_t>
{
   static constexpr std::string_view tag = "t";
   static constexpr std::array ids{"a", "b"};
   static constexpr std::array content{1, 2};
};
#elif defined(GLZ_REJECT_CASE_WITHOUT_TAG)
using tested_t = two_t;
template <>
struct glz::meta<tested_t>
{
   static constexpr std::array ids{"a", "b"};
   static constexpr std::array content{"a", "b"};
};
#endif

int main()
{
   tested_t value{};
   std::string buffer{};
   (void)glz::write_json(value, buffer);
   return 0;
}
