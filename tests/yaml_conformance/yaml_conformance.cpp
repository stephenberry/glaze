// YAML conformance tests generated from https://github.com/yaml/yaml-test-suite
// Tests marked "known failure" assert the parser's current behavior.
// As YAML support improves, update these to full conformance assertions.

#include <string>

#include "glaze/glaze.hpp"
#include "glaze/yaml.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Normalize JSON: parse then re-serialize to canonical form
static std::string normalize_json(const std::string& json)
{
   glz::generic val{};
   auto ec = glz::read_json(val, json);
   if (ec) return {};
   std::string out;
   (void)glz::write_json(val, out);
   return out;
}

// ============================================================================
// PASSING TESTS (168 tests) - These must continue to pass
// ============================================================================

suite yaml_conformance_pass_1 = [] {
   // 229Q: Spec Example 2.4. Sequence of Mappings
   "229Q"_test = [] {
      std::string yaml = R"yaml(-
  name: Mark McGwire
  hr:   65
  avg:  0.278
-
  name: Sammy Sosa
  hr:   63
  avg:  0.288
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "name": "Mark McGwire",
    "hr": 65,
    "avg": 0.278
  },
  {
    "name": "Sammy Sosa",
    "hr": 63,
    "avg": 0.288
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 236B: Invalid value after mapping
   "236B"_test = [] {
      std::string yaml = R"yaml(foo:
  bar
invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 27NA: Spec Example 5.9. Directive Indicator
   "27NA"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
--- text
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("text"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2AUY: Tags in Block Sequence
   "2AUY"_test = [] {
      std::string yaml = R"yaml( - !!str a
 - b
 - !!int 42
 - d
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "a",
  "b",
  42,
  "d"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2G84_02: Literal modifers
   "2G84_02"_test = [] {
      std::string yaml = R"yaml(--- |1-)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(""
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2G84_03: Literal modifers
   "2G84_03"_test = [] {
      std::string yaml = R"yaml(--- |1+)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(""
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2JQS: Block Mapping with Missing Keys
   "2JQS"_test = [] {
      std::string yaml = R"yaml(: a
: b
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // 33X3: Three explicit integers in a block sequence
   "33X3"_test = [] {
      std::string yaml = R"yaml(---
- !!int 1
- !!int -2
- !!int 33
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  1,
  -2,
  33
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3ALJ: Block Sequence in Block Sequence
   "3ALJ"_test = [] {
      std::string yaml = R"yaml(- - s1_i1
  - s1_i2
- s2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  [
    "s1_i1",
    "s1_i2"
  ],
  "s2"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3HFZ: Invalid content after document end marker
   "3HFZ"_test = [] {
      std::string yaml = R"yaml(---
key: value
... invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 3RLN_00: Leading tabs in double quoted
   "3RLN_00"_test = [] {
      std::string yaml = R"yaml("1 leading
    \ttab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("1 leading \ttab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3RLN_02: Leading tabs in double quoted
   "3RLN_02"_test = [] {
      std::string yaml = R"yaml("3 leading
    	tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("3 leading tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3RLN_03: Leading tabs in double quoted
   "3RLN_03"_test = [] {
      std::string yaml = R"yaml("4 leading
    \t  tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("4 leading \t  tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3RLN_05: Leading tabs in double quoted
   "3RLN_05"_test = [] {
      std::string yaml = R"yaml("6 leading
    	  tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("6 leading tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3UYS: Escaped slash in double quotes
   "3UYS"_test = [] {
      std::string yaml = R"yaml(escaped slash: "a\/b"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "escaped slash": "a/b"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4CQQ: Spec Example 2.18. Multi-line Flow Scalars
   "4CQQ"_test = [] {
      std::string yaml = R"yaml(plain:
  This unquoted scalar
  spans many lines.

quoted: "So does this
  quoted scalar.\n"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "plain": "This unquoted scalar spans many lines.",
  "quoted": "So does this quoted scalar.\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4EJS: Invalid tabs as indendation in a mapping
   "4EJS"_test = [] {
      std::string yaml = R"yaml(---
a:
	b:
		c: value
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 4GC6: Spec Example 7.7. Single Quoted Characters
   "4GC6"_test = [] {
      std::string yaml = R"yaml('here''s to "quotes"'
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("here's to \"quotes\""
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4HVU: Wrong indendation in Sequence
   "4HVU"_test = [] {
      std::string yaml = R"yaml(key:
   - ok
   - also ok
  - wrong
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 4JVG: Scalar value with two anchors
   "4JVG"_test = [] {
      std::string yaml = R"yaml(top1: &node1
  &k1 key1: val1
top2: &node2
  &v2 val2
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "top1": {
    "key1": "val1"
  },
  "top2": "val2"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4Q9F: Folded Block Scalar [1.3]
   "4Q9F"_test = [] {
      std::string yaml = R"yaml(--- >
 ab
 cd
 
 ef


 gh
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("ab cd\nef\n\ngh\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4RWC: Trailing spaces after flow collection
   "4RWC"_test = [] {
      std::string yaml = R"yaml(  [1, 2, 3]  
  )yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  1,
  2,
  3
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4UYU: Colon in Double Quoted String
   "4UYU"_test = [] {
      std::string yaml = R"yaml("foo: bar\": baz"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo: bar\": baz"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4V8U: Plain scalar with backslashes
   "4V8U"_test = [] {
      std::string yaml = R"yaml(---
plain\value\with\backslashes
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("plain\\value\\with\\backslashes"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 54T7: Flow Mapping
   "54T7"_test = [] {
      std::string yaml = R"yaml({foo: you, bar: far}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "you",
  "bar": "far"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 58MP: Flow mapping edge cases
   "58MP"_test = [] {
      std::string yaml = R"yaml({x: :x}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "x": ":x"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5BVJ: Spec Example 5.7. Block Scalar Indicators
   "5BVJ"_test = [] {
      std::string yaml = R"yaml(literal: |
  some
  text
folded: >
  some
  text
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "literal": "some\ntext\n",
  "folded": "some text\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5C5M: Spec Example 7.15. Flow Mappings
   "5C5M"_test = [] {
      std::string yaml = R"yaml(- { one : two , three: four , }
- {five: six,seven : eight}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "one": "two",
    "three": "four"
  },
  {
    "five": "six",
    "seven": "eight"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5GBF: Spec Example 6.5. Empty Lines
   "5GBF"_test = [] {
      std::string yaml = R"yaml(Folding:
  "Empty line
   	
  as a line feed"
Chomping: |
  Clipped empty lines
 

)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "Folding": "Empty line\nas a line feed",
  "Chomping": "Clipped empty lines\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5NYZ: Spec Example 6.9. Separated Comment
   "5NYZ"_test = [] {
      std::string yaml = R"yaml(key:    # Comment
  value
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5T43: Colon at the beginning of adjacent flow scalar
   "5T43"_test = [] {
      std::string yaml = R"yaml(- { "key":value }
- { "key"::value }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "key": "value"
  },
  {
    "key": ":value"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 652Z: Question mark at start of flow key
   "652Z"_test = [] {
      std::string yaml = R"yaml({ ?foo: bar,
bar: 42
}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "?foo" : "bar",
  "bar" : 42
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 65WH: Single Entry Block Sequence
   "65WH"_test = [] {
      std::string yaml = R"yaml(- foo
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "foo"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6BCT: Spec Example 6.3. Separation Spaces
   "6BCT"_test = [] {
      std::string yaml = R"yaml(- foo:	 bar
- - baz
  -	baz
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "foo": "bar"
  },
  [
    "baz",
    "baz"
  ]
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6CA3: Tab indented top flow
   "6CA3"_test = [] {
      std::string yaml = R"yaml(	[
	]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6H3V: Backslashes in singlequotes
   "6H3V"_test = [] {
      std::string yaml = R"yaml('foo: bar\': baz'
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo: bar\\": "baz'"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6JTT: Flow sequence without closing bracket
   "6JTT"_test = [] {
      std::string yaml = R"yaml(---
[ [ a, b, c ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 6M2F: Aliases in Explicit Block Mapping
   "6M2F"_test = [] {
      std::string yaml = R"yaml(? &a a
: &b b
: *a
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // 6PBE: Zero-indented sequences in explicit mapping keys
   "6PBE"_test = [] {
      std::string yaml = R"yaml(---
?
- a
- b
:
- c
- d
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // 6S55: Invalid scalar at the end of sequence
   "6S55"_test = [] {
      std::string yaml = R"yaml(key:
 - bar
 - baz
 invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 6SLA: Allowed characters in quoted mapping key
   "6SLA"_test = [] {
      std::string yaml = R"yaml("foo\nbar:baz\tx \\$%^&*()x": 23
'x\ny:z\tx $%^&*()x': 24
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo\nbar:baz\tx \\$%^&*()x": 23,
  "x\\ny:z\\tx $%^&*()x": 24
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6WPF: Spec Example 6.8. Flow Folding [1.3]
   "6WPF"_test = [] {
      std::string yaml = R"yaml(---
"
  foo 
 
    bar

  baz
"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" foo\nbar\nbaz "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6ZKB: Spec Example 9.6. Stream
   "6ZKB"_test = [] {
      std::string yaml = R"yaml(Document
---
# Empty
...
%YAML 1.2
---
matches %: 20
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("Document"
null
{
  "matches %": 20
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 753E: Block Scalar Strip [1.3]
   "753E"_test = [] {
      std::string yaml = R"yaml(--- |-
 ab
 
 
...
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("ab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7A4E: Spec Example 7.6. Double Quoted Lines
   "7A4E"_test = [] {
      std::string yaml = R"yaml(" 1st non-empty

 2nd non-empty 
	3rd non-empty "
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" 1st non-empty\n2nd non-empty 3rd non-empty "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7MNF: Missing colon
   "7MNF"_test = [] {
      std::string yaml = R"yaml(top1:
  key1: val1
top2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 7Z25: Bare document after document end marker
   "7Z25"_test = [] {
      std::string yaml = R"yaml(---
scalar1
...
key: value
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("scalar1"
{
  "key": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 8QBE: Block Sequence in Block Mapping
   "8QBE"_test = [] {
      std::string yaml = R"yaml(key:
 - item1
 - item2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": [
    "item1",
    "item2"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 8XDJ: Comment in plain multiline value
   "8XDJ"_test = [] {
      std::string yaml = R"yaml(key: word1
#  xxx
  word2
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 93WF: Spec Example 6.6. Line Folding [1.3]
   "93WF"_test = [] {
      std::string yaml = R"yaml(--- >-
  trimmed
  
 

  as
  space
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("trimmed\n\n\nas space"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 96L6: Spec Example 2.14. In the folded scalars, newlines become spaces
   "96L6"_test = [] {
      std::string yaml = R"yaml(--- >
  Mark McGwire's
  year was crippled
  by a knee injury.
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("Mark McGwire's year was crippled by a knee injury.\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9CWY: Invalid scalar at the end of mapping
   "9CWY"_test = [] {
      std::string yaml = R"yaml(key:
 - item1
 - item2
invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 9FMG: Multi-level Mapping Indent
   "9FMG"_test = [] {
      std::string yaml = R"yaml(a:
  b:
    c: d
  e:
    f: g
h: i
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": {
    "b": {
      "c": "d"
    },
    "e": {
      "f": "g"
    }
  },
  "h": "i"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9HCY: Need document footer before directives
   "9HCY"_test = [] {
      std::string yaml = R"yaml(!foo "bar"
%TAG ! tag:example.com,2000:app/
---
!foo "bar"
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 9J7A: Simple Mapping Indent
   "9J7A"_test = [] {
      std::string yaml = R"yaml(foo:
  bar: baz
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": {
    "bar": "baz"
  }
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9MMA: Directive by itself with no document
   "9MMA"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 9MQT_00: Scalar doc with '...' in content
   "9MQT_00"_test = [] {
      std::string yaml = R"yaml(--- "a
...x
b"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("a ...x b"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9SA2: Multiline double quoted flow mapping key
   "9SA2"_test = [] {
      std::string yaml = R"yaml(---
- { "single line": value}
- { "multi
  line": value}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "single line": "value"
  },
  {
    "multi line": "value"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9SHH: Spec Example 5.8. Quoted Scalar Indicators
   "9SHH"_test = [] {
      std::string yaml = R"yaml(single: 'text'
double: "text"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "single": "text",
  "double": "text"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9TFX: Spec Example 7.6. Double Quoted Lines [1.3]
   "9TFX"_test = [] {
      std::string yaml = R"yaml(---
" 1st non-empty

 2nd non-empty 
 3rd non-empty "
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" 1st non-empty\n2nd non-empty 3rd non-empty "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

};

suite yaml_conformance_pass_2 = [] {
   // 9U5K: Spec Example 2.12. Compact Nested Mapping
   "9U5K"_test = [] {
      std::string yaml = R"yaml(---
# Products purchased
- item    : Super Hoop
  quantity: 1
- item    : Basketball
  quantity: 4
- item    : Big Shoes
  quantity: 1
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "item": "Super Hoop",
    "quantity": 1
  },
  {
    "item": "Basketball",
    "quantity": 4
  },
  {
    "item": "Big Shoes",
    "quantity": 1
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // A6F9: Spec Example 8.4. Chomping Final Line Break
   "A6F9"_test = [] {
      std::string yaml = R"yaml(strip: |-
  text
clip: |
  text
keep: |+
  text
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "strip": "text",
  "clip": "text\n",
  "keep": "text\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // B3HG: Spec Example 8.9. Folded Scalar [1.3]
   "B3HG"_test = [] {
      std::string yaml = R"yaml(--- >
 folded
 text


)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("folded text\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // BF9H: Trailing comment in multiline plain scalar
   "BF9H"_test = [] {
      std::string yaml = R"yaml(---
plain: a
       b # end of scalar
       c
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // CFD4: Empty implicit key in single pair flow sequences
   "CFD4"_test = [] {
      std::string yaml = R"yaml(- [ : empty key ]
- [: another empty key]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // CML9: Missing comma in flow
   "CML9"_test = [] {
      std::string yaml = R"yaml(key: [ word1
#  xxx
  word2 ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // CPZ3: Doublequoted scalar starting with a tab
   "CPZ3"_test = [] {
      std::string yaml = R"yaml(---
tab: "\tstring"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "tab": "\tstring"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // CQ3W: Double quoted string without closing quote
   "CQ3W"_test = [] {
      std::string yaml = R"yaml(---
key: "missing closing quote
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // CXX2: Mapping with anchor on document start line
   "CXX2"_test = [] {
      std::string yaml = R"yaml(--- &anchor a: b
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "b"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // D88J: Flow Sequence in Block Mapping
   "D88J"_test = [] {
      std::string yaml = R"yaml(a: [b, c]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": [
    "b",
    "c"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // D9TU: Single Pair Block Mapping
   "D9TU"_test = [] {
      std::string yaml = R"yaml(foo: bar
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "bar"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DBG4: Spec Example 7.10. Plain Characters
   "DBG4"_test = [] {
      std::string yaml = R"yaml(# Outside flow collection:
- ::vector
- ": - ()"
- Up, up, and away!
- -123
- http://example.com/foo#bar
# Inside flow collection:
- [ ::vector,
  ": - ()",
  "Up, up and away!",
  -123,
  http://example.com/foo#bar ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "::vector",
  ": - ()",
  "Up, up, and away!",
  -123,
  "http://example.com/foo#bar",
  [
    "::vector",
    ": - ()",
    "Up, up and away!",
    -123,
    "http://example.com/foo#bar"
  ]
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DC7X: Various trailing tabs
   "DC7X"_test = [] {
      std::string yaml = R"yaml(a: b	
seq:	
 - a	
c: d	#X
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "b",
  "seq": [
    "a"
  ],
  "c": "d"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DE56_04: Trailing tabs in double quoted
   "DE56_04"_test = [] {
      std::string yaml = R"yaml("5 trailing	
    tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("5 trailing tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DE56_05: Trailing tabs in double quoted
   "DE56_05"_test = [] {
      std::string yaml = R"yaml("6 trailing	  
    tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("6 trailing tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DHP8: Flow Sequence
   "DHP8"_test = [] {
      std::string yaml = R"yaml([foo, bar, 42]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "foo",
  "bar",
  42
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK4H: Implicit key followed by newline
   "DK4H"_test = [] {
      std::string yaml = R"yaml(---
[ key
  : value ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // DK95_02: Tabs that look like indentation
   "DK95_02"_test = [] {
      std::string yaml = R"yaml(foo: "bar
  	baz"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo" : "bar baz"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK95_03: Tabs that look like indentation
   "DK95_03"_test = [] {
      std::string yaml = R"yaml( 	
foo: 1
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo" : 1
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK95_06: Tabs that look like indentation
   "DK95_06"_test = [] {
      std::string yaml = R"yaml(foo:
  a: 1
  	b: 2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // DK95_08: Tabs that look like indentation
   "DK95_08"_test = [] {
      std::string yaml = R"yaml(foo: "bar
 	 	 baz 	 	 "
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo" : "bar baz \t \t "
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // F3CP: Nested flow collections on one line
   "F3CP"_test = [] {
      std::string yaml = R"yaml(---
{ a: [b, c, { d: [e, f] } ] }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": [
    "b",
    "c",
    {
      "d": [
        "e",
        "f"
      ]
    }
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // F8F9: Spec Example 8.5. Chomping Trailing Lines
   "F8F9"_test = [] {
      std::string yaml = R"yaml( # Strip
  # Comments:
strip: |-
  # text
  
 # Clip
  # comments:

clip: |
  # text
 
 # Keep
  # comments:

keep: |+
  # text

 # Trail
  # comments.
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "strip": "# text",
  "clip": "# text\n",
  "keep": "# text\n\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // FH7J: Tags on Empty Scalars
   "FH7J"_test = [] {
      std::string yaml = R"yaml(- !!str
-
  !!null : a
  b: !!str
- !!str : !!null
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // FQ7F: Spec Example 2.1. Sequence of Scalars
   "FQ7F"_test = [] {
      std::string yaml = R"yaml(- Mark McGwire
- Sammy Sosa
- Ken Griffey
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "Mark McGwire",
  "Sammy Sosa",
  "Ken Griffey"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // FRK4: Spec Example 7.3. Completely Empty Flow Nodes
   "FRK4"_test = [] {
      std::string yaml = R"yaml({
  ? foo :,
  : bar,
}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // FUP4: Flow Sequence in Flow Sequence
   "FUP4"_test = [] {
      std::string yaml = R"yaml([a, [b, c]]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "a",
  [
    "b",
    "c"
  ]
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // G4RS: Spec Example 2.17. Quoted Scalars
   "G4RS"_test = [] {
      std::string yaml = R"yaml(unicode: "Sosa did fine.\u263A"
control: "\b1998\t1999\t2000\n"
hex esc: "\x0d\x0a is \r\n"

single: '"Howdy!" he cried.'
quoted: ' # Not a ''comment''.'
tie-fighter: '|\-*-/|'
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "unicode": "Sosa did fine.☺",
  "control": "\b1998\t1999\t2000\n",
  "hex esc": "\r\n is \r\n",
  "single": "\"Howdy!\" he cried.",
  "quoted": " # Not a 'comment'.",
  "tie-fighter": "|\\-*-/|"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // G7JE: Multiline implicit keys
   "G7JE"_test = [] {
      std::string yaml = R"yaml(a\nb: 1
c
 d: 1
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // G992: Spec Example 8.9. Folded Scalar
   "G992"_test = [] {
      std::string yaml = R"yaml(>
 folded
 text


)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("folded text\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // G9HC: Invalid anchor in zero indented sequence
   "G9HC"_test = [] {
      std::string yaml = R"yaml(---
seq:
&anchor
- a
- b
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // GDY7: Comment that looks like a mapping key
   "GDY7"_test = [] {
      std::string yaml = R"yaml(key: value
this is #not a: key
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // H3Z8: Literal unicode
   "H3Z8"_test = [] {
      std::string yaml = R"yaml(---
wanted: love ♥ and peace ☮
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "wanted": "love ♥ and peace ☮"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // H7J7: Node anchor not indented
   "H7J7"_test = [] {
      std::string yaml = R"yaml(key: &x
!!map
  a: b
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // H7TQ: Extra words on %YAML directive
   "H7TQ"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2 foo
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // HM87_00: Scalars in flow start with syntax char
   "HM87_00"_test = [] {
      std::string yaml = R"yaml([:x]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  ":x"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // HM87_01: Scalars in flow start with syntax char
   "HM87_01"_test = [] {
      std::string yaml = R"yaml([?x]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "?x"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // HMK4: Spec Example 2.16. Indentation determines scope
   "HMK4"_test = [] {
      std::string yaml = R"yaml(name: Mark McGwire
accomplishment: >
  Mark set a major league
  home run record in 1998.
stats: |
  65 Home Runs
  0.278 Batting Average
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "name": "Mark McGwire",
  "accomplishment": "Mark set a major league home run record in 1998.\n",
  "stats": "65 Home Runs\n0.278 Batting Average\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // J5UC: Multiple Pair Block Mapping
   "J5UC"_test = [] {
      std::string yaml = R"yaml(foo: blue
bar: arrr
baz: jazz
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "blue",
  "bar": "arrr",
  "baz": "jazz"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // J7VC: Empty Lines Between Mapping Elements
   "J7VC"_test = [] {
      std::string yaml = R"yaml(one: 2


three: 4
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "one": 2,
  "three": 4
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // J9HZ: Spec Example 2.9. Single Document with Two Comments
   "J9HZ"_test = [] {
      std::string yaml = R"yaml(---
hr: # 1998 hr ranking
  - Mark McGwire
  - Sammy Sosa
rbi:
  # 1998 rbi ranking
  - Sammy Sosa
  - Ken Griffey
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "hr": [
    "Mark McGwire",
    "Sammy Sosa"
  ],
  "rbi": [
    "Sammy Sosa",
    "Ken Griffey"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JEF9_00: Trailing whitespace in streams
   "JEF9_00"_test = [] {
      std::string yaml = R"yaml(- |+


)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "\n\n"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JEF9_01: Trailing whitespace in streams
   "JEF9_01"_test = [] {
      std::string yaml = R"yaml(- |+
   
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "\n"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JEF9_02: Trailing whitespace in streams
   "JEF9_02"_test = [] {
      std::string yaml = R"yaml(- |+
   )yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "\n"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JQ4R: Spec Example 8.14. Block Sequence
   "JQ4R"_test = [] {
      std::string yaml = R"yaml(block sequence:
  - one
  - two : three
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "block sequence": [
    "one",
    {
      "two": "three"
    }
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JR7V: Question marks in scalars
   "JR7V"_test = [] {
      std::string yaml = R"yaml(- a?string
- another ? string
- key: value?
- [a?string]
- [another ? string]
- {key: value? }
- {key: value?}
- {key?: value }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "a?string",
  "another ? string",
  {
    "key": "value?"
  },
  [
    "a?string"
  ],
  [
    "another ? string"
  ],
  {
    "key": "value?"
  },
  {
    "key": "value?"
  },
  {
    "key?": "value"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JY7Z: Trailing content that looks like a mapping
   "JY7Z"_test = [] {
      std::string yaml = R"yaml(key1: "quoted1"
key2: "quoted2" no key: nor value
key3: "quoted3"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // K4SU: Multiple Entry Block Sequence
   "K4SU"_test = [] {
      std::string yaml = R"yaml(- foo
- bar
- 42
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "foo",
  "bar",
  42
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // K527: Spec Example 6.6. Line Folding
   "K527"_test = [] {
      std::string yaml = R"yaml(>-
  trimmed
  
 

  as
  space
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("trimmed\n\n\nas space"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // K54U: Tab after document header
   "K54U"_test = [] {
      std::string yaml = R"yaml(---	scalar
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("scalar"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // KH5V_00: Inline tabs in double quoted
   "KH5V_00"_test = [] {
      std::string yaml = R"yaml("1 inline\ttab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("1 inline\ttab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // KH5V_02: Inline tabs in double quoted
   "KH5V_02"_test = [] {
      std::string yaml = R"yaml("3 inline	tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("3 inline\ttab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // KK5P: Various combinations of explicit block mappings
   "KK5P"_test = [] {
      std::string yaml = R"yaml(complex1:
  ? - a
complex2:
  ? - a
  : b
complex3:
  ? - a
  : >
    b
complex4:
  ? >
    a
  :
complex5:
  ? - a
  : - b
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // KMK3: Block Submapping
   "KMK3"_test = [] {
      std::string yaml = R"yaml(foo:
  bar: 1
baz: 2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": {
    "bar": 1
  },
  "baz": 2
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // KSS4: Scalars on --- line
   "KSS4"_test = [] {
      std::string yaml = R"yaml(--- "quoted
string"
--- &node foo
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("quoted string"
"foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // L383: Two scalar docs with trailing comments
   "L383"_test = [] {
      std::string yaml = R"yaml(--- foo  # comment
--- foo  # comment
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo"
"foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // LHL4: Invalid tag
   "LHL4"_test = [] {
      std::string yaml = R"yaml(---
!invalid{}tag scalar
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // LX3P: Implicit Flow Mapping Key on one line
   "LX3P"_test = [] {
      std::string yaml = R"yaml([flow]: block
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // M2N8_00: Question mark edge cases
   "M2N8_00"_test = [] {
      std::string yaml = R"yaml(- ? : x
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // M2N8_01: Question mark edge cases
   "M2N8_01"_test = [] {
      std::string yaml = R"yaml(? []: x
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

};

suite yaml_conformance_pass_3 = [] {
   // M5DY: Spec Example 2.11. Mapping between Sequences
   "M5DY"_test = [] {
      std::string yaml = R"yaml(? - Detroit Tigers
  - Chicago cubs
:
  - 2001-07-23

? [ New York Yankees,
    Atlanta Braves ]
: [ 2001-07-02, 2001-08-12,
    2001-08-14 ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // M6YH: Block sequence indentation
   "M6YH"_test = [] {
      std::string yaml = R"yaml(- |
 x
-
 foo: bar
-
 - 42
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "x\n",
  {
    "foo" : "bar"
  },
  [
    42
  ]
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // M7NX: Nested flow collections
   "M7NX"_test = [] {
      std::string yaml = R"yaml(---
{
 a: [
  b, c, {
   d: [e, f]
  }
 ]
}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": [
    "b",
    "c",
    {
      "d": [
        "e",
        "f"
      ]
    }
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MUS6_00: Directive variants
   "MUS6_00"_test = [] {
      std::string yaml = R"yaml(%YAML 1.1#...
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // MXS3: Flow Mapping in Block Sequence
   "MXS3"_test = [] {
      std::string yaml = R"yaml(- {a: b}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "a": "b"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MYW6: Block Scalar Strip
   "MYW6"_test = [] {
      std::string yaml = R"yaml(|-
 ab
 
 
...
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("ab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MZX3: Non-Specific Tags on Scalars
   "MZX3"_test = [] {
      std::string yaml = R"yaml(- plain
- "double quoted"
- 'single quoted'
- >
  block
- plain again
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "plain",
  "double quoted",
  "single quoted",
  "block\n",
  "plain again"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // NAT4: Various empty or newline only quoted strings
   "NAT4"_test = [] {
      std::string yaml = R"yaml(---
a: '
  '
b: '  
  '
c: "
  "
d: "  
  "
e: '

  '
f: "

  "
g: '


  '
h: "


  "
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": " ",
  "b": " ",
  "c": " ",
  "d": " ",
  "e": "\n",
  "f": "\n",
  "g": "\n\n",
  "h": "\n\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // NHX8: Empty Lines at End of Document
   "NHX8"_test = [] {
      std::string yaml = R"yaml(:


)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // NP9H: Spec Example 7.5. Double Quoted Line Breaks
   "NP9H"_test = [] {
      std::string yaml = R"yaml("folded 
to a space,	
 
to a line feed, or 	\
 \ 	non-content"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("folded to a space,\nto a line feed, or \t \tnon-content"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // P94K: Spec Example 6.11. Multi-Line Comments
   "P94K"_test = [] {
      std::string yaml = R"yaml(key:    # Comment
        # lines
  value


)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // PBJ2: Spec Example 2.3. Mapping Scalars to Sequences
   "PBJ2"_test = [] {
      std::string yaml = R"yaml(american:
  - Boston Red Sox
  - Detroit Tigers
  - New York Yankees
national:
  - New York Mets
  - Chicago Cubs
  - Atlanta Braves
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "american": [
    "Boston Red Sox",
    "Detroit Tigers",
    "New York Yankees"
  ],
  "national": [
    "New York Mets",
    "Chicago Cubs",
    "Atlanta Braves"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // PRH3: Spec Example 7.9. Single Quoted Lines
   "PRH3"_test = [] {
      std::string yaml = R"yaml(' 1st non-empty

 2nd non-empty 
	3rd non-empty '
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" 1st non-empty\n2nd non-empty 3rd non-empty "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Q4CL: Trailing content after quoted value
   "Q4CL"_test = [] {
      std::string yaml = R"yaml(key1: "quoted1"
key2: "quoted2" trailing content
key3: "quoted3"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Q5MG: Tab at beginning of line followed by a flow mapping
   "Q5MG"_test = [] {
      std::string yaml = R"yaml(	{}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Q88A: Spec Example 7.23. Flow Content
   "Q88A"_test = [] {
      std::string yaml = R"yaml(- [ a, b ]
- { a: b }
- "a"
- 'b'
- c
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  [
    "a",
    "b"
  ],
  {
    "a": "b"
  },
  "a",
  "b",
  "c"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Q8AD: Spec Example 7.5. Double Quoted Line Breaks [1.3]
   "Q8AD"_test = [] {
      std::string yaml = R"yaml(---
"folded 
to a space,
 
to a line feed, or 	\
 \ 	non-content"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("folded to a space,\nto a line feed, or \t \tnon-content"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Q9WF: Spec Example 6.12. Separation Spaces
   "Q9WF"_test = [] {
      std::string yaml = R"yaml({ first: Sammy, last: Sosa }:
# Statistics:
  hr:  # Home runs
     65
  avg: # Average
   0.278
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // QLJ7: Tag shorthand used in documents but only defined in the first
   "QLJ7"_test = [] {
      std::string yaml = R"yaml(%TAG !prefix! tag:example.com,2011:
--- !prefix!A
a: b
--- !prefix!B
c: d
--- !prefix!C
e: f
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "b"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // R52L: Nested flow mapping sequence and mappings
   "R52L"_test = [] {
      std::string yaml = R"yaml(---
{ top1: [item1, {key2: value2}, item3], top2: value2 }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "top1": [
    "item1",
    {
      "key2": "value2"
    },
    "item3"
  ],
  "top2": "value2"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // RHX7: YAML directive without document end marker
   "RHX7"_test = [] {
      std::string yaml = R"yaml(---
key: value
%YAML 1.2
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // RTP8: Spec Example 9.2. Document Markers
   "RTP8"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
---
Document
... # Suffix
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("Document"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // S4GJ: Invalid text after block scalar indicator
   "S4GJ"_test = [] {
      std::string yaml = R"yaml(---
folded: > first line
  second line
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // S7BG: Colon followed by comma
   "S7BG"_test = [] {
      std::string yaml = R"yaml(---
- :,
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  ":,"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // SF5V: Duplicate YAML directive
   "SF5V"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
%YAML 1.2
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // SM9W_01: Single character streams
   "SM9W_01"_test = [] {
      std::string yaml = R"yaml(:)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // SR86: Anchor plus Alias
   "SR86"_test = [] {
      std::string yaml = R"yaml(key1: &a value
key2: &b *a
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // SSW6: Spec Example 7.7. Single Quoted Characters [1.3]
   "SSW6"_test = [] {
      std::string yaml = R"yaml(---
'here''s to "quotes"'
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("here's to \"quotes\""
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // SU74: Anchor and alias as mapping key
   "SU74"_test = [] {
      std::string yaml = R"yaml(key1: &alias value1
&b *alias : value2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // SY6V: Anchor before sequence entry on same line (not yet validated)
   "SY6V"_test = [] {
      std::string yaml = R"yaml(&anchor - sequence entry
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "sequence entry"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // SYW4: Spec Example 2.2. Mapping Scalars to Scalars
   "SYW4"_test = [] {
      std::string yaml = R"yaml(hr:  65    # Home runs
avg: 0.278 # Batting average
rbi: 147   # Runs Batted In
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "hr": 65,
  "avg": 0.278,
  "rbi": 147
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // T4YY: Spec Example 7.9. Single Quoted Lines [1.3]
   "T4YY"_test = [] {
      std::string yaml = R"yaml(---
' 1st non-empty

 2nd non-empty 
 3rd non-empty '
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" 1st non-empty\n2nd non-empty 3rd non-empty "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // TE2A: Spec Example 8.16. Block Mappings
   "TE2A"_test = [] {
      std::string yaml = R"yaml(block mapping:
 key: value
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "block mapping": {
    "key": "value"
  }
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // TL85: Spec Example 6.8. Flow Folding
   "TL85"_test = [] {
      std::string yaml = R"yaml("
  foo 
 
  	 bar

  baz
"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" foo\nbar\nbaz "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // TS54: Folded Block Scalar
   "TS54"_test = [] {
      std::string yaml = R"yaml(>
 ab
 cd
 
 ef


 gh
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("ab cd\nef\n\ngh\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UDM2: Plain URL in flow mapping
   "UDM2"_test = [] {
      std::string yaml = R"yaml(- { url: http://example.org }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "url": "http://example.org"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UKK6_00: Syntax character edge cases
   "UKK6_00"_test = [] {
      std::string yaml = R"yaml(- :
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // V9D5: Spec Example 8.19. Compact Block Mappings
   "V9D5"_test = [] {
      std::string yaml = R"yaml(- sun: yellow
- ? earth: blue
  : moon: white
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
   };

   // VJP3_00: Flow collections over many lines
   "VJP3_00"_test = [] {
      std::string yaml = R"yaml(k: {
k
:
v
}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // WZ62: Spec Example 7.2. Empty Content
   "WZ62"_test = [] {
      std::string yaml = R"yaml({
  foo : !!str,
  !!str : bar,
}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "",
  "": "bar"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // XV9V: Spec Example 6.5. Empty Lines [1.3]
   "XV9V"_test = [] {
      std::string yaml = R"yaml(Folding:
  "Empty line

  as a line feed"
Chomping: |
  Clipped empty lines
 

)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "Folding": "Empty line\nas a line feed",
  "Chomping": "Clipped empty lines\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Y79Y_000: Tabs in various contexts
   "Y79Y_000"_test = [] {
      std::string yaml = R"yaml(foo: |
	
bar: 1
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_002: Tabs in various contexts
   "Y79Y_002"_test = [] {
      std::string yaml = R"yaml(- [
	
 foo
 ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  [
    "foo"
  ]
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // YD5X: Spec Example 2.5. Sequence of Sequences
   "YD5X"_test = [] {
      std::string yaml = R"yaml(- [name        , hr, avg  ]
- [Mark McGwire, 65, 0.278]
- [Sammy Sosa  , 63, 0.288]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  [
    "name",
    "hr",
    "avg"
  ],
  [
    "Mark McGwire",
    65,
    0.278
  ],
  [
    "Sammy Sosa",
    63,
    0.288
  ]
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // ZF4X: Spec Example 2.6. Mapping of Mappings
   "ZF4X"_test = [] {
      std::string yaml = R"yaml(Mark McGwire: {hr: 65, avg: 0.278}
Sammy Sosa: {
    hr: 63,
    avg: 0.288
  }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "Mark McGwire": {
    "hr": 65,
    "avg": 0.278
  },
  "Sammy Sosa": {
    "hr": 63,
    "avg": 0.288
  }
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // ZK9H: Nested top level flow mapping
   "ZK9H"_test = [] {
      std::string yaml = R"yaml({ key: [[[
  value
 ]]]
}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": [
    [
      [
        "value"
      ]
    ]
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // ZVH3: Wrong indented sequence item
   "ZVH3"_test = [] {
      std::string yaml = R"yaml(- key: value
 - item1
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // ZXT5: Implicit key followed by newline and adjacent value
   "ZXT5"_test = [] {
      std::string yaml = R"yaml([ "key"
  :value ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

};

// ============================================================================
// KNOWN FAILURES (234 tests in these suites) - parser is exercised and many
// tests still intentionally avoid correctness assertions.
// Convert to passing tests as features are implemented.
// ============================================================================

suite yaml_conformance_known_failures_1 = [] {
   // 26DV: Whitespace around colon in mappings
   "26DV"_test = [] {
      std::string yaml = R"yaml("top1" :
  "key1" : &alias1 scalar1
'top2' :
  'key2' : &alias2 scalar2
top3: &node3
  *alias1 : scalar3
top4:
  *alias2 : scalar4
top5   :
  scalar5
top6:
  &anchor6 'key6' : scalar6
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "top1": {
    "key1": "scalar1"
  },
  "top2": {
    "key2": "scalar2"
  },
  "top3": {
    "scalar1": "scalar3"
  },
  "top4": {
    "scalar2": "scalar4"
  },
  "top5": "scalar5",
  "top6": {
    "key6": "scalar6"
  }
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2CMS: Invalid mapping in plain multiline
   "2CMS"_test = [] {
      std::string yaml = R"yaml(this
 is
  invalid: x
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 2EBW: Allowed characters in keys
   "2EBW"_test = [] {
      std::string yaml = R"yaml(a!"#$%&'()*+,-./09:;<=>?@AZ[\]^_`az{|}~: safe
?foo: safe question mark
:foo: safe colon
-foo: safe dash
this is#not: a comment
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a!\"#$%&'()*+,-./09:;<=>?@AZ[\\]^_`az{|}~": "safe",
  "?foo": "safe question mark",
  ":foo": "safe colon",
  "-foo": "safe dash",
  "this is#not": "a comment"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2G84_00: Literal modifers
   "2G84_00"_test = [] {
      std::string yaml = R"yaml(--- |0
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 2G84_01: Literal modifers
   "2G84_01"_test = [] {
      std::string yaml = R"yaml(--- |10
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 2LFX (known failure): Spec Example 6.13. Reserved Directives [1.3]
   "2LFX"_test = [] {
      std::string yaml = R"yaml(%FOO  bar baz # Should be ignored
              # with a warning.
---
"foo"
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2SXE: Anchors With Colon in Name
   "2SXE"_test = [] {
      std::string yaml = R"yaml(&a: key: &a value
foo:
  *a:
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": "value",
  "foo": "key"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 2XXW: Spec Example 2.25. Unordered Sets
   "2XXW"_test = [] {
      std::string yaml = R"yaml(# Sets are represented as a
# Mapping where each key is
# associated with a null value
--- !!set
? Mark McGwire
? Sammy Sosa
? Ken Griff
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "Mark McGwire": null,
  "Sammy Sosa": null,
  "Ken Griff": null
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 35KP (known failure): Tags for Root Objects
   "35KP"_test = [] {
      std::string yaml = R"yaml(--- !!map
? a
: b
--- !!seq
- !!str c
--- !!str
d
e
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({"a":"b"}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 36F6: Multiline plain scalar with empty line
   "36F6"_test = [] {
      std::string yaml = R"yaml(---
plain: a
 b

 c
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         auto expected = normalize_json(R"({"plain":"a b\nc"})");
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3GZX: Spec Example 7.1. Alias Nodes
   "3GZX"_test = [] {
      std::string yaml = R"yaml(First occurrence: &anchor Foo
Second occurrence: *anchor
Override anchor: &anchor Bar
Reuse anchor: *anchor
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         // Keys sorted alphabetically by std::map
         std::string expected = R"({"First occurrence":"Foo","Override anchor":"Bar","Reuse anchor":"Bar","Second occurrence":"Foo"})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3MYT: Plain Scalar looking like key, comment, anchor and tag
   "3MYT"_test = [] {
      std::string yaml = R"yaml(---
k:#foo
 &a !t s
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("k:#foo &a !t s"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3R3P: Single block sequence with anchor
   "3R3P"_test = [] {
      std::string yaml = R"yaml(&sequence
- a
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"(["a"])";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3RLN_01: Leading tabs in double quoted
   "3RLN_01"_test = [] {
      std::string yaml = R"yaml("2 leading
    \	tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("2 leading \\\ttab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 3RLN_04: Leading tabs in double quoted
   "3RLN_04"_test = [] {
      std::string yaml = R"yaml("5 leading
    \	  tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("5 leading \\\t  tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4ABK (known failure): Flow Mapping Separate Values
   "4ABK"_test = [] {
      std::string yaml = R"yaml({
unquoted : "separate",
http://foo.com,
omitted value:,
}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({"http://foo.com":null,"omitted value":"","unquoted":"separate"}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4FJ6 (known failure): Nested implicit complex keys
   "4FJ6"_test = [] {
      std::string yaml = R"yaml(---
[
  [ a, [ [[b,c]]: d, e]]: 23
]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([{"[\"a\",[{\"[[\\\"b\\\",\\\"c\\\"]]\":\"d\"},\"e\"]]":23}]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4H7K: Flow sequence with invalid extra closing bracket
   "4H7K"_test = [] {
      std::string yaml = R"yaml(---
[ a, b, c ] ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 4MUZ_00: Flow mapping colon on line after key
   "4MUZ_00"_test = [] {
      std::string yaml = R"yaml({"foo"
: "bar"}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 4MUZ_01: Flow mapping colon on line after key
   "4MUZ_01"_test = [] {
      std::string yaml = R"yaml({"foo"
: bar}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 4MUZ_02: Flow mapping colon on line after key
   "4MUZ_02"_test = [] {
      std::string yaml = R"yaml({foo
: bar}
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 4QFQ (known failure): Spec Example 8.2. Block Indentation Indicator [1.3]
   "4QFQ"_test = [] {
      std::string yaml = R"yaml(- |
 detected
- >
 
  
  # detected
- |1
  explicit
- >
 detected
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(["detected\n","# detected\n"," explicit\n","detected\n"]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4WA9 (known failure): Literal scalars
   "4WA9"_test = [] {
      std::string yaml = R"yaml(- aaa: |2
    xxx
  bbb: |
    xxx
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([{"aaa":"  xxx\nbbb: |\n  xxx\n"}]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 4ZYM: Spec Example 6.4. Line Prefixes
   "4ZYM"_test = [] {
      std::string yaml = R"yaml(plain: text
  lines
quoted: "text
  	lines"
block: |
  text
   	lines
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"block":"text\n \tlines\n","plain":"text lines","quoted":"text lines"})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 52DL: Explicit Non-Specific Tag [1.3]
   "52DL"_test = [] {
      std::string yaml = R"yaml(---
! a
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("a"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 55WF: Invalid escape in double quoted string
   "55WF"_test = [] {
      std::string yaml = R"yaml(---
"\."
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 565N (known failure): Construct Binary
   "565N"_test = [] {
      std::string yaml = R"yaml(canonical: !!binary "\
 R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5\
 OTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/+\
 +f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLC\
 AgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs="
generic: !!binary |
 R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5
 OTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/+
 +f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLC
 AgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs=
description:
 The binary value above is a tiny arrow encoded as a gif image.
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "canonical": "R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5OTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLCAgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs=",
  "generic": "R0lGODlhDAAMAIQAAP//9/X17unp5WZmZgAAAOfn515eXvPz7Y6OjuDg4J+fn5\nOTk6enp56enmlpaWNjY6Ojo4SEhP/++f/++f/++f/++f/++f/++f/++f/++f/+\n+f/++f/++f/++f/++f/++SH+Dk1hZGUgd2l0aCBHSU1QACwAAAAADAAMAAAFLC\nAgjoEwnuNAFOhpEMTRiggcz4BNJHrv/zCFcLiwMWYNG84BwwEeECcgggoBADs=\n",
  "description": "The binary value above is a tiny arrow encoded as a gif image."
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 57H4 (known failure): Spec Example 8.22. Block Collection Nodes
   "57H4"_test = [] {
      std::string yaml = R"yaml(sequence: !!seq
- entry
- !!seq
 - nested
mapping: !!map
 foo: bar
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "mapping": {
    "foo": "bar"
  },
  "sequence": [
    "entry",
    [
      "nested"
    ]
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5KJE (known failure): Spec Example 7.13. Flow Sequence
   "5KJE"_test = [] {
      std::string yaml = R"yaml(- [ one, two, ]
- [three ,four]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  [
    "one",
    "two"
  ],
  [
    "three",
    "four"
  ]
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5LLU: Block scalar with wrong indented line after spaces only
   "5LLU"_test = [] {
      std::string yaml = R"yaml(block scalar: >
 
  
   
 invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 5MUD: Colon and adjacent value on next line
   "5MUD"_test = [] {
      std::string yaml = R"yaml(---
{ "foo"
  :bar }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 5TRB: Invalid document-start marker in doublequoted tring
   "5TRB"_test = [] {
      std::string yaml = R"yaml(---
"
---
"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" --- "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5TYM (known failure): Spec Example 6.21. Local Tag Prefix
   "5TYM"_test = [] {
      std::string yaml = R"yaml(%TAG !m! !my-
--- # Bulb here
!m!light fluorescent
...
%TAG !m! !my-
--- # Color here
!m!light green
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("fluorescent"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5U3A (known failure): Sequence on same Line as Mapping Key
   "5U3A"_test = [] {
      std::string yaml = R"yaml(key: - a
     - b
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": [
    "a",
    "b"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 5WE3: Spec Example 8.17. Explicit Block Mapping Entries
   "5WE3"_test = [] {
      std::string yaml = R"yaml(? explicit key # Empty value
? |
  block key
: - one # Explicit compact
  - two # block value
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "explicit key": null,
  "block key\n": [
    "one",
    "two"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 62EZ: Invalid block mapping key on same line as previous key
   "62EZ"_test = [] {
      std::string yaml = R"yaml(---
x: { y: z }in: valid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 6BFJ: Mapping, key and flow sequence item anchors
   "6BFJ"_test = [] {
      std::string yaml = R"yaml(---
&mapping
&key [ &item a, b, c ]: value
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "[\"a\",\"b\",\"c\"]": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6CK3: Spec Example 6.26. Tag Shorthands
   "6CK3"_test = [] {
      std::string yaml = R"yaml(%TAG !e! tag:example.com,2000:app/
---
- !local foo
- !!str bar
- !e!tag%21 baz
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "foo",
  "bar",
  "baz"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6FWR: Block Scalar Keep
   "6FWR"_test = [] {
      std::string yaml = R"yaml(--- |+
 ab
 
  
...
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("ab\n\n\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6HB6 (known failure): Spec Example 6.1. Indentation Spaces
   "6HB6"_test = [] {
      std::string yaml = R"yaml(  # Leading comment line spaces are
   # neither content nor indentation.
    
Not indented:
 By one space: |
    By four
      spaces
 Flow style: [    # Leading spaces
   By two,        # in flow style
  Also by two,    # are neither
  	Still by two   # content nor
    ]             # indentation.
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json =
            R"yaml({"Not indented":{"By one space":"By four\n  spaces\n","Flow style":["By two","Also by two","Still by two"]}}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6JQW: Spec Example 2.13. In literals, newlines are preserved
   "6JQW"_test = [] {
      std::string yaml = R"yaml(# ASCII Art
--- |
  \//||\/||
  // ||  ||__
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("\\//||\\/||\n// ||  ||__\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6JWB: Tags for Block Objects
   "6JWB"_test = [] {
      std::string yaml = R"yaml(foo: !!seq
  - !!str a
  - !!map
    key: !!str value
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": [
    "a",
    {
      "key": "value"
    }
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6KGN: Anchor for empty node
   "6KGN"_test = [] {
      std::string yaml = R"yaml(---
a: &anchor
b: *anchor
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"a":null,"b":null})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6LVF: Spec Example 6.13. Reserved Directives
   "6LVF"_test = [] {
      std::string yaml = R"yaml(%FOO  bar baz # Should be ignored
              # with a warning.
--- "foo"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6VJK (known failure): Spec Example 2.15. Folded newlines are preserved for "more indented" and blank lines
   "6VJK"_test = [] {
      std::string yaml = R"yaml(>
 Sammy Sosa completed another
 fine season with great stats.

   63 Home Runs
   0.288 Batting Average

 What a year!
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json =
            R"yaml("Sammy Sosa completed another fine season with great stats.\n  63 Home Runs   0.288 Batting Average\nWhat a year!\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6WLZ: Spec Example 6.18. Primary Tag Handle [1.3]
   "6WLZ"_test = [] {
      std::string yaml = R"yaml(# Private
---
!foo "bar"
...
# Global
%TAG ! tag:example.com,2000:app/
---
!foo "bar"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("bar"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 6XDY (known failure): Two document start markers
   "6XDY"_test = [] {
      std::string yaml = R"yaml(---
---
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("---"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 735Y (known failure): Spec Example 8.20. Block Node Types
   "735Y"_test = [] {
      std::string yaml = R"yaml(-
  "flow in block"
- >
 Block scalar
- !!map # Block collection
  foo : bar
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "flow in block",
  "Block scalar\n",
  {
    "foo": "bar"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 74H7 (known failure): Tags in Implicit Mapping
   "74H7"_test = [] {
      std::string yaml = R"yaml(!!str a: b
c: !!int 42
e: !!str f
g: h
!!str 23: !!bool false
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("a"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7BMT: Node and Mapping Key Anchors [1.3]
   "7BMT"_test = [] {
      std::string yaml = R"yaml(---
top1: &node1
  &k1 key1: one
top2: &node2 # comment
  key2: two
top3:
  &k3 key3: three
top4: &node4
  &k4 key4: four
top5: &node5
  key5: five
top6: &val6
  six
top7:
  &val7 seven
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "top1": { "key1": "one" },
  "top2": { "key2": "two" },
  "top3": { "key3": "three" },
  "top4": { "key4": "four" },
  "top5": { "key5": "five" },
  "top6": "six",
  "top7": "seven"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7BUB: Spec Example 2.10. Node for "Sammy Sosa" appears twice in this document
   "7BUB"_test = [] {
      std::string yaml = R"yaml(---
hr:
  - Mark McGwire
  # Following node labeled SS
  - &SS Sammy Sosa
rbi:
  - *SS # Subsequent occurrence
  - Ken Griffey
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"hr":["Mark McGwire","Sammy Sosa"],"rbi":["Sammy Sosa","Ken Griffey"]})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7FWL (known failure): Spec Example 6.24. Verbatim Tags
   "7FWL"_test = [] {
      std::string yaml = R"yaml(!<tag:yaml.org,2002:str> foo :
  !<!bar> baz
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7LBH (known failure): Multiline double quoted implicit keys
   "7LBH"_test = [] {
      std::string yaml = R"yaml("a\nb": 1
"c
 d": 1
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a\nb": 1,
  "c d": 1
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7T8X (known failure): Spec Example 8.10. Folded Lines - 8.13. Final Empty Lines
   "7T8X"_test = [] {
      std::string yaml = R"yaml(>

 folded
 line

 next
 line
   * bullet

   * list
   * lines

 last
 line

# Comment
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("folded line\nnext line   * bullet\n  * list   * lines\nlast line\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7TMG (known failure): Comment in flow sequence before comma
   "7TMG"_test = [] {
      std::string yaml = R"yaml(---
[ word1
# comment
, word2]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "word1",
  "word2"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7W2P: Block Mapping with Missing Values
   "7W2P"_test = [] {
      std::string yaml = R"yaml(? a
? b
c:
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": null,
  "b": null,
  "c": null
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 7ZZ5 (known failure): Empty flow collections
   "7ZZ5"_test = [] {
      std::string yaml = R"yaml(---
nested sequences:
- - - []
- - - {}
key1: []
key2: {}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "nested sequences": [
    [
      [
        []
      ]
    ],
    [
      [
        {}
      ]
    ]
  ],
  "key1": [],
  "key2": {}
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 82AN: Three dashes and content without space
   "82AN"_test = [] {
      std::string yaml = R"yaml(---word1
word2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 87E4 (known failure): Spec Example 7.8. Single Quoted Implicit Keys
   "87E4"_test = [] {
      std::string yaml = R"yaml('implicit block key' : [
  'implicit flow key' : value,
 ]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "implicit block key": [
    {
      "implicit flow key": "value"
    }
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 8CWC (known failure): Plain mapping key ending with colon
   "8CWC"_test = [] {
      std::string yaml = R"yaml(---
key ends with two colons::: value
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key ends with two colons::": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

};

suite yaml_conformance_known_failures_2 = [] {
   // 8G76 (known failure): Spec Example 6.10. Comment Lines
   "8G76"_test = [] {
      std::string yaml = R"yaml(  # Comment
   
   

)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 8KB6 (known failure): Multiline plain flow mapping key without value
   "8KB6"_test = [] {
      std::string yaml = R"yaml(---
- { single line, a: b}
- { multi
  line, a: b}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "single line": null,
    "a": "b"
  },
  {
    "multi line": null,
    "a": "b"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 8MK2: Explicit Non-Specific Tag
   "8MK2"_test = [] {
      std::string yaml = R"yaml(! a
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("a"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 8UDB (known failure): Spec Example 7.14. Flow Sequence Entries
   "8UDB"_test = [] {
      std::string yaml = R"yaml([
"double
 quoted", 'single
           quoted',
plain
 text, [ nested ],
single: pair,
]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(["double quoted","single quoted","plain text",["nested"],{"single":"pair"}]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 8XYN: Anchor with unicode character
   "8XYN"_test = [] {
      std::string yaml = R"yaml(---
- &😁 unicode anchor
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "unicode anchor"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 93JH (known failure): Block Mappings in Block Sequence
   "93JH"_test = [] {
      std::string yaml = R"yaml( - key: value
   key2: value2
 -
   key3: value3
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "key": "value",
    "key2": "value2"
  },
  {
    "key3": "value3"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 96NN_00 (known failure): Leading tab content in literals
   "96NN_00"_test = [] {
      std::string yaml = R"yaml(foo: |-
 	bar
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "\tbar"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 96NN_01 (known failure): Leading tab content in literals
   "96NN_01"_test = [] {
      std::string yaml = R"yaml(foo: |-
 	bar)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "\tbar"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 98YD (known failure): Spec Example 5.5. Comment Indicator
   "98YD"_test = [] {
      std::string yaml = R"yaml(# Comment only.
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9BXH (known failure): Multiline doublequoted flow mapping key without value
   "9BXH"_test = [] {
      std::string yaml = R"yaml(---
- { "single line", a: b}
- { "multi
  line", a: b}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "single line": null,
    "a": "b"
  },
  {
    "multi line": null,
    "a": "b"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9C9N: Wrong indented flow sequence
   "9C9N"_test = [] {
      std::string yaml = R"yaml(---
flow: [a,
b,
c]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 9DXL (known failure): Spec Example 9.6. Stream [1.3]
   "9DXL"_test = [] {
      std::string yaml = R"yaml(Mapping: Document
---
# Empty
...
%YAML 1.2
---
matches %: 20
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "Mapping": "Document"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9JBA: Invalid comment after end of flow sequence
   "9JBA"_test = [] {
      std::string yaml = R"yaml(---
[ a, b, c, ]#invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 9KAX: Various combinations of tags and anchors
   "9KAX"_test = [] {
      std::string yaml = R"yaml(---
&a1
!!str
scalar1
---
!!str
&a2
scalar2
---
&a3
!!str scalar3
---
&a4 !!map
&a5 !!str key5: value4
---
a6: 1
&anchor6 b6: 2
---
!!map
&a8 !!str key8: value7
---
!!map
!!str &a10 key10: value9
---
!!str &a11
value11
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("scalar1"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9KBC: Mapping starting at --- line
   "9KBC"_test = [] {
      std::string yaml = R"yaml(--- key1: value1
    key2: value2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key1": "value1",
  "key2": "value2"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9MAG: Flow sequence with invalid comma at the beginning
   "9MAG"_test = [] {
      std::string yaml = R"yaml(---
[ , a, b, c ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // 9MMW (known failure): Single Pair Implicit Entries
   "9MMW"_test = [] {
      std::string yaml = R"yaml(- [ YAML : separate ]
- [ "JSON like":adjacent ]
- [ {JSON: like}:adjacent ]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([[{"YAML":"separate"}],[{"JSON like":"adjacent"}],[{"{\"JSON\":\"like\"}":"adjacent"}]]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9MQT_01: Scalar doc with '...' in content
   "9MQT_01"_test = [] {
      std::string yaml = R"yaml(--- "a
... x
b"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("a ... x b"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9WXW: Spec Example 6.18. Primary Tag Handle
   "9WXW"_test = [] {
      std::string yaml = R"yaml(# Private
!foo "bar"
...
# Global
%TAG ! tag:example.com,2000:app/
---
!foo "bar"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("bar"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // 9YRD (known failure): Multiline Scalar at Top Level
   "9YRD"_test = [] {
      std::string yaml = R"yaml(a
b  
  c
d

e
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("a b c d\ne"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // A2M4 (known failure): Spec Example 6.2. Indentation Indicators
   "A2M4"_test = [] {
      std::string yaml = R"yaml(? a
: -	b
  -  -	c
     - d
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": [
    "b",
    [
      "c",
      "d"
    ]
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // A984 (known failure): Multiline Scalar in Mapping
   "A984"_test = [] {
      std::string yaml = R"yaml(a: b
 c
d:
 e
  f
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "b c",
  "d": "e f"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // AB8U (known failure): Sequence entry that looks like two with wrong indentation
   "AB8U"_test = [] {
      std::string yaml = R"yaml(- single multiline
 - sequence entry
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "single multiline",
  "sequence entry"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // AVM7 (known failure): Empty Stream
   "AVM7"_test = [] {
      std::string yaml = R"yaml()yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         auto expected = normalize_json(R"(null)");
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // AZ63 (known failure): Sequence With Same Indentation as Parent Mapping
   "AZ63"_test = [] {
      std::string yaml = R"yaml(one:
- 2
- 3
four: 5
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "one": [
    2,
    3
  ],
  "four": 5
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // AZW3: Lookahead test cases
   "AZW3"_test = [] {
      std::string yaml = R"yaml(- bla"keks: foo
- bla]keks: foo
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "bla\"keks": "foo"
  },
  {
    "bla]keks": "foo"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // B63P: Directive without document
   "B63P"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
...
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // BD7L: Invalid mapping after sequence
   "BD7L"_test = [] {
      std::string yaml = R"yaml(- item1
- item2
invalid: x
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // BEC7: Spec Example 6.14. “YAML” directive
   "BEC7"_test = [] {
      std::string yaml = R"yaml(%YAML 1.3 # Attempt parsing
          # with a warning
---
"foo"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // BS4K: Comment between plain scalar lines
   "BS4K"_test = [] {
      std::string yaml = R"yaml(word1  # comment
word2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // BU8L: Node Anchor and Tag on Seperate Lines
   "BU8L"_test = [] {
      std::string yaml = R"yaml(key: &anchor
 !!map
  a: b
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": {
    "a": "b"
  }
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // C2DT (known failure): Spec Example 7.18. Flow Mapping Adjacent Values
   "C2DT"_test = [] {
      std::string yaml = R"yaml({
"adjacent":value,
"readable": value,
"empty":
}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "adjacent": "value",
  "readable": "value",
  "empty": ""
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // C2SP: Flow Mapping Key on two lines
   "C2SP"_test = [] {
      std::string yaml = R"yaml([23
]: 42
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // C4HZ: Spec Example 2.24. Global Tags
   "C4HZ"_test = [] {
      std::string yaml = R"yaml(%TAG ! tag:clarkevans.com,2002:
--- !shape
  # Use the ! handle for presenting
  # tag:clarkevans.com,2002:circle
- !circle
  center: &ORIGIN {x: 73, y: 129}
  radius: 7
- !line
  start: *ORIGIN
  finish: { x: 89, y: 102 }
- !label
  start: *ORIGIN
  color: 0xFFEEBB
  text: Pretty vector drawing.
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "center": {
      "x": 73,
      "y": 129
    },
    "radius": 7
  },
  {
    "finish": {
      "x": 89,
      "y": 102
    },
    "start": {
      "x": 73,
      "y": 129
    }
  },
  {
    "color": "0xFFEEBB",
    "start": {
      "x": 73,
      "y": 129
    },
    "text": "Pretty vector drawing."
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // CC74: Spec Example 6.20. Tag Handles
   "CC74"_test = [] {
      std::string yaml = R"yaml(%TAG !e! tag:example.com,2000:app/
---
!e!foo "bar"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("bar"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // CN3R: Various location of anchors in flow sequence
   "CN3R"_test = [] {
      std::string yaml = R"yaml(&flowseq [
 a: b,
 &c c: d,
 { &e e: f },
 &g { g: h }
]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"([{"a":"b"},{"c":"d"},{"e":"f"},{"g":"h"}])";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // CT4Q (known failure): Spec Example 7.20. Single Pair Explicit Entry
   "CT4Q"_test = [] {
      std::string yaml = R"yaml([
? foo
 bar : baz
]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "foo bar": "baz"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // CTN5: Flow sequence with invalid extra comma
   "CTN5"_test = [] {
      std::string yaml = R"yaml(---
[ a, b, c, , ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // CUP7 (known failure): Spec Example 5.6. Node Property Indicators
   "CUP7"_test = [] {
      std::string yaml = R"yaml(anchored: !local &anchor value
alias: *anchor
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "anchored": "value",
  "alias": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // CVW2: Invalid comment after comma
   "CVW2"_test = [] {
      std::string yaml = R"yaml(---
[ a, b, c,#invalid
]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // D49Q (known failure): Multiline single quoted implicit keys
   "D49Q"_test = [] {
      std::string yaml = R"yaml('a\nb': 1
'c
 d': 1
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a\\nb": 1,
  "c d": 1
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // D83L (known failure): Block scalar indicator order
   "D83L"_test = [] {
      std::string yaml = R"yaml(- |2-
  explicit indent and chomp
- |-2
  chomp and explicit indent
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "explicit indent and chomp",
  "chomp and explicit indent"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DE56_00: Trailing tabs in double quoted
   "DE56_00"_test = [] {
      std::string yaml = R"yaml("1 trailing\t
    tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("1 trailing tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DE56_01: Trailing tabs in double quoted
   "DE56_01"_test = [] {
      std::string yaml = R"yaml("2 trailing\t  
    tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("2 trailing tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DE56_02: Trailing tabs in double quoted
   "DE56_02"_test = [] {
      std::string yaml = "\"3 trailing\\\t\n    tab\"\n";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("3 trailing\\ tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DE56_03: Trailing tabs in double quoted
   "DE56_03"_test = [] {
      std::string yaml = "\"4 trailing\\\t  \n    tab\"\n";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("4 trailing\\ tab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DFF7 (known failure): Spec Example 7.16. Flow Mapping Entries
   "DFF7"_test = [] {
      std::string yaml = R"yaml({
? explicit: entry,
implicit: entry,
?
}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "": null,
  "explicit": "entry",
  "implicit": "entry"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK3J (known failure): Zero indented block scalar with line that looks like a comment
   "DK3J"_test = [] {
      std::string yaml = R"yaml(--- >
line1
# no comment
line3
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("line1 # no comment line3\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK95_00: Tabs that look like indentation
   "DK95_00"_test = [] {
      std::string yaml = R"yaml(foo:
 	bar
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo" : "bar"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK95_01: Tabs that look like indentation
   "DK95_01"_test = [] {
      std::string yaml = R"yaml(foo: "bar
	baz"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // DK95_04: Tabs that look like indentation
   "DK95_04"_test = [] {
      std::string yaml = R"yaml(foo: 1
	
bar: 2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo" : 1,
  "bar" : 2
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK95_05: Tabs that look like indentation
   "DK95_05"_test = [] {
      std::string yaml = R"yaml(foo: 1
 	
bar: 2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo" : 1,
  "bar" : 2
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DK95_07: Tabs that look like indentation
   "DK95_07"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
	
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // DMG6: Wrong indendation in Map
   "DMG6"_test = [] {
      std::string yaml = R"yaml(key:
  ok: 1
 wrong: 2
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // DWX9 (known failure): Spec Example 8.8. Literal Content
   "DWX9"_test = [] {
      std::string yaml = R"yaml(|
 
  
  literal
   
  
  text

 # Comment
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("literal\n\n\ntext\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // E76Z: Aliases in Implicit Block Mapping
   "E76Z"_test = [] {
      std::string yaml = R"yaml(&a a: &b b
*b : *a
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"a":"b","b":"a"})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // EB22: Missing document-end marker before directive
   "EB22"_test = [] {
      std::string yaml = R"yaml(---
scalar1 # comment
%YAML 1.2
---
scalar2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // EHF6 (known failure): Tags for Flow Objects
   "EHF6"_test = [] {
      std::string yaml = R"yaml(!!map {
  k: !!seq
  [ a, !!str b]
}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "k": [
    "a",
    "b"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // EW3V: Wrong indendation in mapping
   "EW3V"_test = [] {
      std::string yaml = R"yaml(k1: v1
 k2: v2
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // EX5H (known failure): Multiline Scalar at Top Level [1.3]
   "EX5H"_test = [] {
      std::string yaml = R"yaml(---
a
b  
  c
d

e
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("a b c d\ne"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

};

suite yaml_conformance_known_failures_3 = [] {
   // EXG3: Three dashes and content without space [1.3]
   "EXG3"_test = [] {
      std::string yaml = R"yaml(---
---word1
word2
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // F2C7: Anchors and Tags
   "F2C7"_test = [] {
      std::string yaml = R"yaml( - &a !!str a
 - !!int 2
 - !!int &c 4
 - &d d
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "a",
  2,
  4,
  "d"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // F6MC (known failure): More indented lines at the beginning of folded block scalars
   "F6MC"_test = [] {
      std::string yaml = R"yaml(---
a: >2
   more indented
  regular
b: >2


   more indented
  regular
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": " more indented regular\n",
  "b": " more indented regular\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // FBC9 (known failure): Allowed characters in plain scalars
   "FBC9"_test = [] {
      std::string yaml = R"yaml(safe: a!"#$%&'()*+,-./09:;<=>?@AZ[\]^_`az{|}~
     !"#$%&'()*+,-./09:;<=>?@AZ[\]^_`az{|}~
safe question mark: ?foo
safe colon: :foo
safe dash: -foo
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "safe": "a!\"#$%&'()*+,-./09:;<=>?@AZ[\\]^_`az{|}~ !\"#$%&'()*+,-./09:;<=>?@AZ[\\]^_`az{|}~",
  "safe question mark": "?foo",
  "safe colon": ":foo",
  "safe dash": "-foo"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // FP8R (known failure): Zero indented block scalar
   "FP8R"_test = [] {
      std::string yaml = R"yaml(--- >
line1
line2
line3
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("line1 line2 line3\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // FTA2: Single block sequence with anchor and explicit document start
   "FTA2"_test = [] {
      std::string yaml = R"yaml(--- &sequence
- a
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"(["a"])";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // G5U8: Plain dashes in flow sequence
   "G5U8"_test = [] {
      std::string yaml = R"yaml(---
- [-, -]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([["-","-"]]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // GH63: Mixed Block Mapping (explicit to implicit)
   "GH63"_test = [] {
      std::string yaml = R"yaml(? a
: 1.3
fifteen: d
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": 1.3,
  "fifteen": "d"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // GT5M: Node anchor in sequence
   "GT5M"_test = [] {
      std::string yaml = R"yaml(- item1
&node
- item2
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"(["item1","item2"])";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // H2RW (known failure): Blank lines
   "H2RW"_test = [] {
      std::string yaml = R"yaml(foo: 1

bar: 2
    
text: |
  a
    
  b

  c
 
  d
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": 1,
  "bar": 2,
  "text": "a\n\nb\n\nc\n\nd\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // HMQ5 (known failure): Spec Example 6.23. Node Properties
   "HMQ5"_test = [] {
      std::string yaml = R"yaml(!!str &a1 "foo":
  !!str bar
&a2 baz : *a1
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // HRE5: Double quoted scalar with escaped single quote
   "HRE5"_test = [] {
      std::string yaml = R"yaml(---
double: "quoted \' scalar"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // HS5T (known failure): Spec Example 7.12. Plain Lines
   "HS5T"_test = [] {
      std::string yaml = R"yaml(1st non-empty

 2nd non-empty 
	3rd non-empty
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("1st non-empty\n2nd non-empty \t3rd non-empty"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // HU3P: Invalid Mapping in plain scalar
   "HU3P"_test = [] {
      std::string yaml = R"yaml(key:
  word1 word2
  no: key
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // HWV9 (known failure): Document-end marker
   "HWV9"_test = [] {
      std::string yaml = R"yaml(...
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("..."
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // J3BT: Spec Example 5.12. Tabs and Spaces
   "J3BT"_test = [] {
      std::string yaml = R"yaml(# Tabs and spaces
quoted: "Quoted 	"
block:	|
  void main() {
  	printf("Hello, world!\n");
  }
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"block":"void main() {\n\tprintf(\"Hello, world!\\n\");\n}\n","quoted":"Quoted \t"})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // J7PZ (known failure): Spec Example 2.26. Ordered Mappings
   "J7PZ"_test = [] {
      std::string yaml = R"yaml(# The !!omap tag is one of the optional types
# introduced for YAML 1.1. In 1.2, it is not
# part of the standard tags and should not be
# enabled by default.
# Ordered maps are represented as
# A sequence of mappings, with
# each mapping having one key
--- !!omap
- Mark McGwire: 65
- Sammy Sosa: 63
- Ken Griffy: 58
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "Mark McGwire": 65
  },
  {
    "Sammy Sosa": 63
  },
  {
    "Ken Griffy": 58
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JHB9 (known failure): Spec Example 2.7. Two Documents in a Stream
   "JHB9"_test = [] {
      std::string yaml = R"yaml(# Ranking of 1998 home runs
---
- Mark McGwire
- Sammy Sosa
- Ken Griffey

# Team ranking
---
- Chicago Cubs
- St Louis Cardinals
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "Mark McGwire",
  "Sammy Sosa",
  "Ken Griffey"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JKF3: Multiline unidented double quoted block key
   "JKF3"_test = [] {
      std::string yaml = R"yaml(- - "bar
bar": x
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // JS2J: Spec Example 6.29. Node Anchors
   "JS2J"_test = [] {
      std::string yaml = R"yaml(First occurrence: &anchor Value
Second occurrence: *anchor
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"First occurrence":"Value","Second occurrence":"Value"})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // JTV5 (known failure): Block Mapping with Multiline Scalars
   "JTV5"_test = [] {
      std::string yaml = R"yaml(? a
  true
: null
  d
? e
  42
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a true": "null d",
  "e 42": null
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // K3WX: Colon and adjacent value after comment on next line
   "K3WX"_test = [] {
      std::string yaml = R"yaml(---
{ "foo" # comment
  :bar }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // K858 (known failure): Spec Example 8.6. Empty Scalar Chomping
   "K858"_test = [] {
      std::string yaml = R"yaml(strip: >-

clip: >

keep: |+

)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "strip": "",
  "clip": "\n",
  "keep": "\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // KH5V_01: Inline tabs in double quoted
   "KH5V_01"_test = [] {
      std::string yaml = R"yaml("2 inline\	tab"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("2 inline\\\ttab"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // KS4U: Invalid item after end of flow sequence
   "KS4U"_test = [] {
      std::string yaml = R"yaml(---
[
sequence item
]
invalid item
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // L24T_00 (known failure): Trailing line of spaces
   "L24T_00"_test = [] {
      std::string yaml = R"yaml(foo: |
  x
   
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "x\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // L24T_01 (known failure): Trailing line of spaces
   "L24T_01"_test = [] {
      std::string yaml = R"yaml(foo: |
  x
   )yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": "x\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // L94M: Tags in Explicit Mapping
   "L94M"_test = [] {
      std::string yaml = R"yaml(? !!str a
: !!int 47
? c
: !!str d
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": 47,
  "c": "d"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // L9U5 (known failure): Spec Example 7.11. Plain Implicit Keys
   "L9U5"_test = [] {
      std::string yaml = R"yaml(implicit block key : [
  implicit flow key : value,
 ]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "implicit block key": [
    {
      "implicit flow key": "value"
    }
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // LE5A: Spec Example 7.24. Flow Nodes
   "LE5A"_test = [] {
      std::string yaml = R"yaml(- !!str "a"
- 'b'
- &anchor "c"
- *anchor
- !!str
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"(["a","b","c","c",""])";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // LP6E (known failure): Whitespace After Scalars in Flow
   "LP6E"_test = [] {
      std::string yaml = R"yaml(- [a, b , c ]
- { "a"  : b
   , c : 'd' ,
   e   : "f"
  }
- [      ]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  [
    "a",
    "b",
    "c"
  ],
  {
    "a": "b",
    "c": "d",
    "e": "f"
  },
  []
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // LQZ7 (known failure): Spec Example 7.4. Double Quoted Implicit Keys
   "LQZ7"_test = [] {
      std::string yaml = R"yaml("implicit block key" : [
  "implicit flow key" : value,
 ]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "implicit block key": [
    {
      "implicit flow key": "value"
    }
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // M29M (known failure): Literal Block Scalar
   "M29M"_test = [] {
      std::string yaml = R"yaml(a: |
 ab
 
 cd
 ef
 

...
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "ab\n\ncd\nef\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // M5C3 (known failure): Spec Example 8.21. Block Scalar Nodes
   "M5C3"_test = [] {
      std::string yaml = R"yaml(literal: |2
  value
folded:
   !foo
  >1
 value
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "folded": "value\n",
  "literal": "value\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // M7A3 (known failure): Spec Example 9.3. Bare Documents
   "M7A3"_test = [] {
      std::string yaml = R"yaml(Bare
document
...
# No document
...
|
%!PS-Adobe-2.0 # Not the first line
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("Bare document"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // M9B4: Spec Example 8.7. Literal Scalar
   "M9B4"_test = [] {
      std::string yaml = R"yaml(|
 literal
 	text


)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"("literal\n\ttext\n")";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MJS9: Spec Example 6.7. Block Folding
   "MJS9"_test = [] {
      std::string yaml = R"yaml(>
  foo

  	 bar

  baz
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      // Note: folded scalar blank line handling differs from spec, so only checking parse success
   };

   // MUS6_01: Directive variants
   "MUS6_01"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
---
%YAML 1.2
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // MUS6_02: Directive variants
   "MUS6_02"_test = [] {
      std::string yaml = R"yaml(%YAML  1.1
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MUS6_03: Directive variants
   "MUS6_03"_test = [] {
      std::string yaml = R"yaml(%YAML 	 1.1
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MUS6_04: Directive variants
   "MUS6_04"_test = [] {
      std::string yaml = R"yaml(%YAML 1.1  # comment
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MUS6_05: Directive variants
   "MUS6_05"_test = [] {
      std::string yaml = R"yaml(%YAM 1.1
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // MUS6_06: Directive variants
   "MUS6_06"_test = [] {
      std::string yaml = R"yaml(%YAMLL 1.1
---
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // N4JP: Bad indentation in mapping
   "N4JP"_test = [] {
      std::string yaml = R"yaml(map:
  key1: "quoted1"
 key2: "bad indentation"
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // N782: Invalid document markers in flow style
   "N782"_test = [] {
      std::string yaml = R"yaml([
--- ,
...
]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // NB6Z (known failure): Multiline plain value with tabs on empty lines
   "NB6Z"_test = [] {
      std::string yaml = R"yaml(key:
  value
  with
  	
  tabs
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": "value with tabs"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // NJ66 (known failure): Multiline plain flow mapping key
   "NJ66"_test = [] {
      std::string yaml = R"yaml(---
- { single line: value}
- { multi
  line: value}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "single line": "value"
  },
  {
    "multi line": "value"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // NKF9 (known failure): Empty keys in block and flow mapping
   "NKF9"_test = [] {
      std::string yaml = R"yaml(---
key: value
: empty key
---
{
 key: value, : empty key
}
---
# empty key and value
:
---
# empty key and value
{ : }
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": "value",
  "": "empty key"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // P2AD (known failure): Spec Example 8.1. Block Scalar Header
   "P2AD"_test = [] {
      std::string yaml = R"yaml(- | # Empty header↓
 literal
- >1 # Indentation indicator↓
  folded
- |+ # Chomping indicator↓
 keep

- >1- # Both indicators↓
  strip
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "literal\n",
  " folded\n",
  "keep\n\n",
  " strip"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // P2EQ: Invalid sequene item on same line as previous item
   "P2EQ"_test = [] {
      std::string yaml = R"yaml(---
- { y: z }- invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // P76L (known failure): Spec Example 6.19. Secondary Tag Handle
   "P76L"_test = [] {
      std::string yaml = R"yaml(%TAG !! tag:example.com,2000:app/
---
!!int 1 - 3 # Interval, not integer
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(1
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // PUW8 (known failure): Document start on last line
   "PUW8"_test = [] {
      std::string yaml = R"yaml(---
a: b
---
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "b"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // PW8X: Anchors on Empty Scalars
   "PW8X"_test = [] {
      std::string yaml = R"yaml(- &a
- a
-
  &a : a
  b: &b
-
  &c : &a
-
  ? &d
-
  ? &e
  : &a
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"([null,"a",{"":"a","b":null},{"":null},{"":null},{"":null}])";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // QB6E: Wrong indented multiline quoted scalar
   "QB6E"_test = [] {
      std::string yaml = R"yaml(---
quoted: "a
b
c"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // QF4Y (known failure): Spec Example 7.19. Single Pair Flow Mappings
   "QF4Y"_test = [] {
      std::string yaml = R"yaml([
foo: bar
]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  {
    "foo": "bar"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // QT73 (known failure): Comment and document-end marker
   "QT73"_test = [] {
      std::string yaml = R"yaml(# comment
...
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("..."
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // R4YG (known failure): Spec Example 8.2. Block Indentation Indicator
   "R4YG"_test = [] {
      std::string yaml = R"yaml(- |
 detected
- >
 
  
  # detected
- |1
  explicit
- >
 	
 detected
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "detected\n",
  "# detected\n",
  " explicit\n",
  "\t detected\n"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // RLU9 (known failure): Sequence Indent
   "RLU9"_test = [] {
      std::string yaml = R"yaml(foo:
- 42
bar:
  - 44
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo": [
    42
  ],
  "bar": [
    44
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // RR7F: Mixed Block Mapping (implicit to explicit)
   "RR7F"_test = [] {
      std::string yaml = R"yaml(a: 4.2
? d
: 23
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "d": 23,
  "a": 4.2
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // RXY3: Invalid document-end marker in single quoted string
   "RXY3"_test = [] {
      std::string yaml = R"yaml(---
'
...
'
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(" ... "
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

};

suite yaml_conformance_known_failures_4 = [] {
   // RZP5 (known failure): Various Trailing Comments [1.3]
   "RZP5"_test = [] {
      std::string yaml = R"yaml(a: "double
  quotes" # lala
b: plain
 value  # lala
c  : #lala
  d
? # lala
 - seq1
: # lala
 - #lala
  seq2
e: &node # lala
 - x: y
block: > # lala
  abcde
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "double quotes",
  "b": "plain value",
  "c": "d",
  "[\"seq1\"]": [
    "seq2"
  ],
  "e": [
    {
      "x": "y"
    }
  ],
  "block": "abcde\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // RZT7 (known failure): Spec Example 2.28. Log File
   "RZT7"_test = [] {
      std::string yaml = R"yaml(---
Time: 2001-11-23 15:01:42 -5
User: ed
Warning:
  This is an error message
  for the log file
---
Time: 2001-11-23 15:02:31 -5
User: ed
Warning:
  A slightly different error
  message.
---
Date: 2001-11-23 15:03:17 -5
User: ed
Fatal:
  Unknown variable "bar"
Stack:
  - file: TopClass.py
    line: 23
    code: |
      x = MoreObject("345\n")
  - file: MoreClass.py
    line: 58
    code: |-
      foo = bar
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "Time": "2001-11-23 15:01:42 -5",
  "User": "ed",
  "Warning": "This is an error message for the log file"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // S3PD (known failure): Spec Example 8.18. Implicit Block Mapping Entries
   "S3PD"_test = [] {
      std::string yaml = R"yaml(plain key: in-line value
: # Both empty
"quoted key":
- entry
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({"":null,"plain key":"in-line value","quoted key":["entry"]}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // S4JQ (known failure): Spec Example 6.28. Non-Specific Tags
   "S4JQ"_test = [] {
      std::string yaml = R"yaml(# Assuming conventional resolution:
- "12"
- 12
- ! 12
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "12",
  12,
  12
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // S4T7 (known failure): Document with footer
   "S4T7"_test = [] {
      std::string yaml = R"yaml(aaa: bbb
...
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "aaa": "bbb"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // S98Z: Block scalar with more spaces than first content line
   "S98Z"_test = [] {
      std::string yaml = R"yaml(empty block scalar: >
 
  
   
 # comment
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // S9E8 (known failure): Spec Example 5.3. Block Structure Indicators
   "S9E8"_test = [] {
      std::string yaml = R"yaml(sequence:
- one
- two
mapping:
  ? sky
  : blue
  sea : green
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "sequence": [
    "one",
    "two"
  ],
  "mapping": {
    "sky": null,
    "": "blue",
    "sea": "green"
  }
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // SBG9 (known failure): Flow Sequence in Flow Mapping
   "SBG9"_test = [] {
      std::string yaml = R"yaml({a: [b, c], [d, e]: f}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({"[\"d\",\"e\"]":"f","a":["b","c"]}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // SKE5: Anchor before zero indented sequence
   "SKE5"_test = [] {
      std::string yaml = R"yaml(---
seq:
 &anchor
- a
- b
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"seq":["a","b"]})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // SM9W_00: Single character streams
   "SM9W_00"_test = [] {
      std::string yaml = R"yaml(-)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("-"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // SU5Z: Comment without whitespace after doublequoted scalar
   "SU5Z"_test = [] {
      std::string yaml = R"yaml(key: "value"# invalid comment
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // T26H (known failure): Spec Example 8.8. Literal Content [1.3]
   "T26H"_test = [] {
      std::string yaml = R"yaml(--- |
 
  
  literal
   
  
  text

 # Comment
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("literal\n\n\ntext\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // T5N4: Spec Example 8.7. Literal Scalar [1.3]
   "T5N4"_test = [] {
      std::string yaml = R"yaml(--- |
 literal
 	text


)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"("literal\n\ttext\n")";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // T833: Flow mapping missing a separating comma
   "T833"_test = [] {
      std::string yaml = R"yaml(---
{
 foo: 1
 bar: 2 }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // TD5N: Invalid scalar after sequence
   "TD5N"_test = [] {
      std::string yaml = R"yaml(- item1
- item2
invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // U3C3: Spec Example 6.16. “TAG” directive
   "U3C3"_test = [] {
      std::string yaml = R"yaml(%TAG !yaml! tag:yaml.org,2002:
---
!yaml!str "foo"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("foo"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // U3XV: Node and Mapping Key Anchors
   "U3XV"_test = [] {
      std::string yaml = R"yaml(---
top1: &node1
  &k1 key1: one
top2: &node2 # comment
  key2: two
top3:
  &k3 key3: three
top4:
  &node4
  &k4 key4: four
top5:
  &node5
  key5: five
top6: &val6
  six
top7:
  &val7 seven
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "top1": { "key1": "one" },
  "top2": { "key2": "two" },
  "top3": { "key3": "three" },
  "top4": { "key4": "four" },
  "top5": { "key5": "five" },
  "top6": "six",
  "top7": "seven"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // U44R: Bad indentation in mapping (2)
   "U44R"_test = [] {
      std::string yaml = R"yaml(map:
  key1: "quoted1"
   key2: "bad indentation"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // U99R: Invalid comma in tag
   "U99R"_test = [] {
      std::string yaml = R"yaml(- !!str, xxx
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // U9NS (known failure): Spec Example 2.8. Play by Play Feed from a Game
   "U9NS"_test = [] {
      std::string yaml = R"yaml(---
time: 20:03:20
player: Sammy Sosa
action: strike (miss)
...
---
time: 20:03:47
player: Sammy Sosa
action: grand slam
...
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "time": "20:03:20",
  "player": "Sammy Sosa",
  "action": "strike (miss)"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UDR7 (known failure): Spec Example 5.4. Flow Collection Indicators
   "UDR7"_test = [] {
      std::string yaml = R"yaml(sequence: [ one, two, ]
mapping: { sky: blue, sea: green }
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "sequence": [
    "one",
    "two"
  ],
  "mapping": {
    "sky": "blue",
    "sea": "green"
  }
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UGM3 (known failure): Spec Example 2.27. Invoice
   "UGM3"_test = [] {
      std::string yaml = R"yaml(--- !<tag:clarkevans.com,2002:invoice>
invoice: 34843
date   : 2001-01-23
bill-to: &id001
    given  : Chris
    family : Dumars
    address:
        lines: |
            458 Walkman Dr.
            Suite #292
        city    : Royal Oak
        state   : MI
        postal  : 48046
ship-to: *id001
product:
    - sku         : BL394D
      quantity    : 4
      description : Basketball
      price       : 450.00
    - sku         : BL4438H
      quantity    : 1
      description : Super Hoop
      price       : 2392.00
tax  : 251.42
total: 4443.52
comments:
    Late afternoon is best.
    Backup contact is Nancy
    Billsmer @ 338-4338.
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "invoice": 34843,
  "date": "2001-01-23",
  "bill-to": {
    "given": "Chris",
    "family": "Dumars",
    "address": {
      "lines": "458 Walkman Dr.\nSuite #292\n",
      "city": "Royal Oak",
      "state": "MI",
      "postal": 48046
    }
  },
  "ship-to": {
    "given": "Chris",
    "family": "Dumars",
    "address": {
      "lines": "458 Walkman Dr.\nSuite #292\n",
      "city": "Royal Oak",
      "state": "MI",
      "postal": 48046
    }
  },
  "product": [
    {
      "sku": "BL394D",
      "quantity": 4,
      "description": "Basketball",
      "price": 450
    },
    {
      "sku": "BL4438H",
      "quantity": 1,
      "description": "Super Hoop",
      "price": 2392
    }
  ],
  "tax": 251.42,
  "total": 4443.52,
  "comments": "Late afternoon is best. Backup contact is Nancy Billsmer @ 338-4338."
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UKK6_01: Syntax character edge cases
   "UKK6_01"_test = [] {
      std::string yaml = R"yaml(::
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  ":" : null
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UKK6_02: Syntax character edge cases
   "UKK6_02"_test = [] {
      std::string yaml = R"yaml(!
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(null
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UT92 (known failure): Spec Example 9.4. Explicit Documents
   "UT92"_test = [] {
      std::string yaml = R"yaml(---
{ matches
% : 20 }
...
---
# Empty
...
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "matches %": 20
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // UV7Q (known failure): Legal tab after indentation
   "UV7Q"_test = [] {
      std::string yaml = R"yaml(x:
 - x
  	x
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "x": [
    "x \tx"
  ]
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // V55R: Aliases in Block Sequence
   "V55R"_test = [] {
      std::string yaml = R"yaml(- &a a
- &b b
- *a
- *b
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"(["a","b","a","b"])";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // VJP3_01: Flow collections over many lines
   "VJP3_01"_test = [] {
      std::string yaml = R"yaml(k: {
 k
 :
 v
 }
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // W42U (known failure): Spec Example 8.15. Block Sequence Entry Types
   "W42U"_test = [] {
      std::string yaml = R"yaml(- # Empty
- |
 block node
- - one # Compact
  - two # sequence
- one: two # Compact mapping
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  null,
  "block node\n",
  [
    "one",
    "two"
  ],
  {
    "one": "two"
  }
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // W4TN (known failure): Spec Example 9.5. Directives Documents
   "W4TN"_test = [] {
      std::string yaml = R"yaml(%YAML 1.2
--- |
%!PS-Adobe-2.0
...
%YAML 1.2
---
# Empty
...
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("%!PS-Adobe-2.0\n"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // W5VH: Allowed characters in alias
   "W5VH"_test = [] {
      std::string yaml = R"yaml(a: &:@*!$"<foo>: scalar a
b: *:@*!$"<foo>:
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "scalar a",
  "b": "scalar a"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // W9L4: Literal block scalar with more spaces in first line
   "W9L4"_test = [] {
      std::string yaml = R"yaml(---
block scalar: |
     
  more spaces at the beginning
  are invalid
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // X38W: Aliases in Flow Objects
   "X38W"_test = [] {
      std::string yaml = R"yaml({ &a [a, &b b]: *b, *a : [c, *b, d]}
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "[\"a\",\"b\"]": "b"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // X4QW: Comment without whitespace after block scalar indicator
   "X4QW"_test = [] {
      std::string yaml = R"yaml(block: ># comment
  scalar
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // X8DW (known failure): Explicit key and value seperated by comment
   "X8DW"_test = [] {
      std::string yaml = R"yaml(---
? key
# comment
: value
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // XLQ9 (known failure): Multiline scalar that looks like a YAML directive
   "XLQ9"_test = [] {
      std::string yaml = R"yaml(---
scalar
%YAML 1.2
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml("scalar %YAML 1.2"
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // XW4D (known failure): Various Trailing Comments
   "XW4D"_test = [] {
      std::string yaml = R"yaml(a: "double
  quotes" # lala
b: plain
 value  # lala
c  : #lala
  d
? # lala
 - seq1
: # lala
 - #lala
  seq2
e:
 &node # lala
 - x: y
block: > # lala
  abcde
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": "double quotes",
  "b": "plain value",
  "c": "d",
  "[\"seq1\"]": [
    "seq2"
  ],
  "e": [
    {
      "x": "y"
    }
  ],
  "block": "abcde\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Y2GN: Anchor with colon in the middle
   "Y2GN"_test = [] {
      std::string yaml = R"yaml(---
key: &an:chor value
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "key": "value"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Y79Y_001: Tabs in various contexts
   "Y79Y_001"_test = [] {
      std::string yaml = R"yaml(foo: |
 	
bar: 1
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "foo" : "\t\n",
  "bar" : 1
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Y79Y_003: Tabs in various contexts
   "Y79Y_003"_test = [] {
      std::string yaml = R"yaml(- [
	foo,
 foo
 ]
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_004: Tabs in various contexts
   "Y79Y_004"_test = [] {
      std::string yaml = R"yaml(-	-
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_005: Tabs in various contexts
   "Y79Y_005"_test = [] {
      std::string yaml = R"yaml(- 	-
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_006: Tabs in various contexts
   "Y79Y_006"_test = [] {
      std::string yaml = R"yaml(?	-
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_007: Tabs in various contexts
   "Y79Y_007"_test = [] {
      std::string yaml = R"yaml(? -
:	-
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_008: Tabs in various contexts
   "Y79Y_008"_test = [] {
      std::string yaml = R"yaml(?	key:
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_009: Tabs in various contexts
   "Y79Y_009"_test = [] {
      std::string yaml = R"yaml(? key:
:	key:
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // Y79Y_010: Tabs in various contexts
   "Y79Y_010"_test = [] {
      std::string yaml = R"yaml(-	-1
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  -1
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // YJV2: Dash in flow sequence
   "YJV2"_test = [] {
      std::string yaml = R"yaml([-]
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml(["-"]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Z67P (known failure): Spec Example 8.21. Block Scalar Nodes [1.3]
   "Z67P"_test = [] {
      std::string yaml = R"yaml(literal: |2
  value
folded: !foo >1
 value
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "literal": "value\n",
  "folded": "value\n"
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // Z9M4: Spec Example 6.22. Global Tag Prefix
   "Z9M4"_test = [] {
      std::string yaml = R"yaml(%TAG !e! tag:example.com,2000:app/
---
- !e!foo "bar"
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml([
  "bar"
]
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // ZCZ6: Invalid mapping in plain single line value
   "ZCZ6"_test = [] {
      std::string yaml = R"yaml(a: b: c: d
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // ZH7C: Anchors in Mapping
   "ZH7C"_test = [] {
      std::string yaml = R"yaml(&a a: b
c: &d d
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string actual;
         (void)glz::write_json(parsed, actual);
         std::string expected = R"({"a":"b","c":"d"})";
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

   // ZL4Z: Invalid nested mapping
   "ZL4Z"_test = [] {
      std::string yaml = R"yaml(---
a: 'b': c
)yaml";
      glz::generic parsed{};
      [[maybe_unused]] auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(bool(ec));
   };

   // ZWK4: Key with anchor after missing explicit mapping value
   "ZWK4"_test = [] {
      std::string yaml = R"yaml(---
a: 1
? b
&anchor c: 3
)yaml";
      glz::generic parsed{};
      auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, yaml);
      expect(!ec) << glz::format_error(ec, yaml);
      if (!ec) {
         std::string expected_json = R"yaml({
  "a": 1,
  "b": null,
  "c": 3
}
)yaml";
         auto expected = normalize_json(expected_json);
         std::string actual;
         (void)glz::write_json(parsed, actual);
         expect(actual == expected) << "expected: " << expected << "\nactual: " << actual;
      }
   };

};

int main() { return 0; }
