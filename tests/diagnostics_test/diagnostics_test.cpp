// Glaze Library
// For the license information refer to glaze.hpp
//
// Tests for the object member diagnostics: the compile-time report a format produces when a struct
// has a member it cannot write or read.
//
// The diagnostics are compile-time only, so they are tested from both sides. Everything an ordinary
// translation unit can check is here: the predicate that decides which member is a problem, the key
// and type the report names, that the concepts stay queryable, and that nothing is reported for the
// members an operation skips by design. The rejection itself is covered by the cases next to this
// file, which are compiled by check_diagnostic.cmake.

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "cases/shapes.hpp"

#include <glaze/beve.hpp>
#include <glaze/bson.hpp>
#include <glaze/cbor.hpp>
#include <glaze/csv.hpp>
#include <glaze/json.hpp>
#include <glaze/jsonb.hpp>
#include <glaze/msgpack.hpp>
#include <glaze/toml.hpp>
#include <glaze/yaml.hpp>

#include "ut/ut.hpp"

using namespace ut;

namespace
{
   // Every member has a writer and a reader, so nothing is diagnosed and every format can handle it.
   struct Supported
   {
      int a{100};
      std::string s{"world"};
   };

   // The member at index 1 has no writer: `glz::meta::skip` says to leave it out of serialization.
   struct SkippedForSerialize
   {
      int a{1};
      Opaque o{};
   };

   // The mirror image: skipped on the way in, so only a reader would have a problem with it.
   struct SkippedForParse
   {
      int a{1};
      Opaque o{};
   };

   // Skipped in both directions, by the member type rather than by the meta.
   struct WithSkipMembers
   {
      int a{1};
      glz::skip o{};
      int b{2};
   };

   // The shape an RPC registry has: callable members that a format is expected to pass over.
   struct WithFunctions
   {
      int a{1};
      int (*free_function)(int){};
      int (WithFunctions::*member_function)(int){};
   };

   // BSON emits the members below itself, inside its member loop, because a BSON element carries its
   // type next to its key and `to<BSON, T>::type_code` cannot express them: nullable, null and
   // variant members are written even though `to<BSON, ...>` has no specialization for them.
   struct BsonInlineMembers
   {
      std::optional<int> engaged{42};
      std::monostate empty{};
      std::variant<int, std::string> choice{std::string{"picked"}};
   };

   // CSV writes a struct of columns rather than a row, so a member has to be a container.
   struct CsvColumns
   {
      std::vector<int> num1{};
      std::vector<float> num2{};
   };
}

template <>
struct glz::meta<SkippedForSerialize>
{
   using T = SkippedForSerialize;
   static constexpr auto value = glz::object(&T::a, &T::o);

   static constexpr bool skip(const std::string_view key, const glz::meta_context& ctx)
   {
      return key == "o" && ctx.op == glz::operation::serialize;
   }
};

template <>
struct glz::meta<SkippedForParse>
{
   using T = SkippedForParse;
   static constexpr auto value = glz::object(&T::a, &T::o);

   static constexpr bool skip(const std::string_view key, const glz::meta_context& ctx)
   {
      return key == "o" && ctx.op == glz::operation::parse;
   }
};

template <>
struct glz::meta<CsvColumns>
{
   using T = CsvColumns;
   static constexpr auto value = glz::object(&T::num1, &T::num2);
};

namespace
{
   // ---- which member is a problem -------------------------------------------------------------
   //
   // `first_unsupported_member` is the half of the diagnostic that decides, and it is separable from
   // the half that reports: naming it here does not instantiate the report.

