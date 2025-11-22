// Glaze Library
// For the license information refer to glaze.hpp

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
      const std::vector<char> path{char(0x00), char(0x43), char(0x7c), char(0x94),
                                   char(0x7c), char(0x00), char(0x2b), char(0x7f)};
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

int main() { return 0; }
