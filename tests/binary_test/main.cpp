// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif
#include "boost/ut.hpp"

#include <bit>

#include "glaze/binary/write.hpp"

using namespace glaze;

void write_tests()
{
   using namespace boost::ut;

   "round_trip"_test = [] {
      {
         std::vector<std::byte> s;
         float f{0.96875f};
         auto start = f;
         s.resize(sizeof(float));
         std::memcpy(s.data(), &f, sizeof(float));
         std::memcpy(&f, s.data(), sizeof(float));
         expect(start == f);
      }
   };
   
   "bool"_test = [] {
      {
         std::vector<std::byte> s;
         bool b = true;
         s.resize(sizeof(bool));
         std::memcpy(s.data(), &b, sizeof(bool));
         
         std::vector<std::byte> out;
         write_binary(b, out);
         const bool success = out == s;
         expect(success);
      }
   };
   
   "float"_test = [] {
      {
         std::vector<std::byte> s;
         float f = 1.5f;
         s.resize(sizeof(float));
         std::memcpy(s.data(), &f, sizeof(float));
         
         std::vector<std::byte> out;
         write_binary(f, out);
         const bool success = out == s;
         expect(success);
      }
   };
   
   "string"_test = [] {
      {
         std::vector<std::byte> in;
         std::string s = "Hello World";
         in.resize(sizeof(char) * s.size());
         std::memcpy(in.data(), s.data(), sizeof(char) * s.size());
         
         std::vector<std::byte> out;
         write_binary(s, out);
         const bool success = out == in;
         expect(success);
      }
   };
   
   "array"_test = [] {
      {
         std::array<float, 3> s = { 1.2f, 3434.343f, 0.f };
         std::vector<std::byte> out;
         write_binary(s, out);      }
   };
   
   "vector"_test = [] {
      {
         std::vector<float> s = { 1.2f, 3434.343f, 0.f };
         std::vector<std::byte> out;
         write_binary(s, out);
      }
   };
}

int main()
{
   using namespace boost::ut;

   write_tests();
}