   static_assert(!glz::detail::member_supported<glz::operation::serialize, glz::JSON, Outer, 1>());
   static_assert(!glz::detail::member_supported<glz::operation::parse, glz::JSON, Outer, 1>());
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::JSON, Outer, 0>());
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::JSON, Outer, 2>());

   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::JSON, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::parse, glz::JSON, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::JSON, Supported> == 2);
   static_assert(glz::detail::first_unsupported_member<glz::operation::parse, glz::JSON, Supported> == 2);

   // The same member is the problem in every format, and a struct that is fine in one is fine in all.
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::JSONB, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::BEVE, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::CBOR, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::MSGPACK, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::BSON, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::YAML, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::TOML, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::CSV, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::parse, glz::BEVE, Outer> == 1);
   static_assert(glz::detail::first_unsupported_member<glz::operation::parse, glz::YAML, Outer> == 1);

   static_assert(glz::detail::writable_members<glz::JSON, Supported>);
   static_assert(glz::detail::readable_members<glz::JSON, Supported>);
   static_assert(glz::detail::writable_members<glz::JSON, WithFunctions>);

   // The member count has to agree with the reflection it stands in for, for a type that has
   // linkage, in both reflection modes: reflectable aggregates are counted through the tie,
   // everything else through reflect<T>.
   static_assert(glz::detail::member_count<Outer>() == glz::reflect<Outer>::size);
   static_assert(glz::detail::member_count<Inner>() == glz::reflect<Inner>::size);
   static_assert(glz::detail::member_count<Supported>() == glz::reflect<Supported>::size);
   static_assert(glz::detail::member_count<Renamed>() == glz::reflect<Renamed>::size);
   static_assert(glz::detail::member_count<Outer>() == 3);

   // Members a format writes itself are not missing support. BSON is the case that exists today:
   // `write_supported` is false for all three of these, yet the format writes all three.
   static_assert(!glz::write_supported<std::optional<int>, glz::BSON>);
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::BSON, BsonInlineMembers, 0>());
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::BSON, BsonInlineMembers, 1>());
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::BSON, BsonInlineMembers, 2>());
   static_assert(glz::detail::writable_members<glz::BSON, BsonInlineMembers>);
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::BSON, BsonInlineMembers> == 3);
   // The exemption is the writer's, not the reader's: the reader has real from<BSON, ...>
   // specializations for these, so nothing is exempted there.
   static_assert(glz::read_supported<std::optional<int>, glz::BSON>);

   // ---- what the report says the member is ----------------------------------------------------

   static_assert(glz::detail::member_key_of<Outer, 1>.sv() == "o");
   static_assert(glz::detail::member_type_of<Outer, 1>.sv() == "Opaque");
   static_assert(glz::detail::member_key_of<Outer, 0>.sv() == "a");
   // A renamed member is reported under the key the output uses, which is what a reader of the
   // document can search for.
   static_assert(glz::detail::member_key_of<Renamed, 1>.sv() == "omicron");

   // ---- members an operation skips by design are not problems ---------------------------------
   //
   // `glz::meta::skip` exists so that a field whose type has no writer or reader can sit in a struct
   // without breaking it, so a skip that covers the operation must silence the diagnostic for it --
   // and only for that operation.

   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::JSON, SkippedForSerialize, 1>());
   static_assert(!glz::detail::member_supported<glz::operation::parse, glz::JSON, SkippedForSerialize, 1>());
   static_assert(glz::detail::member_supported<glz::operation::parse, glz::JSON, SkippedForParse, 1>());
   static_assert(!glz::detail::member_supported<glz::operation::serialize, glz::JSON, SkippedForParse, 1>());

   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::JSON, SkippedForSerialize> ==
                 2);
   static_assert(glz::detail::first_unsupported_member<glz::operation::parse, glz::JSON, SkippedForParse> == 2);

   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::JSON, WithSkipMembers, 1>());
   static_assert(glz::detail::member_supported<glz::operation::parse, glz::JSON, WithSkipMembers, 1>());
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::JSON, WithSkipMembers> == 3);

   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::JSON, WithFunctions, 1>());
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::JSON, WithFunctions, 2>());
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::JSON, WithFunctions> == 3);

   // Which formats a function-pointer member is passed over by is not uniform: JSON and BSON drop
   // both kinds, JSONB, BEVE and CBOR drop both, TOML drops member function pointers, and YAML,
   // MSGPACK and CSV drop neither -- writing this struct as MSGPACK or YAML fails today. The check
   // therefore exempts function pointers everywhere rather than tracking that table, which is the
   // lenient direction: it means such a member is never *reported* by this diagnostic, not that every
   // format writes it. The assertions below are the exemption, stated where it is doing work -- a
   // format with no writer for either kind.
   static_assert(glz::write_supported<int (WithFunctions::*)(int), glz::JSON>);
   static_assert(!glz::write_supported<int (*)(int), glz::MSGPACK>);
   static_assert(!glz::write_supported<int (WithFunctions::*)(int), glz::MSGPACK>);
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::MSGPACK, WithFunctions, 1>());
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::MSGPACK, WithFunctions, 2>());
   static_assert(glz::detail::first_unsupported_member<glz::operation::serialize, glz::MSGPACK, WithFunctions> == 3);
   static_assert(!glz::write_supported<int (*)(int), glz::YAML>);
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::YAML, WithFunctions, 1>());
   static_assert(!glz::write_supported<int (*)(int), glz::JSONB>);
   static_assert(glz::detail::member_supported<glz::operation::serialize, glz::JSONB, WithFunctions, 1>());

   // ---- asking must remain possible -----------------------------------------------------------
   //
   // This is the reason the diagnostic lives inside `op()` instead of at class scope on
   // `to<Format, T>`: `write_supported` is `requires { to<Format, T>{}; }`, so an assert in the
   // class body fires while the concept is being evaluated, turning a query that generic code
   // depends on into a hard error. What is asserted here is that the query is evaluable at all, not
   // what it answers -- `write_supported` tests for the presence of `to<JSON, T>`, which exists for
   // a reflectable struct whatever its members are typed, so it answers true for `Outer` today.

   template <class T>
   consteval bool json_writer_is_queryable()
   {
      if constexpr (glz::write_supported<T, glz::JSON>) {
         return true;
      }
      else {
         return false;
      }
   }

   template <class T>
   consteval bool json_reader_is_queryable()
   {
      if constexpr (glz::read_supported<T, glz::JSON>) {
         return true;
      }
      else {
         return false;
      }
   }

   static_assert(json_writer_is_queryable<Outer>() || !json_writer_is_queryable<Outer>());
   static_assert(json_reader_is_queryable<Outer>() || !json_reader_is_queryable<Outer>());
   static_assert(json_writer_is_queryable<Supported>() || !json_writer_is_queryable<Supported>());
}

