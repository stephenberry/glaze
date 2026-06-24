// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json/jmespath.hpp"

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct Person
{
   std::string first_name{};
   std::string last_name{};
   uint16_t age{};
};

struct Family
{
   Person father{};
   Person mother{};
   std::vector<Person> children{};
};

struct Home
{
   Family family{};
   std::string address{};
};

suite jmespath_read_tests = [] {
   Home home{.family = {.father = {"Gilbert", "Fox", 28},
                        .mother = {"Anne", "Fox", 30},
                        .children = {{"Lilly", "Fox", 7}, {"Vincent", "Fox", 3}}},
             .address = "123 Maple Street"};

   std::string buffer{};
   expect(not glz::write_json(home, buffer));

   "compile-time read_jmespath"_test = [&] {
      std::string first_name{};
      auto ec = glz::read_jmespath<"family.father.first_name">(first_name, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(first_name == "Gilbert");

      std::string mother_last_name{};
      ec = glz::read_jmespath<"family.mother.last_name">(mother_last_name, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(mother_last_name == "Fox");

      uint16_t father_age{};
      ec = glz::read_jmespath<"family.father.age">(father_age, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(father_age == 28);

      std::string address{};
      ec = glz::read_jmespath<"address">(address, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(address == "123 Maple Street");

      Person child{};
      expect(not glz::read_jmespath<"family.children[0]">(child, buffer));
      expect(child.first_name == "Lilly");
      expect(not glz::read_jmespath<"family.children[1]">(child, buffer));
      expect(child.first_name == "Vincent");

      Person non_existent_child{};
      ec = glz::read_jmespath<"family.children[3]">(non_existent_child, buffer);
      expect(ec) << "Expected error for out-of-bounds index";
   };

   "run-time read_jmespath"_test = [&] {
      std::string first_name{};
      auto ec = glz::read_jmespath("family.father.first_name", first_name, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(first_name == "Gilbert");

      std::string mother_last_name{};
      ec = glz::read_jmespath("family.mother.last_name", mother_last_name, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(mother_last_name == "Fox");

      uint16_t father_age{};
      ec = glz::read_jmespath("family.father.age", father_age, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(father_age == 28);

      std::string address{};
      ec = glz::read_jmespath("address", address, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(address == "123 Maple Street");

      Person child{};
      expect(not glz::read_jmespath("family.children[0]", child, buffer));
      expect(child.first_name == "Lilly");
      expect(not glz::read_jmespath("family.children[1]", child, buffer));
      expect(child.first_name == "Vincent");

      Person non_existent_child{};
      ec = glz::read_jmespath("family.children[3]", non_existent_child, buffer);
      expect(ec) << "Expected error for out-of-bounds index";
   };

   "pre-compiled run-time"_test = [&] {
      Person child{};
      // A runtime expression can be pre-computed and saved for more efficient lookups
      glz::jmespath_expression expression{"family.children[0]"};
      expect(not glz::read_jmespath(expression, child, buffer));
      expect(child.first_name == "Lilly");
   };

   "compile-time error_handling"_test = [&] {
      std::string middle_name{};
      auto ec = glz::read_jmespath<"family.father.middle_name">(middle_name, buffer);
      // std::cout << glz::format_error(ec, buffer) << '\n';
      expect(ec) << "Expected error for non-existent field";

      // Invalid JMESPath expression
      // Note: Compile-time JMESPath expressions are validated at compile time,
      // so invalid expressions would cause a compile-time error.
      // Therefore, runtime tests should cover invalid expressions.
   };

   "run-time error_handling"_test = [&] {
      // Access non-existent field
      std::string middle_name{};
      auto ec = glz::read_jmespath("family.father.middle_name", middle_name, buffer);
      expect(ec) << "Expected error for non-existent field";

      // Invalid JMESPath expression
      std::string invalid_query_result{};
      ec = glz::read_jmespath("family..father", invalid_query_result, buffer); // Invalid due to double dot
      expect(ec) << "Expected error for invalid JMESPath expression";
   };
};

suite jmespath_slice_tests = [] {
   "slice compile-time"_test = [] {
      std::vector<int> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      std::vector<int> slice{};
      expect(not glz::read_jmespath<"[0:5]">(slice, buffer));
      expect(slice.size() == 5);
      expect(slice[0] == 0);
      expect(slice[1] == 1);
      expect(slice[2] == 2);
      expect(slice[3] == 3);
      expect(slice[4] == 4);
   };

   "slice run-time"_test = [] {
      std::vector<int> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      std::vector<int> slice{};
      expect(not glz::read_jmespath("[0:5]", slice, buffer));
      expect(slice.size() == 5);
      expect(slice[0] == 0);
      expect(slice[1] == 1);
      expect(slice[2] == 2);
      expect(slice[3] == 3);
      expect(slice[4] == 4);
   };

   "slice negative step start past end"_test = [] {
      // [start:end:-1] with start >= size clamped the start index to size and read
      // value[size] (one past the end) on the first move. The reversed read also
      // aliased the in-place compaction, so even an in-bounds start returned garbage.
      std::vector<int> data{1, 2, 3, 4, 5};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      std::vector<int> slice{};
      glz::jmespath_expression expr{"[10:0:-1]"};
      expect(not glz::read_jmespath(expr, slice, buffer));
      expect(slice == (std::vector<int>{5, 4, 3, 2}));

      std::vector<int> slice_ct{};
      expect(not glz::read_jmespath<"[4:0:-1]">(slice_ct, buffer));
      expect(slice_ct == (std::vector<int>{5, 4, 3, 2}));

      std::vector<int> slice_step{};
      expect(not glz::read_jmespath<"[5:0:-2]">(slice_step, buffer));
      expect(slice_step == (std::vector<int>{5, 3}));

      // Start past the end of a shorter array (the heap-buffer-overflow case).
      std::vector<int> small{1, 2, 3};
      std::string small_buf{};
      expect(not glz::write_json(small, small_buf));
      std::vector<int> slice_small{};
      glz::jmespath_expression expr_small{"[5:0:-1]"};
      expect(not glz::read_jmespath(expr_small, slice_small, small_buf));
      expect(slice_small == (std::vector<int>{3, 2}));
   };

   "slice compile-time multi-bracket"_test = [] {
      std::vector<std::vector<int>> data{{1, 2}, {3, 4, 5}, {6, 7}};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      int v{};
      expect(not glz::read_jmespath<"[1][2]">(v, buffer));
      expect(v == 5);
   };

   "slice run-time multi-bracket"_test = [] {
      std::vector<std::vector<int>> data{{1, 2}, {3, 4, 5}, {6, 7}};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      int v{};
      expect(not glz::read_jmespath("[1][2]", v, buffer));
      expect(v == 5);
   };
};

suite jmespath_negative_slice_tests = [] {
   "slice omitted-end negative step reverses"_test = [] {
      // With an omitted end and a negative step the slice descends to and includes index 0,
      // so "[::-1]" reverses the whole array (previously it returned empty).
      std::vector<int> data{1, 2, 3, 4, 5};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      std::vector<int> rev{};
      expect(not glz::read_jmespath(glz::jmespath_expression{"[::-1]"}, rev, buffer));
      expect(rev == (std::vector<int>{5, 4, 3, 2, 1}));

      std::vector<int> rev_ct{};
      expect(not glz::read_jmespath<"[::-1]">(rev_ct, buffer));
      expect(rev_ct == (std::vector<int>{5, 4, 3, 2, 1}));

      std::vector<int> from2{};
      expect(not glz::read_jmespath<"[2::-1]">(from2, buffer));
      expect(from2 == (std::vector<int>{3, 2, 1}));

      std::vector<int> step2{};
      expect(not glz::read_jmespath<"[4::-2]">(step2, buffer));
      expect(step2 == (std::vector<int>{5, 3, 1}));
   };

   "slice negative end wraps to include index 0"_test = [] {
      // A negative end that resolves below 0 means "include index 0"; it must not be clamped
      // up to 0, which would drop the first element on a reverse slice.
      std::vector<int> data{1, 2, 3, 4, 5};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      std::vector<int> all{};
      expect(not glz::read_jmespath<"[4:-6:-1]">(all, buffer));
      expect(all == (std::vector<int>{5, 4, 3, 2, 1}));

      std::vector<int> neg_start{};
      expect(not glz::read_jmespath<"[-1::-1]">(neg_start, buffer));
      expect(neg_start == (std::vector<int>{5, 4, 3, 2, 1}));
   };

   "slice step of zero is rejected"_test = [] {
      // A step of 0 is invalid; previously it spun in a non-terminating loop. It must error.
      std::vector<int> data{1, 2, 3, 4, 5};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      std::vector<int> out{};
      expect(glz::read_jmespath(glz::jmespath_expression{"[5:0:0]"}, out, buffer));
      expect(glz::read_jmespath(glz::jmespath_expression{"[3:1:0]"}, out, buffer));
      expect(glz::read_jmespath(glz::jmespath_expression{"[::0]"}, out, buffer));
   };

   "slice out-of-range literal is rejected"_test = [] {
      // A bound/step/index literal that overflows int32 is malformed; it must error rather than
      // silently fall back to default bounds (which previously returned the whole array).
      std::vector<int> data{1, 2, 3, 4, 5};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      std::vector<int> out{};
      expect(glz::read_jmespath(glz::jmespath_expression{"[::9999999999]"}, out, buffer));
      expect(glz::read_jmespath(glz::jmespath_expression{"[9999999999:0:-1]"}, out, buffer));
      expect(glz::read_jmespath(glz::jmespath_expression{"[0:9999999999]"}, out, buffer));

      int v{};
      expect(glz::read_jmespath(glz::jmespath_expression{"[9999999999]"}, v, buffer));
   };
};

suite jmespath_negative_index_tests = [] {
   "negative single index"_test = [] {
      // [-1] is the last element, [-n] counts back from the end.
      std::vector<int> data{10, 20, 30, 40, 50};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      int v{};
      expect(not glz::read_jmespath<"[-1]">(v, buffer));
      expect(v == 50);
      expect(not glz::read_jmespath<"[-2]">(v, buffer));
      expect(v == 40);
      expect(not glz::read_jmespath<"[-5]">(v, buffer));
      expect(v == 10);

      expect(not glz::read_jmespath(glz::jmespath_expression{"[-1]"}, v, buffer));
      expect(v == 50);

      // Out of range still errors rather than wrapping further.
      int oob{};
      expect(glz::read_jmespath<"[-6]">(oob, buffer));
   };

   "negative index on empty array errors"_test = [] {
      std::vector<int> data{};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));
      int v{};
      expect(glz::read_jmespath<"[-1]">(v, buffer));
   };

   "negative index after key"_test = [] {
      std::string buffer = R"({"v":[10,20,30]})";
      int v{};
      expect(not glz::read_jmespath<"v[-1]">(v, buffer));
      expect(v == 30);
      expect(not glz::read_jmespath(glz::jmespath_expression{"v[-3]"}, v, buffer));
      expect(v == 10);
      expect(glz::read_jmespath<"v[-4]">(v, buffer));
   };

   "negative index nested"_test = [] {
      std::vector<std::vector<int>> data{{1, 2}, {3, 4, 5}, {6, 7}};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));

      int v{};
      expect(not glz::read_jmespath<"[-1][0]">(v, buffer));
      expect(v == 6);
      expect(not glz::read_jmespath<"[0][-1]">(v, buffer));
      expect(v == 2);
      expect(not glz::read_jmespath<"[-2][-1]">(v, buffer));
      expect(v == 5);
   };
};

// A test case for a GCC14 warning
struct gcc_maybe_uninitialized_t
{
   int acc{};
   int abbb{};
   int cqqq{};
};

suite gcc_maybe_uninitialized_tests = [] {
   "gcc_maybe_uninitialized"_test = [] {
      using namespace std::string_view_literals;

      gcc_maybe_uninitialized_t log{};

      constexpr glz::opts opts{.null_terminated = 0, .error_on_unknown_keys = 0};
      auto ec = glz::read_jmespath<"test", opts>(log, R"({"test":{"acc":1}})"sv);
      expect(not ec) << glz::format_error(ec, R"({"test":{"acc":1}})"sv);
      expect(log.acc == 1);
   };
};

suite fuzz_findings_tests = [] {
   "out_of_bounds_read"_test = [] {
      Person child{};
      std::vector<char> path(1, '\xff');
      std::vector<char> buffer{0x7b, 0x22, 0x22, 0x22, 0x22};
      static constexpr glz::opts options{.null_terminated = false};
      [[maybe_unused]] auto result = glz::read_jmespath<options>(std::string_view(path.data(), path.size()), child,
                                                                 std::string_view(buffer.data(), buffer.size()));
   };

   "unterminated_object_member"_test = [] {
      Person child{};
      const std::vector<char> path{'\x00', '\x43', '\x7c', '\x94', '\x7c', '\x00', '\x2b', '\x7f'};
      const std::vector<char> buffer{char(0x7b), char(0x22), char(0x00), char(0x22),
                                     char(0x22), char(0x22), char(0x2c)};
      static constexpr glz::opts options{.null_terminated = false};
      auto result = glz::read_jmespath<options>(std::string_view(path.data(), path.size()), child,
                                                std::string_view(buffer.data(), buffer.size()));
      expect(result == glz::error_code::unexpected_end);
   };
};

suite tuple_slice_tests = [] {
   "mixed_type_array_tuple_deserialization_correct_slice"_test = [] {
      std::string buffer = R"([1,"a","b",{"c":1}])";
      std::tuple<int, std::string> target;

      // Using [0:2] to get [1, "a"] which matches tuple<int, string>
      auto ec = glz::read_jmespath<"[0:2]">(target, buffer);
      expect(not ec) << "Error code: " << int(ec) << " " << glz::format_error(ec, buffer);
      expect(std::get<0>(target) == 1);
      expect(std::get<1>(target) == "a");
   };

   "mixed_type_array_tuple_deserialization_user_slice"_test = [] {
      std::string buffer = R"([1,"a","b",{"c":1}])";
      std::tuple<int, std::string> target;

      // User used [0:1], which produces [1].
      // This results in partial fill of the tuple.
      auto ec = glz::read_jmespath<"[0:1]">(target, buffer);
      expect(not ec) << "Error code: " << int(ec) << " " << glz::format_error(ec, buffer);
      expect(std::get<0>(target) == 1);
      expect(std::get<1>(target) == ""); // Default constructed string
   };

   "mixed_type_array_glz_tuple_deserialization"_test = [] {
      std::string buffer = R"([1,"a","b",{"c":1}])";
      glz::tuple<int, std::string> target;

      static_assert(glz::tuple_t<decltype(target)>, "glz::tuple should satisfy tuple_t");
      static_assert(glz::is_std_tuple<std::tuple<int, std::string>>, "std::tuple should satisfy is_std_tuple");

      auto ec = glz::read_jmespath<"[0:2]">(target, buffer);
      expect(not ec) << "Error code: " << int(ec) << " " << glz::format_error(ec, buffer);

      expect(glz::get<0>(target) == 1);
      expect(glz::get<1>(target) == "a");
   };
};

// Read every truncation prefix of `complete`, each over its own exact-size heap buffer (so ASAN
// flags any read past the end), with null_terminated = false. Every proper prefix of a complete
// array is malformed, so each must error rather than over-read.
template <class T, glz::string_literal Path>
void fuzz_slice_prefixes(std::string_view complete)
{
   static constexpr glz::opts options{.null_terminated = false};
   for (size_t n = 1; n < complete.size(); ++n) {
      std::vector<char> buf{complete.begin(), complete.begin() + n};
      T value{};
      const auto ec = glz::read_jmespath<Path, options>(value, std::string_view{buf.data(), buf.data() + n});
      expect(bool(ec)) << "expected truncation error at prefix length " << n;
   }
}

// Bounds hardening for the JMESPath array-slice handlers (handle_slice). On a non-null-terminated
// buffer there is no trailing '\0' sentinel, so the structural ']' / ',' scans must respect end
// explicitly. Built under ASAN in CI, these cases catch reads past an exact-size buffer.
suite non_null_terminated_slice_bounds = [] {
   static constexpr glz::opts options{.null_terminated = false};

   "nnt vector slice (partial-read) stays in bounds"_test = [] {
      fuzz_slice_prefixes<std::vector<int>, "[0:2]">("[10,20,30,40,50]");
      fuzz_slice_prefixes<std::vector<int>, "[1:3]">("[ 10 , 20 , 30 , 40 ]");

      std::vector<char> buf{};
      const std::string_view complete = "[10,20,30,40,50]";
      buf.assign(complete.begin(), complete.end());
      std::vector<int> slice{};
      const auto ec =
         glz::read_jmespath<"[1:3]", options>(slice, std::string_view{buf.data(), buf.data() + buf.size()});
      expect(not ec);
      expect(slice == (std::vector<int>{20, 30}));
   };

   "nnt vector slice (read-all fallback) stays in bounds"_test = [] {
      // A negative step routes through the read-entire-array fallback.
      fuzz_slice_prefixes<std::vector<int>, "[::-1]">("[1,2,3,4,5]");
   };

   "nnt tuple slice stays in bounds"_test = [] {
      fuzz_slice_prefixes<std::tuple<int, int>, "[0:2]">("[1,2,3,4,5]");
      fuzz_slice_prefixes<std::tuple<int, int, int>, "[0:3]">("[1,2,3,4,5]");

      std::vector<char> buf{};
      const std::string_view complete = "[7,8,9]";
      buf.assign(complete.begin(), complete.end());
      std::tuple<int, int> target{};
      const auto ec =
         glz::read_jmespath<"[0:2]", options>(target, std::string_view{buf.data(), buf.data() + buf.size()});
      expect(not ec);
      expect(std::get<0>(target) == 7);
      expect(std::get<1>(target) == 8);
   };

   "nnt single index stays in bounds"_test = [] {
      // A negative index scans the whole array to resolve its length, and an out-of-range
      // positive index scans past every element, so both must respect end on a
      // non-null-terminated buffer rather than reading past it. (An in-range positive index
      // short-circuits as soon as its element is complete, so it is not a truncation case.)
      fuzz_slice_prefixes<int, "[-1]">("[10,20,30,40,50]");
      fuzz_slice_prefixes<int, "[10]">("[10,20,30,40,50]");

      std::vector<char> buf{};
      const std::string_view complete = "[10,20,30]";
      buf.assign(complete.begin(), complete.end());
      int value{};
      const auto ec = glz::read_jmespath<"[-1]", options>(value, std::string_view{buf.data(), buf.data() + buf.size()});
      expect(not ec);
      expect(value == 30);
   };
};

int main() { return 0; }
