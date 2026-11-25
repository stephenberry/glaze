// Reader-resolution tests for TOML: which members a document assigned, which array element a
// sub-table belongs to, and which variant alternative an inline table is.
//
// These live outside toml_test.cpp deliberately. That translation unit is already large enough
// that GCC compiles it near its memory ceiling, and the ambiguous-nest case below instantiates a
// reader per level of a 24-deep variant nest. Kept together they push cc1plus into an OOM.

import std;

import glaze.toml;

import ut;

using namespace ut;

// A struct assembled from more than one place in the document -- see issue #2832
namespace missing_keys
{
   struct tls
   {
      bool enabled{};
      std::string cert{};
   };

   struct server
   {
      std::string host{};
      int port{};
      tls secure{};
   };

   struct other
   {
      int k{};
   };

   struct document
   {
      server srv{};
      other misc{};
   };

   struct item
   {
      std::string name{};
      int qty{};
   };

   struct inventory
   {
      std::vector<item> items{};
   };

   struct optionals
   {
      std::string a{};
      std::optional<std::string> b{};
   };

   struct with_pointer
   {
      std::string a{};
      std::unique_ptr<tls> t{};
   };
}

suite error_on_missing_keys_tests = [] {
   using namespace missing_keys;

   static constexpr glz::opts strict{.format = glz::TOML, .error_on_missing_keys = true};

   "top level missing key"_test = [] {
      const std::string toml = "host = \"localhost\"\nport = 1883\n";
      server value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "secure") << ec.custom_error_message;
   };

   "top level complete"_test = [] {
      const std::string toml = "host = \"localhost\"\nport = 1883\nsecure = { enabled = true, cert = \"c\" }\n";
      server value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
   };

   "option off leaves missing keys alone"_test = [] {
      const std::string toml = "host = \"localhost\"\nport = 1883\n";
      server value{};
      const auto ec = glz::read<glz::opts{.format = glz::TOML}>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
   };

   "a table completed by a later section"_test = [] {
      // The key is only settled once the whole document has been read: [srv] is missing "secure"
      // where its body ends, and an unrelated table sits between it and the section that supplies it.
      const std::string toml = R"(
[srv]
host = "h"
port = 1

[misc]
k = 2

[srv.secure]
enabled = true
cert = "c"
)";
      document value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.srv.secure.cert == "c");
   };

   "a table left incomplete by a later section"_test = [] {
      const std::string toml = R"(
[srv]
host = "h"
port = 1

[misc]
k = 2

[srv.secure]
enabled = true
)";
      document value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "cert") << ec.custom_error_message;
   };

   "a table never named at all"_test = [] {
      const std::string toml = "[srv]\nhost = \"h\"\nport = 1\n\n[misc]\nk = 2\n";
      document value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "secure") << ec.custom_error_message;
   };

   "dotted keys reaching into a table"_test = [] {
      const std::string toml =
         "srv.host = \"h\"\nsrv.port = 1\nsrv.secure.enabled = true\nsrv.secure.cert = \"c\"\nmisc.k = 2\n";
      document value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
   };

   "dotted keys leaving a table incomplete"_test = [] {
      const std::string toml = "srv.host = \"h\"\nsrv.port = 1\nsrv.secure.enabled = true\nmisc.k = 2\n";
      document value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "cert") << ec.custom_error_message;
   };

   "incomplete inline table"_test = [] {
      const std::string toml = "host = \"h\"\nport = 1\nsecure = { enabled = true }\n";
      server value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "cert") << ec.custom_error_message;
   };

   "empty inline table"_test = [] {
      const std::string toml = "host = \"h\"\nport = 1\nsecure = {}\n";
      server value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "enabled") << ec.custom_error_message;
   };

   "every element of an array of tables is checked"_test = [] {
      const std::string toml = "[[items]]\nname = \"a\"\nqty = 1\n\n[[items]]\nname = \"b\"\nqty = 2\n";
      inventory value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.items.size() == 2);
   };

   "an incomplete element of an array of tables"_test = [] {
      const std::string toml = "[[items]]\nname = \"a\"\n\n[[items]]\nname = \"b\"\nqty = 2\n";
      inventory value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "qty") << ec.custom_error_message;
   };

   "nullable members are not required"_test = [] {
      const std::string toml = "a = \"x\"\n";
      optionals value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
   };

   "nullable members are required when null members are written"_test = [] {
      const std::string toml = "a = \"x\"\n";
      optionals value{};
      static constexpr glz::opts opts{.format = glz::TOML, .skip_null_members = false, .error_on_missing_keys = true};
      const auto ec = glz::read<opts>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "b") << ec.custom_error_message;
   };

   "a nullable member that the document does provide is still checked"_test = [] {
      const std::string toml = "a = \"x\"\n\n[t]\nenabled = true\n";
      with_pointer value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "cert") << ec.custom_error_message;
   };

   "unknown keys and missing keys together"_test = [] {
      const std::string toml = "host = \"h\"\nport = 1\nstray = 5\n";
      server value{};
      static constexpr glz::opts opts{
         .format = glz::TOML, .error_on_unknown_keys = false, .error_on_missing_keys = true};
      const auto ec = glz::read<opts>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "secure") << ec.custom_error_message;
   };
};