suite diagnostics_tests = [] {
   "every format round trips a struct with only supported members"_test = [] {
      Supported v{};
      std::string text;
      std::vector<std::byte> binary;

      expect(!glz::write_json(v, text));
      expect(!glz::read_json(v, text));
      expect(v.a == 100);
      expect(v.s == "world");

      expect(!glz::write_jsonb(v, binary));
      expect(!glz::read_jsonb(v, binary));
      expect(v.a == 100);
      expect(v.s == "world");

      expect(!glz::write_beve(v, binary));
      expect(!glz::read_beve(v, binary));
      expect(v.a == 100);
      expect(v.s == "world");

      expect(!glz::write_cbor(v, binary));
      expect(!glz::read_cbor(v, binary));
      expect(v.a == 100);
      expect(v.s == "world");

      expect(!glz::write_msgpack(v, binary));
      expect(!glz::read_msgpack(v, binary));
      expect(v.a == 100);
      expect(v.s == "world");

      expect(!glz::write_bson(v, binary));
      expect(!glz::read_bson(v, binary));
      expect(v.a == 100);
      expect(v.s == "world");

      expect(!glz::write_yaml(v, text));
      expect(!glz::read_yaml(v, text));
      expect(v.a == 100);
      expect(v.s == "world");

      expect(!glz::write_toml(v, text));
      expect(!glz::read_toml(v, text));
      expect(v.a == 100);
      expect(v.s == "world");
   };

   "a member skipped for serialization is written without"_test = [] {
      SkippedForSerialize v{};
      std::string text;
      expect(!glz::write_json(v, text));
      expect(text == R"({"a":1})") << text;
   };

   "a member skipped for parsing does not reject the read"_test = [] {
      SkippedForParse v{};
      expect(!glz::read_json(v, R"({"a":42})"));
      expect(v.a == 42);
   };

   "glz::skip members and function pointers are passed over"_test = [] {
      WithSkipMembers skipped{};
      std::string text;
      expect(!glz::write_json(skipped, text));
      expect(text == R"({"a":1,"b":2})") << text;

      WithFunctions functions{};
      std::string callables;
      expect(!glz::write_json(functions, callables));
      expect(callables == R"({"a":1})") << callables;


      // MSGPACK is one of the formats that does not pass function pointers over, so it is not
      // written here: the point of the assertions above is that this diagnostic stays out of its way,
      // not that every format accepts the struct.
   };

   "BSON writes the members it emits itself"_test = [] {
      BsonInlineMembers v{};
      std::string bytes;
      expect(!glz::write_bson(v, bytes)) << "write_bson failed";

      BsonInlineMembers back{};
      expect(!glz::read_bson(back, bytes));
      expect(back.engaged == 42);
      expect(std::holds_alternative<std::string>(back.choice));
      expect(std::get<std::string>(back.choice) == "picked");
   };

   "the CSV writer still round trips a struct of columns"_test = [] {
      CsvColumns v{};
      v.num1 = {1, 2};
      v.num2 = {3.5f, 4.5f};

      auto written = glz::write_csv(v);
      expect(bool(written)) << "write_csv failed";

      if (written) {
         CsvColumns back{};
         expect(!glz::read_csv<glz::rowwise>(back, *written));
         expect(back.num1 == v.num1);
         expect(back.num2 == v.num2);
      }
   };
};

int main() { return 0; }
