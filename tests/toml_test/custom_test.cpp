// Issue #2786: glz::custom<Read, Write> support for TOML.
//
// This lives in its own translation unit that includes only "glaze/toml.hpp". The custom
// read/write handlers are defined in "glaze/core/custom.hpp", and the bug being guarded here was
// that only the JSON headers pulled that in - so mixing this into a file that also includes
// "glaze/json.hpp" would hide the regression.

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "glaze/toml.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace i2786
{
   // Member function setter/getter that transform the value on the way through.
   struct transformed
   {
      uint64_t x{};
      std::string y{};
      std::array<uint32_t, 3> z{};

      void read_x(const std::string& s) { x = std::stoull(s); }
      uint64_t write_x() const { return x; }
      void read_y(const std::string& s) { y = "hello" + s; }
      auto& write_z()
      {
         z[0] = 5;
         return z;
      }
   };

   // Lambda setter taking the parsed value, lambda getter returning a reference.
   struct buffer_input
   {
      std::string str{};
   };

   // Lambda setter taking a glz::context so it can report a domain error.
   struct constrained_age
   {
      int age{};
   };

   // A setter that takes no input at all: the incoming array is discarded and the handler is
   // simply invoked. This exercises the TOML array skipper.
   struct trigger_only
   {
      int calls{};
      void trigger() { ++calls; }
      int count() const { return calls; }
   };

   // glz::skip on the read side: the key is written but never read back.
   struct write_only
   {
      int derived{};
      int stored{};
   };

   // A custom getter/setter over a nested object, which is laid out as a [table].
   struct leaf_value
   {
      std::string name{};
      int id{};
      bool operator==(const leaf_value&) const = default;
   };
   struct nested_custom
   {
      leaf_value inner{};
      const leaf_value& get_inner() const { return inner; }
      void set_inner(leaf_value l) { inner = std::move(l); }
      bool operator==(const nested_custom&) const = default;
   };

   // TOML has no null, so a getter yielding a disengaged optional must drop the key rather than
   // emit `key = ` with nothing after it.
   struct nullable_custom
   {
      std::optional<int> opt{};
      int plain{7};
      const std::optional<int>& get_opt() const { return opt; }
      void set_opt(std::optional<int> v) { opt = v; }
   };

   // std::function members on both sides.
   struct function_members
   {
      int v{};
      std::function<void(int)> setter = [this](int in) { v = in * 2; };
      std::function<int()> getter = [this] { return v + 1; };
   };

   // glz::custom as the entire meta::value rather than one field of an object.
   struct whole_value
   {
      std::string s{};
      void set(const std::string& in) { s = in + "-in"; }
      std::string get() const { return s + "-out"; }
   };
}

template <>
struct glz::meta<i2786::transformed>
{
   using T = i2786::transformed;
   static constexpr auto value = object("x", custom<&T::read_x, &T::write_x>, //
                                        "y", custom<&T::read_y, &T::y>, //
                                        "z", custom<&T::z, &T::write_z>);
};

template <>
struct glz::meta<i2786::buffer_input>
{
   static constexpr auto read_x = [](i2786::buffer_input& s, const std::string& input) { s.str = input; };
   static constexpr auto write_x = [](auto& s) -> auto& { return s.str; };
   static constexpr auto value = glz::object("str", glz::custom<read_x, write_x>);
};

template <>
struct glz::meta<i2786::constrained_age>
{
   using T = i2786::constrained_age;
   static constexpr auto read_x = [](T& s, int age, glz::context& ctx) {
      if (age < 21) {
         ctx.error = glz::error_code::constraint_violated;
         ctx.custom_error_message = "age too young";
      }
      else {
         s.age = age;
      }
   };
   static constexpr auto value = object("age", glz::custom<read_x, &T::age>);
};

template <>
struct glz::meta<i2786::trigger_only>
{
   using T = i2786::trigger_only;
   static constexpr auto value = object("calls", custom<&T::trigger, &T::count>);
};

template <>
struct glz::meta<i2786::write_only>
{
   using T = i2786::write_only;
   static constexpr auto value = object("derived", custom<glz::skip{}, &T::derived>, "stored", &T::stored);
};

template <>
struct glz::meta<i2786::leaf_value>
{
   using T = i2786::leaf_value;
   static constexpr auto value = object("name", &T::name, "id", &T::id);
};

template <>
struct glz::meta<i2786::nested_custom>
{
   using T = i2786::nested_custom;
   static constexpr auto value = object("inner", custom<&T::set_inner, &T::get_inner>);
};

template <>
struct glz::meta<i2786::nullable_custom>
{
   using T = i2786::nullable_custom;
   static constexpr auto value = object("opt", custom<&T::set_opt, &T::get_opt>, "plain", &T::plain);
};

template <>
struct glz::meta<i2786::function_members>
{
   using T = i2786::function_members;
   static constexpr auto value = object("v", custom<&T::setter, &T::getter>);
};

template <>
struct glz::meta<i2786::whole_value>
{
   using T = i2786::whole_value;
   static constexpr auto value = custom<&T::set, &T::get>;
};

