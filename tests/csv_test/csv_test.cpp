#define UT_RUN_TIME_ONLY

#include <deque>
#include <map>
#include <unordered_map>

#include "glaze/csv/read.hpp"
#include "glaze/csv/write.hpp"
#include "glaze/record/recorder.hpp"
#include "ut/ut.hpp"

// Specification: https://datatracker.ietf.org/doc/html/rfc4180

using namespace ut;

struct my_struct
{
   std::vector<int> num1{};
   std::deque<float> num2{};
   std::vector<bool> maybe{};
   std::vector<std::array<int, 3>> v3s{};
};
struct issue_768_test_struct
{
   std::vector<int> num1{};
   std::vector<std::string> str1{};
   void reserve(std::size_t cap)
   {
      num1.reserve(cap);
      str1.reserve(cap);
   }
};
template <>
struct glz::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto value = glz::object(&T::num1, &T::num2, &T::maybe, &T::v3s);
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

      expect(not glz::write<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(obj, out));
      expect(out ==
             R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4
)");

      expect(!glz::write_file_csv<glz::colwise>(obj, "csv_test_colwise.csv", std::string{}));
   };

   "read/write column wise carriage return"_test = [] {
      std::string input_col =
         "num1,num2,maybe,v3s[0],v3s[1],v3s[2]\r\n11,22,1,1,1,1\r\n33,44,1,2,2,2\r\n55,66,0,3,3,3\r\n77,88,0,4,4,4";

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

      expect(not glz::write<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(obj, out));
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

      expect(not glz::write<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(obj, out));
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

      expect(not glz::write<glz::opts{.format = glz::CSV}>(obj, out));
      expect(out ==
             R"(num1,11,33,55,77
num2,22,44,66,88
maybe,1,1,0,0
v3s[0],1,2,3,4
v3s[1],1,2,3,4
v3s[2],1,2,3,4)");

      expect(!glz::write_file_csv(obj, "csv_test_rowwise.csv", std::string{}));
   };

   "read/write row wise carriage return"_test = [] {
      std::string input_row =
         "num1,11,33,55,77\r\nnum2,22,44,66,88\r\nmaybe,1,1,0,0\r\nv3s[0],1,2,3,4\r\nv3s[1],1,2,3,4\r\nv3s[2],1,2,3,4";

      my_struct obj{};
      expect(!glz::read_csv(obj, input_row));

      expect(obj.num1[0] == 11);
      expect(obj.num2[2] == 66);
      expect(obj.maybe[3] == false);
      expect(obj.v3s[0][2] == 1);

      std::string out{};

      expect(not glz::write<glz::opts{.format = glz::CSV}>(obj, out));
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
      expect(not glz::write<glz::opts{.format = glz::CSV}>(m, out));
      expect(out == R"(x,0,1,2,3,4,5,6,7,8,9
y,1,2,3,4,5,6,7,8,9,10
)");

      out.clear();
      expect(not glz::write<glz::opts{.format = glz::CSV}>(m, out));

      m.clear();
      expect(!glz::read<glz::opts{.format = glz::CSV}>(m, out));

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
      expect(not glz::write<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(m, out));
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
      expect(not glz::write<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(m, out));

      m.clear();
      expect(!glz::read<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(m, out));

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
      expect(not glz::write<glz::opts{.format = glz::CSV}>(m, out));
      expect(out == R"(y,1,2,3,4,5,6,7,8,9,10
x,0,1,2,3,4,5,6,7,8,9
)" || out == R"(x,0,1,2,3,4,5,6,7,8,9
y,1,2,3,4,5,6,7,8,9,10
)");

      out.clear();
      expect(not glz::write<glz::opts{.format = glz::CSV}>(m, out));

      m.clear();
      expect(!glz::read<glz::opts{.format = glz::CSV}>(m, out));

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
      expect(not write<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(recorder, s));
      expect(s ==
             R"(t,x
0,1
1,2
2,3
3,4
4,5
)") << s;
   };

   "issue 768 valid_record"_test = [] {
      constexpr std::string_view valid_record =
         R"(num1,str1
11,Warszawa
33,Krakow)";
      glz::context ctx{};
      issue_768_test_struct value;
      glz::error_ctx glaze_err{
         glz::read<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(value, std::string{valid_record}, ctx)};
      expect(!bool(glaze_err));
   };
   "issue 768 invalid_record 1"_test = [] {
      constexpr std::string_view invalid_record_1 =
         R"(num1,str1
11,Warszawa
33,Krakow,some text,
55,Gdynia
77,Reda)";
      glz::context ctx{};
      issue_768_test_struct value;
      glz::error_ctx glaze_err{
         glz::read<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(value, std::string{invalid_record_1}, ctx)};
      expect(bool(glaze_err));
   };

   "issue 768 invalid_record 2"_test = [] {
      constexpr std::string_view invalid_record_2 =
         R"(num1,str1
11,Warszawa
33,Krakow,some text
55,Gdynia
77,Reda)";
      glz::context ctx{};
      issue_768_test_struct value;
      glz::error_ctx glaze_err{
         glz::read<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(value, std::string{invalid_record_2}, ctx)};
      expect(bool(glaze_err));
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

      expect(not glz::write<glz::opts{.format = glz::CSV, .layout = glz::colwise}>(obj, out));
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

      expect(not glz::write<glz::opts{.format = glz::CSV}>(obj, out));
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

