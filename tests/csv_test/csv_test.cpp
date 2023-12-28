#include <deque>
#include <map>
#include <unordered_map>

#include "boost/ut.hpp"
#include "glaze/csv/read.hpp"
#include "glaze/csv/write.hpp"
#include "glaze/record/recorder.hpp"

using namespace boost::ut;

struct my_struct
{
   std::vector<int> num1{};
   std::deque<float> num2{};
   std::vector<bool> maybe{};
   std::vector<std::array<int, 3>> v3s{};
};

template <>
struct glz::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto value = glz::object("num1", &T::num1, "num2", &T::num2, "maybe", &T::maybe, "v3s", &T::v3s);
};

struct string_elements
{
   std::vector<int> id{};
   std::vector<std::string> udl{};
};

template <>
struct glz::meta<string_elements>
{
   using T = string_elements;
   static constexpr auto value = glz::object("id", &T::id, &T::udl);
};

suite csv_tests = [] {
   "read/write column wise"_test = [] {
      std::string input_col =
         R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4)";

      my_struct obj{};

      expect(!glz::read_csv<glz::colwise>(obj, input_col));

      expect(obj.num1[0] == 11);
      expect(obj.num2[2] == 66);
      expect(obj.maybe[3] == false);
      expect(obj.v3s[0] == std::array{1, 1, 1});
      expect(obj.v3s[1] == std::array{2, 2, 2});
      expect(obj.v3s[2] == std::array{3, 3, 3});
      expect(obj.v3s[3] == std::array{4, 4, 4});

      std::string out{};

      glz::write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(obj, out);
      expect(out ==
             R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4
)");

      expect(!glz::write_file_csv<glz::colwise>(obj, "csv_test_colwise.csv", std::string{}));
   };

   "column wise string arguments"_test = [] {
      std::string input_col =
         R"(id,udl
1,BRN
2,STR
3,ASD
4,USN)";

      string_elements obj{};

      expect(!glz::read_csv<glz::colwise>(obj, input_col));

      expect(obj.id[0] == 1);
      expect(obj.id[1] == 2);
      expect(obj.id[2] == 3);
      expect(obj.id[3] == 4);
      expect(obj.udl[0] == "BRN");
      expect(obj.udl[1] == "STR");
      expect(obj.udl[2] == "ASD");
      expect(obj.udl[3] == "USN");

      std::string out{};

      glz::write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(obj, out);
      expect(out ==
             R"(id,udl
1,BRN
2,STR
3,ASD
4,USN
)");

      expect(!glz::write_file_csv<glz::colwise>(obj, "csv_test_colwise.csv", std::string{}));
   };

   "read/write row wise"_test = [] {
      std::string input_row =
         R"(num1,11,33,55,77
num2,22,44,66,88
maybe,1,1,0,0
v3s[0],1,2,3,4
v3s[1],1,2,3,4
v3s[2],1,2,3,4)";

      my_struct obj{};
      expect(!glz::read_csv(obj, input_row));

      expect(obj.num1[0] == 11);
      expect(obj.num2[2] == 66);
      expect(obj.maybe[3] == false);
      expect(obj.v3s[0][2] == 1);

      std::string out{};

      glz::write<glz::opts{.format = glz::csv}>(obj, out);
      expect(out ==
             R"(num1,11,33,55,77
num2,22,44,66,88
maybe,1,1,0,0
v3s[0],1,2,3,4
v3s[1],1,2,3,4
v3s[2],1,2,3,4)");

      expect(!glz::write_file_csv(obj, "csv_test_rowwise.csv", std::string{}));
   };

   "std::map row wise"_test = [] {
      std::map<std::string, std::vector<uint64_t>> m;
      auto& x = m["x"];
      auto& y = m["y"];

      for (size_t i = 0; i < 10; ++i) {
         x.emplace_back(i);
         y.emplace_back(i + 1);
      }

      std::string out{};
      glz::write<glz::opts{.format = glz::csv}>(m, out);
      expect(out == R"(x,0,1,2,3,4,5,6,7,8,9
y,1,2,3,4,5,6,7,8,9,10
)");

      out.clear();
      glz::write<glz::opts{.format = glz::csv}>(m, out);

      m.clear();
      expect(!glz::read<glz::opts{.format = glz::csv}>(m, out));

      expect(m["x"] == std::vector<uint64_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
      expect(m["y"] == std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
   };

   "std::map column wise"_test = [] {
      std::map<std::string, std::vector<uint64_t>> m;
      auto& x = m["x"];
      auto& y = m["y"];

      for (size_t i = 0; i < 10; ++i) {
         x.emplace_back(i);
         y.emplace_back(i + 1);
      }

      std::string out{};
      glz::write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(m, out);
      expect(out == R"(x,y
0,1
1,2
2,3
3,4
4,5
5,6
6,7
7,8
8,9
9,10
)");

      out.clear();
      glz::write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(m, out);

      m.clear();
      expect(!glz::read<glz::opts{.format = glz::csv, .layout = glz::colwise}>(m, out));

      expect(m["x"] == std::vector<uint64_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
      expect(m["y"] == std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
   };

   "std::unordered_map row wise"_test = [] {
      std::unordered_map<std::string, std::vector<uint64_t>> m;
      auto& x = m["x"];
      auto& y = m["y"];

      for (size_t i = 0; i < 10; ++i) {
         x.emplace_back(i);
         y.emplace_back(i + 1);
      }

      std::string out{};
      glz::write<glz::opts{.format = glz::csv}>(m, out);
      expect(out == R"(y,1,2,3,4,5,6,7,8,9,10
x,0,1,2,3,4,5,6,7,8,9
)" || out == R"(x,0,1,2,3,4,5,6,7,8,9
y,1,2,3,4,5,6,7,8,9,10
)");

      out.clear();
      glz::write<glz::opts{.format = glz::csv}>(m, out);

      m.clear();
      expect(!glz::read<glz::opts{.format = glz::csv}>(m, out));

      expect(m["x"] == std::vector<uint64_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
      expect(m["y"] == std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
   };

   "recorder rowwise"_test = [] {
      uint64_t t{};
      uint64_t x{1};

      glz::recorder<uint64_t> recorder;
      recorder["t"] = t;
      recorder["x"] = x;

      for (size_t i = 0; i < 5; ++i) {
         recorder.update();
         ++t;
         ++x;
      }

      auto s = glz::write_csv(recorder);
      expect(s == R"(t,0,1,2,3,4
x,1,2,3,4,5)");
   };

   "recorder colwise"_test = [] {
      uint64_t t{};
      uint64_t x{1};

      glz::recorder<uint64_t> recorder;
      recorder["t"] = t;
      recorder["x"] = x;

      for (size_t i = 0; i < 5; ++i) {
         recorder.update();
         ++t;
         ++x;
      }

      std::string s;
      write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(recorder, s);
      expect(s ==
             R"(t,x
0,1
1,2
2,3
3,4
4,5
)") << s;
   };
};

struct reflect_my_struct
{
   std::vector<int> num1{};
   std::deque<float> num2{};
   std::vector<bool> maybe{};
   std::vector<std::array<int, 3>> v3s{};
};

suite reflect_my_struct_test = [] {
   "reflection read/write column wise"_test = [] {
      std::string input_col =
         R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4)";

      reflect_my_struct obj{};

      expect(!glz::read_csv<glz::colwise>(obj, input_col));

      expect(obj.num1[0] == 11);
      expect(obj.num2[2] == 66);
      expect(obj.maybe[3] == false);
      expect(obj.v3s[0] == std::array{1, 1, 1});
      expect(obj.v3s[1] == std::array{2, 2, 2});
      expect(obj.v3s[2] == std::array{3, 3, 3});
      expect(obj.v3s[3] == std::array{4, 4, 4});

      std::string out{};

      glz::write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(obj, out);
      expect(out ==
             R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4
)");

      expect(!glz::write_file_csv<glz::colwise>(obj, "csv_test_colwise.csv", std::string{}));
   };

   "reflect read/write row wise"_test = [] {
      std::string input_row =
         R"(num1,11,33,55,77
num2,22,44,66,88
maybe,1,1,0,0
v3s[0],1,2,3,4
v3s[1],1,2,3,4
v3s[2],1,2,3,4)";

      reflect_my_struct obj{};
      expect(!glz::read_csv(obj, input_row));

      expect(obj.num1[0] == 11);
      expect(obj.num2[2] == 66);
      expect(obj.maybe[3] == false);
      expect(obj.v3s[0][2] == 1);

      std::string out{};

      glz::write<glz::opts{.format = glz::csv}>(obj, out);
      expect(out ==
             R"(num1,11,33,55,77
num2,22,44,66,88
maybe,1,1,0,0
v3s[0],1,2,3,4
v3s[1],1,2,3,4
v3s[2],1,2,3,4)");

      expect(!glz::write_file_csv(obj, "csv_test_rowwise.csv", std::string{}));
   };
};

int main() { return boost::ut::cfg<>.run({.report_errors = true}); }