// Sub-tables of an array of tables: [array.sub] names the last element [[array]] defined
namespace array_subtables
{
   struct tls
   {
      bool enabled{};
      std::string cert{};
   };

   struct limits
   {
      int max{};
   };

   struct entry
   {
      std::string name{};
      tls secure{};
   };

   struct entry_with_two
   {
      std::string name{};
      tls secure{};
      limits caps{};
   };

   struct nested_entry
   {
      std::string name{};
      std::vector<limits> caps{};
   };

   struct inventory
   {
      std::vector<entry> items{};
   };

   struct inventory_two
   {
      std::vector<entry_with_two> items{};
   };

   struct inventory_nested
   {
      std::vector<nested_entry> items{};
   };

   struct optional_inventory
   {
      std::optional<std::vector<entry>> items{};
   };
}

suite array_of_tables_subtable_tests = [] {
   using namespace array_subtables;

   "a sub-table fills the element it follows"_test = [] {
      const std::string toml = R"(
[[items]]
name = "a"

[items.secure]
enabled = true
cert = "c"
)";
      inventory value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.items.size() == 1);
      expect(value.items[0].name == "a");
      expect(value.items[0].secure.enabled);
      expect(value.items[0].secure.cert == "c");
   };

   "a sub-table always names the latest element"_test = [] {
      const std::string toml = R"(
[[items]]
name = "a"

[items.secure]
cert = "first"

[[items]]
name = "b"

[items.secure]
cert = "second"
)";
      inventory value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.items.size() == 2);
      expect(value.items[0].secure.cert == "first");
      expect(value.items[1].secure.cert == "second");
   };

   "two sub-tables on one element"_test = [] {
      const std::string toml = R"(
[[items]]
name = "a"

[items.secure]
cert = "c"

[items.caps]
max = 7
)";
      inventory_two value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.items.size() == 1);
      expect(value.items[0].secure.cert == "c");
      expect(value.items[0].caps.max == 7);
   };

   "an array of tables inside an element"_test = [] {
      const std::string toml = R"(
[[items]]
name = "a"

[[items.caps]]
max = 1

[[items.caps]]
max = 2
)";
      inventory_nested value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.items.size() == 1);
      expect(value.items[0].caps.size() == 2);
      expect(value.items[0].caps[0].max == 1);
      expect(value.items[0].caps[1].max == 2);
   };

   "a sub-table through a nullable array"_test = [] {
      const std::string toml = "[[items]]\nname = \"a\"\n\n[items.secure]\ncert = \"c\"\n";
      optional_inventory value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.items.has_value());
      expect(value.items->size() == 1);
      expect((*value.items)[0].secure.cert == "c");
   };

   "a nullable array is tracked the same as a plain one"_test = [] {
      // resolve_array_of_tables unwraps the member before classifying it and resolve_nested does
      // not, so "[[items]]" and "[items.secure]" once disagreed about whether the same
      // optional<vector<T>> was being tracked: a complete document was rejected, and one missing
      // "name" was accepted.
      static constexpr glz::opts strict{.format = glz::TOML, .error_on_missing_keys = true};
      const std::string complete = "[[items]]\nname = \"a\"\n\n[items.secure]\nenabled = true\ncert = \"c\"\n";
      const std::string no_name = "[[items]]\n\n[items.secure]\nenabled = true\ncert = \"c\"\n";

      optional_inventory value{};
      expect(!glz::read<strict>(value, complete)) << complete;
      expect(value.items.has_value());
      expect((*value.items)[0].secure.cert == "c");

      optional_inventory partial{};
      const auto ec = glz::read<strict>(partial, no_name);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "name") << ec.custom_error_message;

      inventory plain{};
      const auto plain_ec = glz::read<strict>(plain, no_name);
      expect(plain_ec.ec == ec.ec);
      expect(plain_ec.custom_error_message == ec.custom_error_message);
   };

   "a sub-table with no element to belong to is rejected"_test = [] {
      // "[items.secure]" without a preceding "[[items]]" is not a document TOML can produce.
      // Opening an element for it would accept the malformed input and hide a missing "name".
      const std::string toml = "[items.secure]\nenabled = true\ncert = \"c\"\n";
      inventory value{};
      expect(glz::read_toml(value, toml).ec == glz::error_code::syntax_error);
      expect(value.items.empty());
   };

   "a dotted key cannot address an array of tables"_test = [] {
      const std::string toml = "items.name = \"a\"\n";
      inventory value{};
      expect(glz::read_toml(value, toml).ec == glz::error_code::syntax_error);
      expect(value.items.empty());
   };

   "a sub-table counts toward required keys"_test = [] {
      static constexpr glz::opts strict{.format = glz::TOML, .error_on_missing_keys = true};
      const std::string complete = "[[items]]\nname = \"a\"\n\n[items.secure]\nenabled = true\ncert = \"c\"\n";
      inventory value{};
      expect(!glz::read<strict>(value, complete)) << complete;

      const std::string incomplete = "[[items]]\nname = \"a\"\n\n[items.secure]\nenabled = true\n";
      inventory partial{};
      const auto ec = glz::read<strict>(partial, incomplete);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "cert") << ec.custom_error_message;
   };
};