suite issue_2786_custom_toml = [] {
   using namespace i2786;

   "member function setter and getter"_test = [] {
      transformed obj{};
      const std::string input = R"(x = "3"
y = "world"
z = [1, 2, 3])";
      auto ec = glz::read_toml(obj, input);
      expect(not ec) << glz::format_error(ec, input);
      expect(obj.x == 3);
      expect(obj.y == "helloworld");
      expect(obj.z == std::array<uint32_t, 3>{1, 2, 3});

      const auto out = glz::write_toml(obj);
      expect(out.has_value());
      expect(out.value() == "x = 3\ny = \"helloworld\"\nz = [5, 2, 3]") << out.value();
   };

   "lambda setter and getter round-trip"_test = [] {
      buffer_input obj{};
      const std::string input = R"(str = "Hello!")";
      auto ec = glz::read_toml(obj, input);
      expect(not ec) << glz::format_error(ec, input);
      expect(obj.str == "Hello!");

      const auto out = glz::write_toml(obj);
      expect(out.has_value());
      expect(out.value() == R"(str = "Hello!")") << out.value();
   };

   "setter reports a custom error through the context"_test = [] {
      constrained_age obj{};
      const std::string bad = "age = 18";
      auto ec = glz::read_toml(obj, bad);
      expect(ec.ec == glz::error_code::constraint_violated);
      expect(glz::format_error(ec, bad).find("age too young") != std::string::npos);
      expect(obj.age == 0);

      const std::string good = "age = 25";
      auto ec2 = glz::read_toml(obj, good);
      expect(not ec2) << glz::format_error(ec2, good);
      expect(obj.age == 25);
   };

   "setter with no input consumes the array"_test = [] {
      trigger_only obj{};
      const std::string input = "calls = [1, 2, 3]";
      auto ec = glz::read_toml(obj, input);
      expect(not ec) << glz::format_error(ec, input);
      expect(obj.calls == 1);

      const auto out = glz::write_toml(obj);
      expect(out.has_value());
      expect(out.value() == "calls = 1") << out.value();
   };

   "glz::skip on the read side leaves the member untouched"_test = [] {
      write_only obj{};
      const std::string input = "derived = 5\nstored = 6";
      auto ec = glz::read_toml(obj, input);
      expect(not ec) << glz::format_error(ec, input);
      expect(obj.derived == 0);
      expect(obj.stored == 6);
   };

   // A custom getter/setter over a nested object must round-trip through a [table] header, which
   // reaches the member along the nested-key resolution path rather than the inline one.
   "custom nested object round-trips through a table header"_test = [] {
      nested_custom obj{};
      obj.inner = {"asdf", 123};

      const auto out = glz::write_toml(obj);
      expect(out.has_value());
      expect(out.value() == "[inner]\nname = \"asdf\"\nid = 123\n") << out.value();

      nested_custom parsed{};
      auto ec = glz::read_toml(parsed, out.value());
      expect(not ec) << glz::format_error(ec, out.value());
      expect(parsed == obj);
   };

   // TOML has no null: a getter yielding a disengaged optional must drop the key. Emitting
   // `opt = ` would produce a document that does not parse back.
   "null getter result drops the key instead of writing a bare ="_test = [] {
      nullable_custom obj{};
      const auto empty = glz::write_toml(obj);
      expect(empty.has_value());
      expect(empty.value() == "plain = 7") << empty.value();

      obj.opt = 5;
      const auto filled = glz::write_toml(obj);
      expect(filled.has_value());
      expect(filled.value() == "opt = 5\nplain = 7") << filled.value();

      nullable_custom parsed{};
      auto ec = glz::read_toml(parsed, filled.value());
      expect(not ec) << glz::format_error(ec, filled.value());
      expect(parsed.opt == 5);
   };

   // The same null check applies in value position, where the object is written inline.
   "null getter result is dropped inside an inline table"_test = [] {
      std::vector<nullable_custom> items{};
      items.resize(2);
      items[1].opt = 5;

      constexpr glz::toml_opts inline_opts{true};
      const auto out = glz::write<inline_opts>(items);
      expect(out.has_value());
      expect(out.value() == "[{plain = 7}, {opt = 5, plain = 7}]") << out.value();
   };

   "std::function members on both sides"_test = [] {
      function_members obj{};
      const std::string input = "v = 21";
      auto ec = glz::read_toml(obj, input);
      expect(not ec) << glz::format_error(ec, input);
      expect(obj.v == 42);

      const auto out = glz::write_toml(obj);
      expect(out.has_value());
      expect(out.value() == "v = 43") << out.value();
   };

   "glz::custom as the whole meta::value"_test = [] {
      whole_value obj{};
      const std::string input = R"("abc")";
      auto ec = glz::read_toml(obj, input);
      expect(not ec) << glz::format_error(ec, input);
      expect(obj.s == "abc-in");

      const auto out = glz::write_toml(obj);
      expect(out.has_value());
      expect(out.value() == R"("abc-in-out")") << out.value();
   };
};

int main() { return 0; }
