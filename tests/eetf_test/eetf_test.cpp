// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include <array>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <variant>

#include "glaze/eetf/eetf_to_json.hpp"
#include "glaze/eetf/read.hpp"
#include "glaze/eetf/wrappers.hpp"
#include "glaze/eetf/write.hpp"
#include "glaze/trace/trace.hpp"
#include "ut/ut.hpp"

using namespace glz::eetf;

using namespace ut;

glz::trace trace{};
suite start_trace = [] { trace.begin("eetf_test", "Full test suite duration."); };

/*
T = #{a => atom_term, arr => [9,8,7], d => 3.1415926, hello => "Hello Erlang Term", i => 1}.
io:format("~p", [erlang:term_to_binary(T)]).
*/

std::array<std::uint8_t, 81> term_map_001{
   131, 116, 0,   0,   0,  5,   100, 0,   1,   97,  100, 0,   9,   97,  116, 111, 109, 95,  116, 101, 114,
   109, 100, 0,   3,   97, 114, 114, 107, 0,   3,   9,   8,   7,   100, 0,   1,   100, 70,  64,  9,   33,
   251, 77,  18,  216, 74, 100, 0,   5,   104, 101, 108, 108, 111, 107, 0,   17,  72,  101, 108, 108, 111,
   32,  69,  114, 108, 97, 110, 103, 32,  84,  101, 114, 109, 100, 0,   1,   105, 97,  1};

std::array<std::uint8_t, 92> term_proplist_001{
   131, 108, 0,   0,   0,   5,   104, 2,   100, 0,   1,  97,  100, 0,   9,   97,  116, 111, 109, 95,  116, 101, 114,
   109, 104, 2,   100, 0,   3,   97,  114, 114, 107, 0,  3,   9,   8,   7,   104, 2,   100, 0,   1,   100, 70,  64,
   9,   33,  251, 77,  18,  216, 74,  104, 2,   100, 0,  5,   104, 101, 108, 108, 111, 107, 0,   17,  72,  101, 108,
   108, 111, 32,  69,  114, 108, 97,  110, 103, 32,  84, 101, 114, 109, 104, 2,   100, 0,   1,   105, 97,  1,   106};

std::array<std::uint8_t, 16> term_atom{131, 116, 0, 0, 0, 1, 100, 0, 1, 97, 100, 0, 3, 113, 119, 101};

struct simple
{
   int a;
};

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   glz::eetf::atom a = "elang_atom_field"_atom;
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

struct my_struct_meta
{
   my_struct_meta() : val_i{287}, val_d{3.14}, val_str{"Hello World"}, val_arr{1, 2, 3} {}
   my_struct_meta(int i, double d, std::string s, std::vector<uint64_t> v)
      : val_i{i}, val_d{d}, val_str{s}, val_arr{std::move(v)}
   {}

   int val_i;
   double val_d;
   std::string val_str;
   std::vector<uint64_t> val_arr;
};

template <>
struct glz::meta<my_struct_meta>
{
   using T = my_struct_meta;
   static constexpr auto value = object("i", &T::val_i, //
                                        "d", &T::val_d, //
                                        "hello", &T::val_str, //
                                        "arr", &T::val_arr //
   );
};

struct atom_rw
{
   std::string a;
};

template <>
struct glz::meta<atom_rw>
{
   using T = atom_rw;
   static constexpr auto value = object("a", glz::atom_as_string<&T::a>);
};

static_assert(glz::write_supported<my_struct_meta, glz::EETF>);
static_assert(glz::read_supported<my_struct_meta, glz::EETF>);