// A variant with more than one object alternative is resolved by trying them
namespace variant_alternatives
{
   struct point
   {
      int x{};
      int y{};
   };

   struct pair
   {
      std::string a{};
      int b{};
   };

   struct trio
   {
      int p{};
      int q{};
      int r{};
   };

   struct holder
   {
      std::string name{};
      std::variant<point, pair, trio> v{};
   };

   struct inner
   {
      int k{};
   };

   struct outer
   {
      inner in{};
      int z{};
   };

   struct alt_outer
   {
      std::string s{};
   };

   struct nested_holder
   {
      std::variant<outer, alt_outer> v{};
   };

   // Shares outer's nested "in" table so that the deduced alternative descends into it, and back
   // out of it, before reaching the key that does not fit.
   struct alt_nested
   {
      inner in{};
      std::string w{};
   };

   struct rewind_holder
   {
      std::variant<outer, alt_nested> v{};
   };
}

suite variant_object_alternative_tests = [] {
   using namespace variant_alternatives;

   "the structurally deduced alternative is kept when it parses"_test = [] {
      const std::string toml = "name = \"n\"\nv = { x = 1, y = 2 }\n";
      holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 0);
      expect(std::get<0>(value.v).x == 1);
      expect(std::get<0>(value.v).y == 2);
   };

   "a later alternative is tried when the first does not fit"_test = [] {
      const std::string toml = "name = \"n\"\nv = { a = \"s\", b = 2 }\n";
      holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 1);
      expect(std::get<1>(value.v).a == "s");
      expect(std::get<1>(value.v).b == 2);
   };

   "every remaining alternative is tried"_test = [] {
      const std::string toml = "name = \"n\"\nv = { p = 1, q = 2, r = 3 }\n";
      holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 2);
      expect(std::get<2>(value.v).r == 3);
   };

   "an alternative whose members are objects"_test = [] {
      const std::string toml = "v = { in = { k = 5 }, z = 9 }\n";
      nested_holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 0);
      expect(std::get<0>(value.v).in.k == 5);
      expect(std::get<0>(value.v).z == 9);
   };

   "retrying past an alternative whose members are objects"_test = [] {
      const std::string toml = "v = { s = \"str\" }\n";
      nested_holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 1);
      expect(std::get<1>(value.v).s == "str");
   };

   "a failed alternative that descended into a nested table rewinds cleanly"_test = [] {
      // "outer" is deduced and consumes "in = { k = 5 }" -- entering a nesting level and leaving it
      // again -- before failing on "w". Only ctx.error, the message and the iterator are reset
      // between attempts, so the retry is correct only because every level of the abandoned parse
      // was entered through an RAII depth_guard that restored ctx.depth on the way out. Were any
      // level to leak, the retry would start deeper than the first attempt did and the leak would
      // accumulate over the read.
      const std::string toml = "v = { in = { k = 5 }, w = \"s\" }\n";
      rewind_holder value{};
      glz::context ctx{};
      const auto ec = glz::read<glz::opts{.format = glz::TOML}>(value, toml, ctx);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 1);
      expect(std::get<1>(value.v).in.k == 5);
      expect(std::get<1>(value.v).w == "s");
      expect(ctx.depth == 0) << ctx.depth;
   };

   "when nothing fits, the deduced alternative's error is reported"_test = [] {
      const std::string toml = "name = \"n\"\nv = { zz = 1 }\n";
      holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(ec.ec == glz::error_code::unknown_key);
   };

   "required keys are not retried past"_test = [] {
      // missing_key is the caller's own strictness, not a sign that the alternative is wrong;
      // "pair" must not be substituted for a "point" that is merely incomplete.
      static constexpr glz::opts strict{.format = glz::TOML, .error_on_missing_keys = true};
      const std::string toml = "name = \"n\"\nv = { x = 1 }\n";
      holder value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "y") << ec.custom_error_message;
   };

   "retrying survives unknown keys being skipped"_test = [] {
      // With error_on_unknown_keys off, a wrong alternative fails with missing_key rather than
      // unknown_key, so treating missing_key as final would stop the retry before it started and
      // leave "pair" unreachable. JSON accepts this same input.
      static constexpr glz::opts opts{
         .format = glz::TOML, .error_on_unknown_keys = false, .error_on_missing_keys = true};
      const std::string toml = "name = \"n\"\nv = { a = \"s\", b = 2 }\n";
      holder value{};
      const auto ec = glz::read<opts>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 1);
      expect(std::get<1>(value.v).b == 2);
   };

   "an incomplete alternative still reports its own strictness"_test = [] {
      // The retry above must not turn a merely incomplete "point" into a "pair": when nothing
      // fits, the deduced alternative's error is what comes back.
      static constexpr glz::opts opts{
         .format = glz::TOML, .error_on_unknown_keys = false, .error_on_missing_keys = true};
      const std::string toml = "name = \"n\"\nv = { x = 1 }\n";
      holder value{};
      const auto ec = glz::read<opts>(value, toml);
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "y") << ec.custom_error_message;
   };

   "retrying still happens under strict options"_test = [] {
      static constexpr glz::opts strict{.format = glz::TOML, .error_on_missing_keys = true};
      const std::string toml = "name = \"n\"\nv = { a = \"s\", b = 2 }\n";
      holder value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.index() == 1);
   };
};

