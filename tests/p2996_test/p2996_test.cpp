// Glaze Library
// For the license information refer to glaze.hpp
// P2996 Reflection Test - Verifies C++26 P2996 reflection works with Glaze

#include "glaze/glaze.hpp"
#include <iostream>

struct TestStruct {
   std::string name;
   int value;
   double data;
};

int main()
{
   std::cout << "GLZ_REFLECTION26 = " << GLZ_REFLECTION26 << std::endl;

   // Test member_names
   constexpr auto names = glz::member_names<TestStruct>;
   if (names.size() != 3) {
      std::cerr << "Expected 3 members, got " << names.size() << std::endl;
      return 1;
   }
   if (names[0] != "name") {
      std::cerr << "Expected names[0] == 'name', got " << names[0] << std::endl;
      return 1;
   }
   if (names[1] != "value") {
      std::cerr << "Expected names[1] == 'value', got " << names[1] << std::endl;
      return 1;
   }
   if (names[2] != "data") {
      std::cerr << "Expected names[2] == 'data', got " << names[2] << std::endl;
      return 1;
   }

   // Test JSON round-trip with struct
   TestStruct obj{"test", 42, 3.14};
   auto json = glz::write_json(obj).value_or("error");

   TestStruct obj2;
   if (glz::read_json(obj2, json)) {
      std::cerr << "Failed to parse JSON: " << json << std::endl;
      return 1;
   }
   if (obj2.name != "test" || obj2.value != 42) {
      std::cerr << "Round-trip failed" << std::endl;
      return 1;
   }

   std::cout << "P2996 test passed!" << std::endl;
   std::cout << "JSON: " << json << std::endl;
   return 0;
}
