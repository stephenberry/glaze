// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include <array>
#include <cstdint>
#include <string>

#include "glaze/eetf/read.hpp"
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

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   glz::eetf::atom a = "elang_atom_field"_atom;
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

// static_assert(glz::write_eetf_supported<my_struct>);
static_assert(glz::read_eetf_supported<my_struct>);

struct my_struct_meta
{
   my_struct_meta() : val_i{287}, val_d{3.14}, val_str{"Hello World"}, val_arr{1, 2, 3} {}

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

// static_assert(glz::write_eetf_supported<my_struct_meta>);
static_assert(glz::read_eetf_supported<my_struct_meta>);

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
      auto ec = glz::read<glz::opts{
         .format = glz::ERLANG,
         .bits = glz::options(glz::json_options_default).set(glz::option::error_on_unknown_keys, false)}>(s,
                                                                                                          term_map_001);
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
      auto ec = glz::read_term<glz::proplist>(s, term_proplist_001);
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
      auto ec = glz::read<glz::opts{
         .format = glz::ERLANG,
         .bits = glz::options(glz::json_options_default).set(glz::option::error_on_unknown_keys, false),
         .layout = glz::proplist}>(s, term_proplist_001);
      expect(not ec) << glz::format_error(ec, "can't read");
      expect(s.val_d == 3.1415926);
      expect(s.val_i == 1);
      expect(s.val_arr == decltype(s.val_arr){9, 8, 7});
      expect(s.val_str == "Hello Erlang Term");
      trace.end("read_proplist_term_meta");
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