// An ambiguous nest: every level reads as either alternative and the mismatch only shows at the
// bottom, so each retry re-parses the whole subtree below it. Recursive through a vector rather
// than through a template parameter, so depth costs runtime nesting instead of one instantiated
// reader per level -- the templated form cost 185MB of compile memory here, and these jobs build
// several heavy translation units at once on runners with no room to spare.
namespace ambiguous_nest
{
   struct node;

   struct left
   {
      std::vector<node> child{};
      int l{};
   };

   struct right
   {
      std::vector<node> child{};
      int r{};
   };

   struct node
   {
      std::variant<left, right> v{};
   };

   // Calibrated: unbounded retrying costs ~4x per two levels, so this depth takes ~10s in a debug
   // build without the budget against a flat ~50ms with it. Below about 18 the budget saturates
   // before the nest does and both cost the same, so lowering this stops the test discriminating.
   inline constexpr int depth = 22;

   inline std::string nest(const std::string_view bottom)
   {
      std::string toml = "v = ";
      for (int i = 0; i < depth; ++i) toml += "{ child = [ { v = ";
      toml += bottom;
      for (int i = 0; i < depth; ++i) toml += " } ] }";
      return toml;
   }
}

suite variant_alternative_budget_tests = [] {
   "an ambiguous nest that resolves"_test = [] {
      const std::string toml = ambiguous_nest::nest("{ r = 1 }");
      ambiguous_nest::node value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
   };

   "an ambiguous nest that does not resolve is bounded"_test = [] {
      // Retrying every alternative at every level is 2^depth. The speculation budget stops it, and
      // a level that discovered the budget was spent only after running one more retry would still
      // parse its subtree twice per level -- so this must stay fast, not merely terminate.
      const std::string toml = ambiguous_nest::nest("{ zzz = 1 }");
      ambiguous_nest::node value{};
      const auto start = std::chrono::steady_clock::now();
      const auto ec = glz::read_toml(value, toml);
      const auto elapsed = std::chrono::steady_clock::now() - start;
      expect(bool(ec));
      expect(elapsed < std::chrono::seconds(3));
   };
};

