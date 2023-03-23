#include "boost/ut.hpp"


#include "glaze/csv/read.hpp"
#include "glaze/csv/write.hpp"

#include <deque>

using namespace boost::ut;
using namespace glz;

struct my_struct
{
   std::vector<int> num1 = {};
   std::deque<float> num2 = {};
   std::vector<bool> maybe = {};
   //std::vector<std::string> errors = {};


   std::vector<std::array<int, 3>> v3s = {};

   // enum support
};

template <>
struct glz::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto value = glz::object(
      "num1", &T::num1,
      "num2", &T::num2,
      "maybe", &T::maybe,
      "v3s", &T::v3s
   );
};


void read_tests()
{
   //"basic read"_test = []
   //{
      std::string input_col =
R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4)";

      std::string input_row =
R"(num1,11,33,55,77
num2,22,44,66,88
maybe,1,1,0,0
v3s[0],1,2,3,4
v3s[1],1,2,3,4
v3s[2],1,2,3,4)";


      my_struct obj_row{};
      my_struct obj_col{};

      read_csv(obj_row, input_row);
      read_csv<false>(obj_col, input_col);

   //};

      write_file_csv<false>(obj_col, "csv_test_colwise.csv");
      write_file_csv(obj_row, "csv_test_rowwise.csv");


      return;
}

int main()
{
   read_tests();
}
