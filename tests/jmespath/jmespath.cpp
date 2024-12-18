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
   "compile-time read_jmespath"_test = [] {
      Home home{.family = {.father = {"Gilbert", "Fox", 28},
            .mother = {"Anne", "Fox", 30},
         .children = {{"Lilly", "Fox", 7}, {"Vincent", "Fox", 3}}},
         .address = "123 Maple Street"};

      std::string buffer{};
      expect(not glz::write_json(home, buffer));

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

   "run-time read_jmespath"_test = [] {
      Home home{.family = {.father = {"Gilbert", "Fox", 28},
            .mother = {"Anne", "Fox", 30},
         .children = {{"Lilly", "Fox", 7}, {"Vincent", "Fox", 3}}},
         .address = "123 Maple Street"};

      std::string buffer{};
      expect(not glz::write_json(home, buffer));

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
   
   "compile-time error_handling"_test = [] {
      Home home{.family = {.father = {"Gilbert", "Fox", 28},
                           .mother = {"Anne", "Fox", 30},
                           .children = {{"Lilly"}, {"Vincent"}}},
                .address = "123 Maple Street"};

      std::string buffer{};
      expect(not glz::write_json(home, buffer));

      std::string middle_name{};
      auto ec = glz::read_jmespath<"family.father.middle_name">(middle_name, buffer);
      //std::cout << glz::format_error(ec, buffer) << '\n';
      expect(ec) << "Expected error for non-existent field";

      // Invalid JMESPath expression
      // Note: Compile-time JMESPath expressions are validated at compile time,
      // so invalid expressions would cause a compile-time error.
      // Therefore, runtime tests should cover invalid expressions.
   };
   
   "run-time error_handling"_test = [] {
      Home home{.family = {.father = {"Gilbert", "Fox", 28},
                           .mother = {"Anne", "Fox", 30},
                           .children = {{"Lilly"}, {"Vincent"}}},
                .address = "123 Maple Street"};

      std::string buffer{};
      expect(not glz::write_json(home, buffer));

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
      std::vector<int> data{0,1,2,3,4,5,6,7,8,9};
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
      std::vector<int> data{0,1,2,3,4,5,6,7,8,9};
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
      std::vector<std::vector<int>> data{{1,2},{3,4,5},{6,7}};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));
      
      int v{};
      expect(not glz::read_jmespath<"[1][2]">(v, buffer));
      expect(v == 5);
   };
   
   "slice run-time multi-bracket"_test = [] {
      std::vector<std::vector<int>> data{{1,2},{3,4,5},{6,7}};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));
      
      int v{};
      expect(not glz::read_jmespath("[1][2]", v, buffer));
      expect(v == 5);
   };
};

int main() { return 0; }