// An inline table's body is parsed by a loop that returns at '}'. Falling out of it any other way
// means the document ended mid-table, which used to be silently accepted.
namespace truncated_inline
{
   struct point
   {
      int x{};
      int y{};
   };

   struct holder
   {
      point v{};
   };
}

suite truncated_inline_table_tests = [] {
   using namespace truncated_inline;

   "an inline table with no closing brace is an error"_test = [] {
      for (const std::string toml : {"v = {", "v = {\n", "v = { x = 1", "v = { x = 1,", "v = { x = 1, y = 2"}) {
         holder value{};
         const auto ec = glz::read_toml(value, toml);
         expect(ec.ec == glz::error_code::unexpected_end) << toml;
      }
   };

   "truncation is reported as truncation, not as a missing key"_test = [] {
      // The required-key check belongs to the '}' path; reaching the end of the document instead
      // is a malformed document, and saying "missing key" of it would send the caller looking for
      // a key they did in fact write.
      static constexpr glz::opts strict{.format = glz::TOML, .error_on_missing_keys = true};
      const std::string toml = "v = { x = 1";
      holder value{};
      const auto ec = glz::read<strict>(value, toml);
      expect(ec.ec == glz::error_code::unexpected_end) << glz::format_error(ec, toml);
   };

   "a complete inline table still reads"_test = [] {
      const std::string toml = "v = { x = 1, y = 2 }\n";
      holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.x == 1);
      expect(value.v.y == 2);
   };

   "a trailing comma before the brace is still accepted"_test = [] {
      const std::string toml = "v = { x = 1, y = 2, }\n";
      holder value{};
      const auto ec = glz::read_toml(value, toml);
      expect(!ec) << glz::format_error(ec, toml);
      expect(value.v.y == 2);
   };
};

int main() { return 0; }
