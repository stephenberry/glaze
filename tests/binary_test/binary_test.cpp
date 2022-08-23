// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif
#include "boost/ut.hpp"

#include <bit>
#include <map>

#include "glaze/binary/write.hpp"
#include "glaze/binary/read.hpp"

using namespace glaze;

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

template <>
struct glaze::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto value = glaze::object("i", &T::i,          //
                                               "d", &T::d,          //
                                               "hello", &T::hello,  //
                                               "arr", &T::arr       //
   );
};

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
         std::string s = "Hello World";
         std::vector<std::byte> out;
         write_binary(s, out);
         std::string s2{};
         read_binary(s2, out);
         expect(s == s2);
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
   
   "my_struct"_test = [] {
      my_struct s{};
      s.i = 5;
      s.hello = "Wow!";
      std::vector<std::byte> out;
      write_binary(s, out);
      my_struct s2{};
      read_binary(s2, out);
      expect(s.i == s2.i);
      expect(s.hello == s2.hello);
   };

   "nullable"_test = [] {
      std::vector<std::byte> out;

      std::optional<int> op_int{};
      write_binary(op_int, out);

      std::optional<int> new_op{};
      read_binary(new_op, out);

      expect(op_int == new_op);

      op_int = 10;
      out.clear();

      write_binary(op_int, out);
      read_binary(new_op, out);

      expect(op_int == new_op);

      out.clear();

      std::shared_ptr<float> sh_float = std::make_shared<float>(5.55);
      write_binary(sh_float, out);

      std::shared_ptr<float> out_flt;
      read_binary(out_flt, out);

      expect(*sh_float == *out_flt);

      out.clear();

      std::unique_ptr<double> uni_dbl = std::make_unique<double>(5.55);
      write_binary(uni_dbl, out);

      std::shared_ptr<double> out_dbl;
      read_binary(out_dbl, out);

      expect(*uni_dbl == *out_dbl);
   };

   "map"_test = [] {
      std::vector<std::byte> out;

      std::map<std::string, int> str_map{
         {"a", 1}, {"b", 10}, {"c", 100}, {"d", 1000}};

      write_binary(str_map, out);

      std::map<std::string, int> str_read;

      read_binary(str_read, out);

      for (auto& item : str_map) {
         expect(str_read[item.first] == item.second);
      }

      out.clear();

      std::map<int, double> dbl_map{
         {1, 5.55}, {3, 7.34}, {8, 44.332}, {0, 0.000}};
      write_binary(dbl_map, out);

      std::map<int, double> dbl_read{};
      read_binary(dbl_read, out);

      for (auto& item : dbl_map) {
         expect(dbl_read[item.first] == item.second);
      }
   };
}

int main()
{
   using namespace boost::ut;

   write_tests();
}
