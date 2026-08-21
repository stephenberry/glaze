// Glaze Library
// For the license information refer to glaze.hpp

// Tests that a concept-constrained glz::meta specialization owns every type it matches.
// Glaze names built-in and compound types through glz::name_meta rather than through partial
// specializations of glz::meta, so a user specialization written against a concept (rather than an
// exact type) is never ambiguous with Glaze's own. See issue #2775.

#include <concepts>
#include <memory>
#include <string>
#include <vector>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

// glz::meta must carry no Glaze specialization that a user's constrained specialization could tie
// with. This has to go through a concept: glz::meta's primary template is only declared, so a bare
// requires-expression on glz::meta<T>::name at namespace scope is a hard error rather than false.
template <class T>
concept meta_named = requires { glz::meta<T>::name; };

static_assert(!meta_named<int32_t>);
static_assert(!meta_named<const int32_t>);
static_assert(!meta_named<int32_t&>);
static_assert(!meta_named<int32_t*>);
static_assert(!meta_named<std::string>);
static_assert(!meta_named<std::string_view>);
static_assert(!meta_named<std::vector<int32_t>>);
static_assert(!meta_named<std::shared_ptr<int32_t>>);

struct shape_base
{
   int id = 0;
};

struct circle : shape_base
{
   circle() : shape_base{.id = 1} {}
};

template <std::derived_from<shape_base> T>
struct glz::meta<T>
{
   static constexpr auto write_id = [](const T& t) { return t.id; };
   static constexpr auto value = custom<skip{}, write_id>;
};

struct shape_source
{
   std::shared_ptr<const shape_base> get() const { return std::make_shared<circle>(); }
   std::vector<std::shared_ptr<const shape_base>> get_all() const
   {
      return {std::make_shared<circle>(), std::make_shared<circle>()};
   }
};

template <>
struct glz::meta<shape_source>
{
   static constexpr auto value =
      object("value", custom<skip{}, &shape_source::get>, "values", custom<skip{}, &shape_source::get_all>);
};

// A second, disjoint hierarchy: a constrained specialization that reflects members, to check that a
// concept-constrained meta drives reading as well as writing.
struct record_base
{
   int count = 0;
};

struct counter : record_base
{};

template <std::derived_from<record_base> T>
struct glz::meta<T>
{
   static constexpr auto value = object("count", &T::count);
};

// A user name for a type Glaze also names must win over glz::name_meta.
struct tag_t
{
   int v = 0;
};

template <>
struct glz::meta<std::vector<tag_t>>
{
   static constexpr std::string_view name = "tag_list";
};

suite constrained_meta_tests = [] {
   // std::derived_from also accepts cv-qualified types, so the constrained specialization matches
   // `const circle`. Naming `const T` through glz::meta would have made this ambiguous.
   "constrained meta claims cv-qualified types"_test = [] {
      static_assert(requires { glz::meta<circle>::value; });
      static_assert(requires { glz::meta<const circle>::value; });
      // GCC reports the ambiguity here rather than at the meta<const circle> use above: clang treats
      // it as a substitution failure and silently falls back to the compiler's spelling.
      static_assert(glz::name_v<const circle> == "const circle");
   };

   "writes through pointers to const base"_test = [] {
      std::string buffer{};
      expect(not glz::write_json(shape_source{}, buffer));
      expect(buffer == R"({"value":1,"values":[1,1]})") << buffer;
   };

   "constrained meta round trips"_test = [] {
      std::string buffer{};
      expect(not glz::write_json(counter{{7}}, buffer));
      expect(buffer == R"({"count":7})") << buffer;

      counter parsed{};
      expect(not glz::read_json(parsed, buffer));
      expect(parsed.count == 7);
   };

   // Glaze's own names still apply to types the user has not claimed, and give way to one the user
   // supplies through glz::meta.
   "name resolution order"_test = [] {
      static_assert(glz::name_v<int32_t> == "int32_t");
      static_assert(glz::name_v<const int32_t> == "const int32_t");
      static_assert(glz::name_v<int32_t&> == "int32_t&");
      static_assert(glz::name_v<int32_t*> == "int32_t*");
      static_assert(glz::name_v<int32_t* const> == "const int32_t*");
      static_assert(glz::name_v<std::vector<int32_t>> == "std::vector<int32_t>");
      static_assert(glz::name_v<std::string> == "std::string");
      static_assert(glz::name_v<std::vector<tag_t>> == "tag_list");
   };
};

int main() { return 0; }
