// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif
#include "boost/ut.hpp"

#include <bit>

#include "glaze/binary/write.hpp"
#include "glaze/binary/read.hpp"

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
   
   "bool_memcpy"_test = [] {
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
   
   "bool"_test = [] {
      {
         bool b = true;
         std::vector<std::byte> out;
         write_binary(b, out);
         bool b2{};
         read_binary(b2, out);
         expect(b == b2);
      }
   };
   
   "float"_test = [] {
      {
         float f = 1.5f;
         std::vector<std::byte> out;
         write_binary(f, out);
         float f2{};
         read_binary(f2, out);
         expect(f == f2);
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
         std::array<float, 3> arr = { 1.2f, 3434.343f, 0.f };
         std::vector<std::byte> out;
         write_binary(arr, out);
         std::array<float, 3> arr2{};
         read_binary(arr2, out);
         expect(arr == arr2);
      }
   };
   
   "vector"_test = [] {
      {
         std::vector<float> v = { 1.2f, 3434.343f, 0.f };
         std::vector<std::byte> out;
         write_binary(v, out);
         std::vector<float> v2;
         read_binary(v2, out);
         expect(v == v2);
      }
   };
}

int main()
{
   using namespace boost::ut;

   write_tests();
}
