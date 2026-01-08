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

// Test enum with automatic P2996 reflection
enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color> : glz::reflect_enum {};

// Test enum with name transformation (snake_case)
enum class HttpStatus { Ok, NotFound, InternalServerError };

template <>
struct glz::meta<HttpStatus> : glz::reflect_enum, glz::snake_case {};

// Test enum WITHOUT any glz::meta - used for reflect_enums option test
enum class Direction { North, South, East, West };

// Custom opts with reflect_enums enabled
struct reflect_enums_opts : glz::opts
{
   bool reflect_enums = true;
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

   // Test JSON round-trip
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

   // Test Color enum (basic reflect_enum)
   {
      Color c = Color::Green;
      auto enum_json = glz::write_json(c).value_or("error");
      if (enum_json != "\"Green\"") {
         std::cerr << "Expected Color::Green to serialize as \"Green\", got " << enum_json << std::endl;
         return 1;
      }

      Color c2;
      if (glz::read_json(c2, enum_json)) {
         std::cerr << "Failed to parse Color JSON: " << enum_json << std::endl;
         return 1;
      }
      if (c2 != Color::Green) {
         std::cerr << "Color round-trip failed" << std::endl;
         return 1;
      }
      std::cout << "Color enum test passed: " << enum_json << std::endl;
   }

   // Test HttpStatus enum (with snake_case transformation)
   {
      HttpStatus s = HttpStatus::InternalServerError;
      auto status_json = glz::write_json(s).value_or("error");
      if (status_json != "\"internal_server_error\"") {
         std::cerr << "Expected HttpStatus::InternalServerError to serialize as \"internal_server_error\", got " << status_json << std::endl;
         return 1;
      }

      HttpStatus s2;
      if (glz::read_json(s2, status_json)) {
         std::cerr << "Failed to parse HttpStatus JSON: " << status_json << std::endl;
         return 1;
      }
      if (s2 != HttpStatus::InternalServerError) {
         std::cerr << "HttpStatus round-trip failed" << std::endl;
         return 1;
      }
      std::cout << "HttpStatus enum test passed: " << status_json << std::endl;
   }

   // Test reflect_enums option with Direction enum (no glz::meta)
   {
      Direction d = Direction::East;

      // Write using reflect_enums option
      std::string dir_json;
      auto ec = glz::write<reflect_enums_opts{}>(d, dir_json);
      if (ec) {
         std::cerr << "Failed to write Direction with reflect_enums" << std::endl;
         return 1;
      }
      if (dir_json != "\"East\"") {
         std::cerr << "Expected Direction::East to serialize as \"East\", got " << dir_json << std::endl;
         return 1;
      }

      // Read using reflect_enums option
      Direction d2;
      ec = glz::read<reflect_enums_opts{}>(d2, dir_json);
      if (ec) {
         std::cerr << "Failed to parse Direction JSON: " << dir_json << std::endl;
         return 1;
      }
      if (d2 != Direction::East) {
         std::cerr << "Direction round-trip failed" << std::endl;
         return 1;
      }
      std::cout << "reflect_enums option test passed: " << dir_json << std::endl;
   }

   std::cout << "P2996 test passed!" << std::endl;
   std::cout << "JSON: " << json << std::endl;
   return 0;
}