suite etf_tests = [] {
   "read_map_term"_test = [] {
      trace.begin("read_map_term");
      my_struct s{};
      auto ec = glz::read_term(s, term_map_001);
      expect(not ec) << glz::format_error(ec, "can't read");
      expect(s.a == "atom_term");
      expect(s.d == 3.1415926);
      expect(s.i == 1);
      expect(s.arr == decltype(s.arr){9, 8, 7});
      expect(s.hello == "Hello Erlang Term");
      trace.end("read_map_term");
   };

   "read_map_term_meta"_test = [] {
      trace.begin("read_map_term_meta");
      my_struct_meta s{};
      auto ec = glz::read<glz::eetf::eetf_opts{.format = glz::EETF, .error_on_unknown_keys = false}>(s, term_map_001);
      expect(not ec) << glz::format_error(ec, "can't read");
      expect(s.val_d == 3.1415926);
      expect(s.val_i == 1);
      expect(s.val_arr == decltype(s.val_arr){9, 8, 7});
      expect(s.val_str == "Hello Erlang Term");
      trace.end("read_map_term_meta");
   };

   "read_proplist_term"_test = [] {
      trace.begin("read_proplist_term");
      my_struct s{};
      auto ec = glz::read_term<glz::eetf::proplist_layout>(s, term_proplist_001);
      expect(not ec) << glz::format_error(ec, "can't read");
      expect(s.a == "atom_term");
      expect(s.d == 3.1415926);
      expect(s.i == 1);
      expect(s.arr == decltype(s.arr){9, 8, 7});
      expect(s.hello == "Hello Erlang Term");
      trace.end("read_proplist_term");
   };

   "read_proplist_term_meta"_test = [] {
      trace.begin("read_proplist_term_meta");
      my_struct_meta s{};
      auto ec = glz::read<glz::eetf::eetf_opts{
         .format = glz::EETF, .layout = glz::eetf::proplist_layout, .error_on_unknown_keys = false}>(s,
                                                                                                     term_proplist_001);
      expect(not ec) << glz::format_error(ec, "can't read");
      expect(s.val_d == 3.1415926);
      expect(s.val_i == 1);
      expect(s.val_arr == decltype(s.val_arr){9, 8, 7});
      expect(s.val_str == "Hello Erlang Term");
      trace.end("read_proplist_term_meta");
   };

   "roundtrip_map_as_term (string)"_test = [] {
      trace.begin("roundtrip_map_as_term (string)");
      std::map<std::string, int> m = {{"first", 1}, {"second", 2}, {"third", 3}}, v;
      std::vector<std::uint8_t> buff;
      auto ec = glz::write_term(m, buff);

      expect(not ec) << glz::format_error(ec, "can't write map");

      ec = glz::read_term(v, buff);
      expect(not ec) << glz::format_error(ec, "can't read map");

      expect(m == v);
      trace.end("roundtrip_map_as_term (string)");
   };

   "roundtrip_map_as_term (atom)"_test = [] {
      trace.begin("roundtrip_map_as_term (atom)");
      std::map<glz::eetf::atom, int> m = {{"first"_atom, 1}, {"second"_atom, 2}, {"third"_atom, 3}}, v;
      std::vector<std::uint8_t> buff;
      auto ec = glz::write_term(m, buff);

      expect(not ec) << glz::format_error(ec, "can't write map");

      ec = glz::read_term(v, buff);
      expect(not ec) << glz::format_error(ec, "can't read map");

      expect(m == v);
      trace.end("roundtrip_map_as_term (atom)");
   };

   "write_term"_test = [] {
      trace.begin("write_term");
      my_struct sw{.i = 123, .d = 2.71827, .hello = "Hello write", .a = "qwe"_atom, .arr = {45, 67, 89}};
      std::vector<std::uint8_t> buff;
      auto ec = glz::write_term(sw, buff);
      trace.end("write_term");

      expect(not ec) << glz::format_error(ec, "can't write");

      my_struct s{};
      ec = glz::read_term(s, buff);
      expect(not ec) << glz::format_error(ec, "can't read");

      expect(s.a == "qwe");
      expect(s.d == 2.71827);
      expect(s.i == 123);
      expect(s.arr == decltype(s.arr){45, 67, 89});
      expect(s.hello == "Hello write");
   };

   "write_term_meta"_test = [] {
      trace.begin("write_term");
      my_struct_meta sw(123, 2.71827, "Hello write meta", {45, 67, 89});
      std::vector<std::uint8_t> buff;
      auto ec = glz::write_term(sw, buff);
      trace.end("write_term");

      expect(not ec) << glz::format_error(ec, "can't write");

      my_struct s{};
      ec = glz::read_term(s, buff);
      expect(not ec) << glz::format_error(ec, "can't read");

      expect(s.d == 2.71827);
      expect(s.i == 123);
      expect(s.arr == decltype(s.arr){45, 67, 89});
      expect(s.hello == "Hello write meta");
   };

   "write_proplist_term"_test = [] {
      trace.begin("write_term");
      my_struct sw{.i = 123, .d = 2.71827, .hello = "Hello write", .a = "qwe"_atom, .arr = {45, 67, 89}};
      std::vector<std::uint8_t> buff;
      auto ec = glz::write_term<glz::eetf::proplist_layout>(sw, buff);
      trace.end("write_term");

      expect(not ec) << glz::format_error(ec, "can't write");

      my_struct s{};
      ec = glz::read_term<glz::eetf::proplist_layout>(s, buff);
      expect(not ec) << glz::format_error(ec, "can't read");

      expect(s.a == "qwe");
      expect(s.d == 2.71827);
      expect(s.i == 123);
      expect(s.arr == decltype(s.arr){45, 67, 89});
      expect(s.hello == "Hello write");
   };

   "write_proplist_term_meta"_test = [] {
      trace.begin("write_term");
      my_struct_meta sw(123, 2.71827, "Hello write meta", {45, 67, 89});
      std::vector<std::uint8_t> buff;
      auto ec = glz::write_term<glz::eetf::proplist_layout>(sw, buff);
      trace.end("write_term");

      expect(not ec) << glz::format_error(ec, "can't write");

      my_struct s{};
      ec = glz::read_term<glz::eetf::proplist_layout>(s, buff);
      expect(not ec) << glz::format_error(ec, "can't read");

      expect(s.d == 2.71827);
      expect(s.i == 123);
      expect(s.arr == decltype(s.arr){45, 67, 89});
      expect(s.hello == "Hello write meta");
   };

   "read_write_string_as_atom"_test = [] {
      trace.begin("read_write_string_as_atom");
      atom_rw s{}, r{};
      auto ec = glz::read_term(s, term_atom);
      expect(not ec) << glz::format_error(ec, "can't read");
      expect(s.a == "qwe");

      std::vector<std::uint8_t> out{};
      expect(not glz::write_term(s, out)) << "can't write";
      expect(not glz::read_term(r, out)) << "can't read again";
      expect(r.a == "qwe");

      trace.end("read_write_string_as_atom");
   };

   "roundtrip_map_as_proplist"_test = [] {
      trace.begin("roundtrip_map_as_proplist");
      std::map<std::string, int> m = {{"first", 1}, {"second", 2}, {"third", 3}}, v;
      std::vector<std::uint8_t> buff;
      auto ec = glz::write_term<glz::eetf::proplist_layout>(m, buff);

      expect(not ec) << glz::format_error(ec, "can't write map");

      ec = glz::read_term<glz::eetf::proplist_layout>(v, buff);
      expect(not ec) << glz::format_error(ec, "can't read map");

      expect(m == v);
      trace.end("roundtrip_map_as_proplist");
   };

   "write_variant"_test = [] {
      using Doubles = std::vector<double>;
      using V = std::variant<int, Doubles>;
      V v = Doubles{1.2, 3.4, 5.6};
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      // can't parse into std::variant (see comment in eetf/read.cpp)
      Doubles res;
      expect(not glz::read_term(res, buffer));
      expect(res == std::get<Doubles>(v));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"([1.2,3.4,5.6])") << json;
   };
};