struct unicode_keys
{
   std::vector<int> field1{0, 1, 2};
   std::vector<int> field2{0, 1, 2};
   std::vector<int> field3{0, 1, 2};
   std::vector<int> field4{0, 1, 2};
   std::vector<int> field5{0, 1, 2};
   std::vector<int> field6{0, 1, 2};
   std::vector<int> field7{0, 1, 2};
};

// example 1
template <>
struct glz::meta<unicode_keys>
{
   using T = unicode_keys;
   static constexpr auto value = object("alpha", &T::field1, "bravo", &T::field2, "charlie", &T::field3, "♥️",
                                        &T::field4, "delta", &T::field5, "echo", &T::field6, "😄", &T::field7);
};

struct unicode_keys2
{
   std::vector<int> field1{0, 1, 2};
   std::vector<int> field2{0, 1, 2};
   std::vector<int> field3{0, 1, 2};
};

template <>
struct glz::meta<unicode_keys2>
{
   using T = unicode_keys2;
   static constexpr auto value = object("😄", &T::field1, "💔", &T::field2, "alpha", &T::field3);
};

struct unicode_keys3
{
   std::vector<int> field0{0, 1, 2};
   std::vector<int> field1{0, 1, 2};
   std::vector<int> field2{0, 1, 2};
   std::vector<int> field3{0, 1, 2};
   std::vector<int> field4{0, 1, 2};
   std::vector<int> field5{0, 1, 2};
   std::vector<int> field6{0, 1, 2};
};

template <>
struct glz::meta<unicode_keys3>
{
   using T = unicode_keys3;
   static constexpr auto value = object("简体汉字", &T::field0, // simplified chinese characters
                                        "漢字寿限無寿限無五劫", &T::field1, // traditional chinese characters / kanji
                                        "こんにちはむところやぶら", &T::field2, // katakana
                                        "한국인", &T::field3, // korean
                                        "русский", &T::field4, // cyrillic
                                        "สวัสดี", &T::field5, // thai
                                        "english", &T::field6);
};

suite unicode_keys_test = [] {
   "unicode_keys"_test = [] {
      unicode_keys obj{};
      std::string buffer{};
      expect(not glz::write_csv(obj, buffer));

      expect(!glz::read_csv(obj, buffer));
   };

   "unicode_keys2"_test = [] {
      unicode_keys2 obj{};
      std::string buffer{};
      expect(not glz::write_csv(obj, buffer));

      expect(!glz::read_csv(obj, buffer));
   };

   "unicode_keys3"_test = [] {
      unicode_keys3 obj{};
      std::string buffer{};
      expect(not glz::write_csv(obj, buffer));

      expect(!glz::read_csv(obj, buffer));
   };
};

struct FishRecord
{
   std::vector<float> Duration;
   std::vector<float> FishSize;
   std::vector<std::uint8_t> Amount;

   std::vector<std::string> FishBaitName;
   std::vector<std::string> SurfaceSlapFishName;
   std::vector<std::string> MoochFishName;
   std::vector<std::string> BuffName;
   std::vector<std::string> FishingSpotPlaceName;

   std::vector<std::string> BiteTypeName;
   std::vector<std::string> CaughtFishName;
   std::vector<std::string> HooksetName;
   std::vector<std::string> IsLargeSizeName;
   std::vector<std::string> IsCollectableName;
};

template <>
struct glz::meta<FishRecord>
{
   using T = FishRecord;
   static constexpr auto value = object("上钩的鱼", &T::CaughtFishName, //
                                        "间隔", &T::Duration, //
                                        "尺寸", &T::FishSize, //
                                        "数量", &T::Amount, //
                                        "鱼饵", &T::FishBaitName, //
                                        "拍水的鱼", &T::SurfaceSlapFishName, //
                                        "以小钓大的鱼", &T::MoochFishName, //
                                        "Buff", &T::BuffName, //
                                        "钓场", &T::FishingSpotPlaceName, //
                                        "咬钩类型", &T::BiteTypeName, //
                                        "提钩类型", &T::HooksetName, //
                                        "大尺寸", &T::IsLargeSizeName, //
                                        "收藏品", &T::IsCollectableName //
   );
};

suite fish_record = [] {
   "fish_record"_test = [] {
      FishRecord obj{};
      std::string buffer{};
      expect(not glz::write_csv<glz::colwise>(obj, buffer));

      expect(!glz::read_csv<glz::colwise>(obj, buffer));
   };
};

int main() { return 0; }