suite eetf_to_json_tests = [] {
   "eetf_to_json true"_test = [] {
      bool b = true;
      std::string buffer{};
      expect(not glz::write_term(b, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"(true)");
   };

   "eetf_to_json false"_test = [] {
      bool b = false;
      std::string buffer{};
      expect(not glz::write_term(b, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"(false)");
   };

   "eetf_to_json double"_test = [] {
      double v = 3.14;
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"(3.14)") << json;
      double res{};
      expect(!glz::read_json(res, json));
      expect(v == res);
   };

   "eetf_to_json string"_test = [] {
      std::string v = "Hello World";
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"("Hello World")") << json;
   };

   "eetf_to_json atom"_test = [] {
      auto v = "hello"_atom;
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"("hello")") << json;
   };

   "eetf_to_json std::map"_test = [] {
      std::map<std::string, int> v = {{"first", 1}, {"second", 2}, {"third", 3}};
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"({"first":1,"second":2,"third":3})") << json;

      expect(!glz::eetf_to_json<glz::opts{.prettify = true}>(buffer, json));
      expect(json ==
             R"({
   "first": 1,
   "second": 2,
   "third": 3
})") << json;
   };

   "eetf_to_json std::vector<int32_t>"_test = [] {
      std::vector<int32_t> v = {1, 2, 3, 4, 5};
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"([1,2,3,4,5])") << json;
   };

   "eetf_to_json empty std::vector"_test = [] {
      std::vector<int32_t> v{};
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"([])") << json;
   };

   "eetf_to_json std::vector<double>"_test = [] {
      std::vector<double> v = {1.0, 2.0, 3.0, 4.0, 5.0};
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"([1,2,3,4,5])") << json;
   };

   "eetf_to_json std::vector<std::string>"_test = [] {
      std::vector<std::string> v = {"one", "two", "three"};
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"(["one","two","three"])") << json;
   };

   "eetf_to_json std::tuple<int, std::string>"_test = [] {
      std::tuple<int, std::string> v = {99, "spiders"};
      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == R"([99,"spiders"])") << json;
   };

   "eetf_to_json small big integer"_test = [] {
      const std::array<std::uint8_t, 5> buffer{uint8_t(glz::eetf_magic_version), uint8_t(ERL_SMALL_BIG_EXT), 1, 0, 42};

      std::string json{};
      expect(!glz::eetf_to_json(buffer, json));
      expect(json == "42") << json;
   };

   "eetf_to_json truncated small big integer"_test = [] {
      auto expect_unexpected_end = [](const auto& buffer) {
         std::string json{};
         const auto ec = glz::eetf_to_json(buffer, json);
         expect(ec.ec == glz::error_code::unexpected_end);
      };

      const std::array<std::uint8_t, 2> missing_size{uint8_t(glz::eetf_magic_version), uint8_t(ERL_SMALL_BIG_EXT)};
      const std::array<std::uint8_t, 3> missing_sign{uint8_t(glz::eetf_magic_version), uint8_t(ERL_SMALL_BIG_EXT), 1};
      const std::array<std::uint8_t, 4> missing_digits{uint8_t(glz::eetf_magic_version), uint8_t(ERL_SMALL_BIG_EXT), 8,
                                                       0};
      const std::array<std::uint8_t, 6> partial_digits{
         uint8_t(glz::eetf_magic_version), uint8_t(ERL_SMALL_BIG_EXT), 3, 0, 1, 2};

      expect_unexpected_end(missing_size);
      expect_unexpected_end(missing_sign);
      expect_unexpected_end(missing_digits);
      expect_unexpected_end(partial_digits);
   };

   "eetf_to_json empty buffer"_test = [] {
      // No version byte at all: the entry guard rejects before decode_version reads the tag.
      const std::string buffer{};
      std::string json{};
      const auto ec = glz::eetf_to_json(buffer, json);
      expect(ec.ec == glz::error_code::no_read_input);
   };

   "eetf_to_json truncated map"_test = [] {
      auto expect_unexpected_end = [](const auto& buffer) {
         std::string json{};
         const auto ec = glz::eetf_to_json(buffer, json);
         expect(ec.ec == glz::error_code::unexpected_end);
      };

      // ERL_MAP_EXT declares arity 1 but the buffer ends before any key/value entry.
      const std::array<std::uint8_t, 6> missing_entries{
         uint8_t(glz::eetf_magic_version), uint8_t(ERL_MAP_EXT), 0, 0, 0, 1};
      // ERL_MAP_EXT arity 1 with a key tag (ERL_ATOM_EXT) as the final byte: the tag must be
      // classified without ei_get_type reading its 2-byte length header past the end.
      const std::array<std::uint8_t, 7> truncated_key{
         uint8_t(glz::eetf_magic_version), uint8_t(ERL_MAP_EXT), 0, 0, 0, 1, uint8_t(ERL_ATOM_EXT)};

      expect_unexpected_end(missing_entries);
      expect_unexpected_end(truncated_key);
   };

   "eetf_to_json list missing tail"_test = [] {
      // ERL_LIST_EXT, one element (small integer 5), but the ERL_NIL_EXT tail is missing.
      const std::array<std::uint8_t, 8> missing_tail{
         uint8_t(glz::eetf_magic_version), uint8_t(ERL_LIST_EXT), 0, 0, 0, 1, uint8_t(ERL_SMALL_INTEGER_EXT), 5};
      std::string json{};
      const auto ec = glz::eetf_to_json(missing_tail, json);
      expect(ec.ec == glz::error_code::unexpected_end);

      // The same list with the NIL tail present parses cleanly: the tail guard must not reject it.
      const std::array<std::uint8_t, 9> with_tail{uint8_t(glz::eetf_magic_version),
                                                  uint8_t(ERL_LIST_EXT),
                                                  0,
                                                  0,
                                                  0,
                                                  1,
                                                  uint8_t(ERL_SMALL_INTEGER_EXT),
                                                  5,
                                                  uint8_t(ERL_NIL_EXT)};
      json.clear();
      expect(!glz::eetf_to_json(with_tail, json));
      expect(json == "[5]") << json;
   };

   "eetf_to_json list improper tail"_test = [] {
      // ERL_LIST_EXT with one element and a non-NIL tail tag (ERL_MAP_EXT) as the final byte. The
      // tail must be classified from its single tag byte without ei_get_type reading the 4-byte
      // length header past the end of the buffer.
      const std::array<std::uint8_t, 9> buffer{uint8_t(glz::eetf_magic_version),
                                               uint8_t(ERL_LIST_EXT),
                                               0,
                                               0,
                                               0,
                                               1,
                                               uint8_t(ERL_SMALL_INTEGER_EXT),
                                               5,
                                               uint8_t(ERL_MAP_EXT)};
      std::string json{};
      const auto ec = glz::eetf_to_json(buffer, json);
      expect(ec.ec == glz::error_code::array_element_not_found);
   };

   "eetf_to_json truncated scalar"_test = [] {
      auto expect_unexpected_end = [](const auto& buffer) {
         std::string json{};
         const auto ec = glz::eetf_to_json(buffer, json);
         expect(ec.ec == glz::error_code::unexpected_end);
      };

      // Numeric tags whose fixed payload is cut off after the tag: ei_decode_long/double must not
      // read it off the raw pointer before the bounds check.
      const std::array<std::uint8_t, 2> small_int{uint8_t(glz::eetf_magic_version), uint8_t(ERL_SMALL_INTEGER_EXT)};
      const std::array<std::uint8_t, 2> integer{uint8_t(glz::eetf_magic_version), uint8_t(ERL_INTEGER_EXT)};
      const std::array<std::uint8_t, 4> integer_partial{uint8_t(glz::eetf_magic_version), uint8_t(ERL_INTEGER_EXT), 0,
                                                        0};
      const std::array<std::uint8_t, 2> new_float{uint8_t(glz::eetf_magic_version), uint8_t(NEW_FLOAT_EXT)};
      const std::array<std::uint8_t, 2> old_float{uint8_t(glz::eetf_magic_version), uint8_t(ERL_FLOAT_EXT)};

      expect_unexpected_end(small_int);
      expect_unexpected_end(integer);
      expect_unexpected_end(integer_partial);
      expect_unexpected_end(new_float);
      expect_unexpected_end(old_float);
   };

   "eetf_to_json truncated container header"_test = [] {
      auto expect_unexpected_end = [](const auto& buffer) {
         std::string json{};
         const auto ec = glz::eetf_to_json(buffer, json);
         expect(ec.ec == glz::error_code::unexpected_end);
      };

      // Container tags whose 1- or 4-byte arity is cut off after the tag: decode_*_header must not
      // read it off the raw pointer before the bounds check.
      const std::array<std::uint8_t, 2> list{uint8_t(glz::eetf_magic_version), uint8_t(ERL_LIST_EXT)};
      const std::array<std::uint8_t, 4> list_partial{uint8_t(glz::eetf_magic_version), uint8_t(ERL_LIST_EXT), 0, 0};
      const std::array<std::uint8_t, 2> map{uint8_t(glz::eetf_magic_version), uint8_t(ERL_MAP_EXT)};
      const std::array<std::uint8_t, 2> small_tuple{uint8_t(glz::eetf_magic_version), uint8_t(ERL_SMALL_TUPLE_EXT)};
      const std::array<std::uint8_t, 2> large_tuple{uint8_t(glz::eetf_magic_version), uint8_t(ERL_LARGE_TUPLE_EXT)};

      expect_unexpected_end(list);
      expect_unexpected_end(list_partial);
      expect_unexpected_end(map);
      expect_unexpected_end(small_tuple);
      expect_unexpected_end(large_tuple);
   };

   "eetf_to_json oversized list arity terminates"_test = [] {
      // List header declares 2^32-1 elements but only one is present. The sequence loop must stop
      // on the first exhausted read rather than spinning through the declared arity.
      const std::array<std::uint8_t, 8> buffer{uint8_t(glz::eetf_magic_version),
                                               uint8_t(ERL_LIST_EXT),
                                               0xFF,
                                               0xFF,
                                               0xFF,
                                               0xFF,
                                               uint8_t(ERL_SMALL_INTEGER_EXT),
                                               1};
      std::string json{};
      const auto ec = glz::eetf_to_json(buffer, json);
      expect(ec.ec == glz::error_code::unexpected_end);
   };

   "eetf_to_json no header"_test = [] {
      constexpr int items = 3;
      std::vector<simple> v;
      for (int idx = 0; idx < items; idx++) {
         v.push_back({idx});
      }

      std::string buffer{};
      expect(not glz::write_term(v, buffer));

      // parse list header manually and iterate through items
      int index{0};
      int version;
      int res = ei_decode_version(buffer.data(), &index, &version);
      expect(res == 0);
      expect(version == glz::eetf_magic_version);

      int arity;
      res = ei_decode_list_header(buffer.data(), &index, &arity);
      expect(res == 0);
      expect(arity == items);

      for (int idx = 0; idx < arity; idx++) {
         std::string json;
         int curr = index;
         res = ei_skip_term(buffer.data(), &index);
         expect(res == 0);

         expect(!glz::eetf_to_json<glz::no_header_on<glz::eetf::eetf_opts{}>()>(
            std::string_view{buffer.data() + curr, static_cast<size_t>(index - curr)}, json));
         expect(json == std::format(R"({{"a":{}}})", idx));
      }
   };
};

int main()
{
   trace.end("eetf_test");
   const auto ec = glz::write_file_json(trace, "eetf_test.trace.json", std::string{});
   if (ec) {
      std::cerr << "trace output failed\n";
   }

   return 0;
}
