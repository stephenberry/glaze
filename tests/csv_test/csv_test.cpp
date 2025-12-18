#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <unordered_map>

#include "glaze/base64/base64.hpp"
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

struct signed_min_columns
{
   std::vector<std::int8_t> i8{};
   std::vector<std::int32_t> i32{};
};

template <>
struct glz::meta<signed_min_columns>
{
   using T = signed_min_columns;
   static constexpr auto value = glz::object("i8", &T::i8, "i32", &T::i32);
};

struct csv_char_row
{
   bool operator==(const csv_char_row&) const = default;

   char letter{};
   int count{};
};

template <>
struct glz::meta<csv_char_row>
{
   using T = csv_char_row;
   static constexpr auto value = glz::object(&T::letter, &T::count);
};

constexpr glz::opts_csv rowwise_char_opts{
   .layout = glz::rowwise,
   .use_headers = false,
   .raw_string = true,
};

constexpr glz::opts_csv rowwise_char_opts_with_escaping{
   .layout = glz::rowwise,
   .use_headers = false,
   .raw_string = false,
};

enum struct csv_color : std::uint8_t {
   red = 0,
   green = 1,
   blue = 2,
};

template <>
struct glz::meta<csv_color>
{
   using enum csv_color;
   static constexpr auto value = enumerate("rouge", red, "vert", green, "bleu", blue);
};

struct enum_column_struct
{
   std::vector<csv_color> colors{};
};

template <>
struct glz::meta<enum_column_struct>
{
   using T = enum_column_struct;
   static constexpr auto value = glz::object("colors", &T::colors);
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

      expect(not glz::write<glz::opts_csv{.layout = glz::colwise}>(obj, out));
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

      expect(not glz::write<glz::opts_csv{.layout = glz::colwise}>(obj, out));
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

      expect(not glz::write<glz::opts_csv{.layout = glz::colwise}>(obj, out));
      expect(out ==
             R"(id,udl
1,BRN
2,STR
3,ASD
4,USN
)");

      expect(!glz::write_file_csv<glz::colwise>(obj, "csv_test_colwise.csv", std::string{}));
   };

   "signed minimum integers"_test = [] {
      std::string input =
         R"(i8,i32
-128,-2147483648)";

      signed_min_columns obj{};
      expect(!glz::read_csv<glz::colwise>(obj, input));

      expect(obj.i8.size() == 1);
      expect(obj.i32.size() == 1);
      expect(obj.i8[0] == std::numeric_limits<std::int8_t>::min());
      expect(obj.i32[0] == std::numeric_limits<std::int32_t>::min());
   };

   "rowwise char round trip"_test = [] {
      std::vector<csv_char_row> data{
         {.letter = 'A', .count = 42},
         {.letter = ',', .count = -7},
         {.letter = '"', .count = 0},
      };

      std::string buffer{};
      expect(not glz::write<rowwise_char_opts>(data, buffer));

      std::vector<csv_char_row> result{};
      glz::error_ctx ec{glz::read<rowwise_char_opts>(result, buffer)};
      expect(!ec) << glz::format_error(ec, buffer) << '\n';

      expect(result.size() == data.size());
      for (size_t i = 0; i < data.size(); ++i) {
         expect(result[i] == data[i]);
      }
   };

   "rowwise char empty field"_test = [] {
      std::string csv_data = ",7\n";

      std::vector<csv_char_row> result{};
      glz::error_ctx ec{glz::read<rowwise_char_opts>(result, csv_data)};
      expect(!ec) << glz::format_error(ec, csv_data) << '\n';

      expect(result.size() == 1);
      expect(result.front().letter == char{});
      expect(result.front().count == 7);
   };

   "rowwise char quoted input"_test = [] {
      std::string csv_data = "\"\"\"\",9\n";

      std::vector<csv_char_row> result{};
      glz::error_ctx ec{glz::read<rowwise_char_opts_with_escaping>(result, csv_data)};
      expect(!ec) << glz::format_error(ec, csv_data) << '\n';

      expect(result.size() == 1);
      expect(result.front().letter == '"');
      expect(result.front().count == 9);
   };

   "rowwise char multi character error"_test = [] {
      std::string csv_data = "AB,5\n";

      std::vector<csv_char_row> result{};
      glz::error_ctx ec{glz::read<rowwise_char_opts>(result, csv_data)};
      expect(ec);
      expect(ec == glz::error_code::syntax_error);
   };

   "rowwise char numeric string error"_test = [] {
      std::string csv_data = "65,3\n";

      std::vector<csv_char_row> result{};
      glz::error_ctx ec{glz::read<rowwise_char_opts>(result, csv_data)};
      expect(ec);
      expect(ec == glz::error_code::syntax_error);
   };

   "named enum column wise"_test = [] {
      std::string input =
         R"(colors
rouge
vert
bleu)";

      enum_column_struct obj{};
      expect(!glz::read_csv<glz::colwise>(obj, input));

      expect(obj.colors.size() == 3);
      expect(obj.colors[0] == csv_color::red);
      expect(obj.colors[1] == csv_color::green);
      expect(obj.colors[2] == csv_color::blue);
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

      expect(not glz::write<glz::opts_csv{}>(obj, out));
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

      expect(not glz::write<glz::opts_csv{}>(obj, out));
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
      expect(not glz::write<glz::opts_csv{}>(m, out));
      expect(out == R"(x,0,1,2,3,4,5,6,7,8,9
y,1,2,3,4,5,6,7,8,9,10
)");

      out.clear();
      expect(not glz::write<glz::opts_csv{}>(m, out));

      m.clear();
      expect(!glz::read<glz::opts_csv{}>(m, out));

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
      expect(not glz::write<glz::opts_csv{.layout = glz::colwise}>(m, out));
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
      expect(not glz::write<glz::opts_csv{.layout = glz::colwise}>(m, out));

      m.clear();
      expect(!glz::read<glz::opts_csv{.layout = glz::colwise}>(m, out));

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
      expect(not glz::write<glz::opts_csv{}>(m, out));
      expect(out == R"(y,1,2,3,4,5,6,7,8,9,10
x,0,1,2,3,4,5,6,7,8,9
)" || out == R"(x,0,1,2,3,4,5,6,7,8,9
y,1,2,3,4,5,6,7,8,9,10
)");

      out.clear();
      expect(not glz::write<glz::opts_csv{}>(m, out));

      m.clear();
      expect(!glz::read<glz::opts_csv{}>(m, out));

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
      expect(not write<glz::opts_csv{.layout = glz::colwise}>(recorder, s));
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
      glz::error_ctx glaze_err{glz::read<glz::opts_csv{.layout = glz::colwise}>(value, std::string{valid_record}, ctx)};
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
         glz::read<glz::opts_csv{.layout = glz::colwise}>(value, std::string{invalid_record_1}, ctx)};
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
         glz::read<glz::opts_csv{.layout = glz::colwise}>(value, std::string{invalid_record_2}, ctx)};
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

      expect(not glz::write<glz::opts_csv{.layout = glz::colwise}>(obj, out));
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

      expect(not glz::write<glz::opts_csv{}>(obj, out));
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

struct CurrencyCSV
{
   std::vector<std::string> Entity;
   std::vector<std::string> Currency;
   std::vector<std::string> AlphabeticCode;
   std::vector<std::string> NumericCode;
   std::vector<std::string> MinorUnit;
   std::vector<std::string> WithdrawalDate;
};

suite currency_csv_test = [] {
   "currency_col"_test = [] {
      CurrencyCSV obj{};
      std::string buffer{};
      auto ec = glz::read_file_csv<glz::colwise>(obj, GLZ_TEST_DIRECTORY "/currency.csv", buffer);
      expect(not ec) << glz::format_error(ec, buffer) << '\n';

      constexpr auto kExpectedSize = 445;

      expect(obj.Entity.size() == kExpectedSize);
      expect(obj.Currency.size() == kExpectedSize);
      expect(obj.AlphabeticCode.size() == kExpectedSize);
      expect(obj.NumericCode.size() == kExpectedSize);
      expect(obj.MinorUnit.size() == kExpectedSize);
      expect(obj.WithdrawalDate.size() == kExpectedSize);

      expect(obj.Entity[0] == "AFGHANISTAN");
      expect(obj.Currency[0] == "Afghani");
      expect(obj.AlphabeticCode[0] == "AFN");
      expect(obj.NumericCode[0] == "971");
      expect(obj.MinorUnit[0] == "2");
      expect(obj.WithdrawalDate[0] == "");

      expect(obj.Entity[29] == "BONAIRE, SINT EUSTATIUS AND SABA");
      expect(obj.Currency[29] == "US Dollar");
      expect(obj.AlphabeticCode[29] == "USD");
      expect(obj.NumericCode[29] == "840");
      expect(obj.MinorUnit[29] == "2");
      expect(obj.WithdrawalDate[29] == "");

      expect(obj.Entity[324] == "EUROPEAN MONETARY CO-OPERATION FUND (EMCF)");
      expect(obj.Currency[324] == "European Currency Unit (E.C.U)");
      expect(obj.AlphabeticCode[324] == "XEU");
      expect(obj.NumericCode[324] == "954");
      expect(obj.MinorUnit[324] == "");
      expect(obj.WithdrawalDate[324] == "1999-01");
   };
   "currency_row"_test = [] {
      /*CurrencyCSV obj{};
      std::string buffer{};
      auto ec = glz::read_file_csv<glz::colwise>(obj, GLZ_TEST_DIRECTORY "/currency.csv", buffer);
      ec = glz::write_file_csv(obj, "currency_rowwise.csv", std::string{});
      expect(not ec);
      ec = glz::read_file_csv(obj, "currency_rowwise.csv", buffer);
      expect(not ec) << glz::format_error(ec, buffer) << '\n';*/
   };
};

suite quoted_fields_csv_test = [] {
   "quoted_fields_with_commas_and_brackets"_test = [] {
      std::string csv_data = R"(Entity,Currency,AlphabeticCode,NumericCode,MinorUnit,WithdrawalDate
"MOLDOVA, REPUBLIC OF",Russian Ruble,RUR,810,,1993-12
"FALKLAND ISLANDS (THE) [MALVINAS]",Falkland Islands Pound,FKP,238,2,
"BONAIRE, SINT EUSTATIUS AND SABA",US Dollar,USD,840,2,)";

      CurrencyCSV obj{};
      auto ec = glz::read<glz::opts_csv{.layout = glz::colwise}>(obj, csv_data);
      expect(!ec) << "Should parse quoted fields with commas and brackets\n";

      expect(obj.Entity.size() == 3) << "Should have 3 entities\n";
      expect(obj.Entity[0] == "MOLDOVA, REPUBLIC OF") << "First entity should preserve comma\n";
      expect(obj.Entity[1] == "FALKLAND ISLANDS (THE) [MALVINAS]") << "Second entity should preserve brackets\n";
      expect(obj.Entity[2] == "BONAIRE, SINT EUSTATIUS AND SABA") << "Third entity should preserve comma\n";

      expect(obj.Currency[0] == "Russian Ruble") << "First currency should be 'Russian Ruble'\n";
      expect(obj.Currency[1] == "Falkland Islands Pound") << "Second currency should be 'Falkland Islands Pound'\n";
      expect(obj.Currency[2] == "US Dollar") << "Third currency should be 'US Dollar'\n";

      expect(obj.AlphabeticCode[0] == "RUR") << "First code should be 'RUR'\n";
      expect(obj.AlphabeticCode[1] == "FKP") << "Second code should be 'FKP'\n";
      expect(obj.AlphabeticCode[2] == "USD") << "Third code should be 'USD'\n";
   };

   "quoted_fields_with_escaped_quotes"_test = [] {
      std::string csv_data = R"(Entity,Currency,AlphabeticCode
"Country with ""quotes""",Some Currency,ABC
"Another ""quoted"" country",Other Currency,DEF)";

      CurrencyCSV obj{};
      auto ec = glz::read<glz::opts_csv{.layout = glz::colwise}>(obj, csv_data);
      expect(!ec) << "Should parse quoted fields with escaped quotes\n";

      expect(obj.Entity.size() == 2) << "Should have 2 entities\n";
      expect(obj.Entity[0] == "Country with \"quotes\"") << "First entity should have unescaped quotes\n";
      expect(obj.Entity[1] == "Another \"quoted\" country") << "Second entity should have unescaped quotes\n";
   };

   "mixed_quoted_and_unquoted_fields"_test = [] {
      std::string csv_data = R"(Entity,Currency,AlphabeticCode
"QUOTED, FIELD",Unquoted Currency,ABC
Unquoted Field,"QUOTED, CURRENCY",DEF
"BOTH, QUOTED","CURRENCY, TOO",GHI)";

      CurrencyCSV obj{};
      auto ec = glz::read<glz::opts_csv{.layout = glz::colwise}>(obj, csv_data);
      expect(!ec) << "Should parse mixed quoted and unquoted fields\n";

      expect(obj.Entity.size() == 3) << "Should have 3 entities\n";
      expect(obj.Entity[0] == "QUOTED, FIELD") << "First entity should preserve comma\n";
      expect(obj.Entity[1] == "Unquoted Field") << "Second entity should be unquoted\n";
      expect(obj.Entity[2] == "BOTH, QUOTED") << "Third entity should preserve comma\n";

      expect(obj.Currency[0] == "Unquoted Currency") << "First currency should be unquoted\n";
      expect(obj.Currency[1] == "QUOTED, CURRENCY") << "Second currency should preserve comma\n";
      expect(obj.Currency[2] == "CURRENCY, TOO") << "Third currency should preserve comma\n";
   };

   "raw_string_option_test"_test = [] {
      // Test with raw_string = true - should still work but handle escapes differently
      std::string csv_data = R"(Entity,Currency
"MOLDOVA, REPUBLIC OF",Russian Ruble
"Country with ""quotes""",Some Currency)";

      CurrencyCSV obj{};
      auto ec = glz::read<glz::opts_csv{.layout = glz::colwise, .raw_string = true}>(obj, csv_data);
      expect(!ec) << "Should parse with raw_string = true\n";

      expect(obj.Entity.size() == 2) << "Should have 2 entities\n";
      expect(obj.Entity[0] == "MOLDOVA, REPUBLIC OF") << "First entity should preserve comma\n";
      expect(obj.Entity[1] == "Country with \"quotes\"") << "Second entity should handle quotes with raw_string\n";
   };

   "empty_quoted_fields"_test = [] {
      std::string csv_data = R"(Entity,Currency,AlphabeticCode
"",Non-empty,ABC
"Non-empty","",DEF
"","",GHI)";

      CurrencyCSV obj{};
      auto ec = glz::read<glz::opts_csv{.layout = glz::colwise}>(obj, csv_data);
      expect(!ec) << "Should parse empty quoted fields\n";

      expect(obj.Entity.size() == 3) << "Should have 3 entities\n";
      expect(obj.Entity[0] == "") << "First entity should be empty\n";
      expect(obj.Entity[1] == "Non-empty") << "Second entity should be non-empty\n";
      expect(obj.Entity[2] == "") << "Third entity should be empty\n";

      expect(obj.Currency[0] == "Non-empty") << "First currency should be non-empty\n";
      expect(obj.Currency[1] == "") << "Second currency should be empty\n";
      expect(obj.Currency[2] == "") << "Third currency should be empty\n";
   };

   "complex_quoted_content"_test = [] {
      std::string csv_data = R"(Entity,Currency,AlphabeticCode
"COUNTRY (WITH) [BRACKETS], COMMAS, AND ""QUOTES""",Complex Currency,ABC
"Line 1
Line 2",Multi-line Currency,DEF)";

      CurrencyCSV obj{};
      auto ec = glz::read<glz::opts_csv{.layout = glz::colwise}>(obj, csv_data);
      expect(!ec) << "Should parse complex quoted content\n";

      expect(obj.Entity.size() == 2) << "Should have 2 entities\n";
      expect(obj.Entity[0] == "COUNTRY (WITH) [BRACKETS], COMMAS, AND \"QUOTES\"")
         << "First entity should preserve all special characters\n";
      expect(obj.Entity[1] == "Line 1\nLine 2") << "Second entity should preserve newlines\n";
   };
};

struct csv_headers_struct
{
   std::vector<int> num1{1, 2, 3};
   std::vector<float> num2{4.0f, 5.0f, 6.0f};
   std::vector<std::string> text{"a", "b", "c"};
};

template <>
struct glz::meta<csv_headers_struct>
{
   using T = csv_headers_struct;
   static constexpr auto value = glz::object(&T::num1, &T::num2, &T::text);
};

struct data_point
{
   int id{};
   float value{};
   std::string name{};
};

template <>
struct glz::meta<data_point>
{
   using T = data_point;
   static constexpr auto value = glz::object(&T::id, &T::value, &T::name);
};

struct data_with_skip
{
   int id{};
   std::string name{};
};

template <>
struct glz::meta<data_with_skip>
{
   using T = data_with_skip;
   static constexpr auto value = glz::object("id", &T::id, "unused", glz::skip{}, "name", &T::name);
};

struct rowwise_data_with_skip
{
   std::vector<int> id{};
   std::vector<std::string> name{};
};

template <>
struct glz::meta<rowwise_data_with_skip>
{
   using T = rowwise_data_with_skip;
   static constexpr auto value = glz::object("id", &T::id, "unused", glz::skip{}, "name", &T::name);
};

suite csv_headers_control_tests = [] {
   "rowwise_with_headers"_test = [] {
      csv_headers_struct obj{};
      std::string buffer{};

      expect(not glz::write<glz::opts_csv{.use_headers = true}>(obj, buffer));
      expect(buffer ==
             R"(num1,1,2,3
num2,4,5,6
text,a,b,c
)");
   };

   "rowwise_without_headers"_test = [] {
      csv_headers_struct obj{};
      std::string buffer{};

      expect(not glz::write<glz::opts_csv{.use_headers = false}>(obj, buffer));
      expect(buffer ==
             R"(1,2,3
4,5,6
a,b,c
)");
   };

   "colwise_with_headers"_test = [] {
      csv_headers_struct obj{};
      std::string buffer{};

      expect(not glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = true}>(obj, buffer));
      expect(buffer ==
             R"(num1,num2,text
1,4,a
2,5,b
3,6,c
)");
   };

   "colwise_without_headers"_test = [] {
      csv_headers_struct obj{};
      std::string buffer{};

      expect(not glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(obj, buffer));
      expect(buffer ==
             R"(1,4,a
2,5,b
3,6,c
)");
   };

   "incremental_writing"_test = [] {
      csv_headers_struct obj{};
      std::string result;
      std::string buffer;

      // First write with headers
      expect(not glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = true}>(obj, buffer));
      result.append(buffer);

      // Subsequent write without headers
      buffer.clear();
      expect(not glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(obj, buffer));
      result.append(buffer);

      expect(result ==
             R"(num1,num2,text
1,4,a
2,5,b
3,6,c
1,4,a
2,5,b
3,6,c
)");
   };
};

// Read the CSV data into a struct with vectors
struct csv_data
{
   std::vector<int> id{};
   std::vector<float> value{};
   std::vector<std::string> name{};

   struct glaze
   {
      using T = csv_data;
      static constexpr auto value = glz::object(&T::id, &T::value, &T::name);
   };
};

// Test suite for the vector-of-structs feature
suite vector_struct_csv_tests = [] {
   "vector_of_structs_with_headers"_test = [] {
      std::vector<data_point> data = {{1, 10.5f, "Point A"}, {2, 20.3f, "Point B"}, {3, 15.7f, "Point C"}};

      std::string buffer{};
      expect(not glz::write<glz::opts_csv{}>(data, buffer));

      expect(buffer ==
             R"(id,value,name
1,10.5,Point A
2,20.3,Point B
3,15.7,Point C
)");
   };

   "vector_of_structs_without_headers"_test = [] {
      std::vector<data_point> data = {{1, 10.5f, "Point A"}, {2, 20.3f, "Point B"}, {3, 15.7f, "Point C"}};

      std::string buffer{};
      expect(not glz::write<glz::opts_csv{.use_headers = false}>(data, buffer));

      expect(buffer ==
             R"(1,10.5,Point A
2,20.3,Point B
3,15.7,Point C
)");
   };

   "empty_vector"_test = [] {
      std::vector<data_point> data{};

      std::string buffer{};
      expect(not glz::write<glz::opts_csv{}>(data, buffer));

      // Should only contain headers
      expect(buffer == R"(id,value,name
)");
   };

   "vector_roundtrip"_test = [] {
      // Create test data
      std::vector<data_point> original = {{1, 10.5f, "Point A"}, {2, 20.3f, "Point B"}, {3, 15.7f, "Point C"}};

      // Write to CSV string
      std::string csv_str{};
      expect(not glz::write<glz::opts_csv{}>(original, csv_str));

      csv_data data{};
      expect(not glz::read<glz::opts_csv{.layout = glz::colwise}>(data, csv_str));

      // Verify the data was read correctly
      expect(data.id.size() == 3);
      expect(data.value.size() == 3);
      expect(data.name.size() == 3);

      expect(data.id[0] == 1);
      expect(data.id[1] == 2);
      expect(data.id[2] == 3);

      expect(data.value[0] == 10.5f);
      expect(data.value[1] == 20.3f);
      expect(data.value[2] == 15.7f);

      expect(data.name[0] == "Point A");
      expect(data.name[1] == "Point B");
      expect(data.name[2] == "Point C");
   };
};

suite vector_struct_direct_read_tests = [] {
   "read_vector_of_structs"_test = [] {
      // Create test data
      std::vector<data_point> original = {{1, 10.5f, "Point A"}, {2, 20.3f, "Point B"}, {3, 15.7f, "Point C"}};

      // Write to CSV string
      std::string csv_str{};
      expect(not glz::write<glz::opts_csv{}>(original, csv_str));

      // This is what the CSV looks like
      expect(csv_str ==
             R"(id,value,name
1,10.5,Point A
2,20.3,Point B
3,15.7,Point C
)");

      // Now read directly back into vector of structs
      std::vector<data_point> read_back{};
      expect(not glz::read<glz::opts_csv{.layout = glz::colwise}>(read_back, csv_str));

      // Verify the data was read correctly
      expect(read_back.size() == 3);

      expect(read_back[0].id == 1);
      expect(read_back[0].value == 10.5f);
      expect(read_back[0].name == "Point A");

      expect(read_back[1].id == 2);
      expect(read_back[1].value == 20.3f);
      expect(read_back[1].name == "Point B");

      expect(read_back[2].id == 3);
      expect(read_back[2].value == 15.7f);
      expect(read_back[2].name == "Point C");
   };

   "read_vector_of_structs_without_headers"_test = [] {
      std::string csv_str =
         R"(1,10.5,Point A
2,20.3,Point B
3,15.7,Point C)";

      std::vector<data_point> read_back{};
      expect(not glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(read_back, csv_str));

      // Verify the data was read correctly
      expect(read_back.size() == 3);

      expect(read_back[0].id == 1);
      expect(read_back[0].value == 10.5f);
      expect(read_back[0].name == "Point A");

      expect(read_back[1].id == 2);
      expect(read_back[1].value == 20.3f);
      expect(read_back[1].name == "Point B");

      expect(read_back[2].id == 3);
      expect(read_back[2].value == 15.7f);
      expect(read_back[2].name == "Point C");
   };

   "append_to_vector"_test = [] {
      // Initial vector with some data
      std::vector<data_point> data = {{1, 10.5f, "Point A"}};

      // CSV with additional data
      std::string csv_str =
         R"(id,value,name
2,20.3,Point B
3,15.7,Point C)";

      // Append the new data
      struct colwise_append_opts : glz::opts_csv
      {
         uint8_t layout = glz::colwise;
         bool append_arrays = true;
      };
      expect(not glz::read<colwise_append_opts{}>(data, csv_str));

      // Verify combined data
      expect(data.size() == 3);
      expect(data[0].id == 1);
      expect(data[1].id == 2);
      expect(data[2].id == 3);
   };
};

// Test suite for vector<data_point> without headers
suite vector_data_point_no_headers_tests = [] {
   "write_vector_data_point_no_headers"_test = [] {
      std::vector<data_point> data = {{1, 10.5f, "Point A"}, {2, 20.3f, "Point B"}, {3, 15.7f, "Point C"}};

      std::string output;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));

      expect(output == R"(1,10.5,Point A
2,20.3,Point B
3,15.7,Point C
)") << "Should write data without headers";
   };

   "read_vector_data_point_no_headers"_test = [] {
      std::string csv_data = R"(4,25.5,Point D
5,30.2,Point E
6,35.9,Point F)";

      std::vector<data_point> data;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(data, csv_data));

      expect(data.size() == 3) << "Should read 3 data points";
      expect(data[0].id == 4) << "First point id should be 4";
      expect(data[0].value == 25.5f) << "First point value should be 25.5";
      expect(data[0].name == "Point D") << "First point name should be Point D";
      expect(data[2].id == 6) << "Last point id should be 6";
      expect(data[2].name == "Point F") << "Last point name should be Point F";
   };

   "roundtrip_vector_data_point_no_headers"_test = [] {
      std::vector<data_point> original = {
         {10, 100.5f, "Alpha"}, {20, 200.3f, "Beta"}, {30, 300.7f, "Gamma"}, {40, 400.1f, "Delta"}};

      std::string buffer;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(original, buffer));

      std::vector<data_point> result;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, buffer));

      expect(result.size() == original.size()) << "Sizes should match after roundtrip";
      for (size_t i = 0; i < original.size(); ++i) {
         expect(result[i].id == original[i].id) << "IDs should match";
         expect(result[i].value == original[i].value) << "Values should match";
         expect(result[i].name == original[i].name) << "Names should match";
      }
   };

   "read_with_skip_header_vector_data_point"_test = [] {
      // CSV with header that we want to skip
      std::string csv_with_header = R"(id,value,name
100,1000.5,Header Test A
200,2000.3,Header Test B
300,3000.7,Header Test C)";

      std::vector<data_point> data;
      expect(!glz::read<glz::opts_csv{.use_headers = false, .skip_header_row = true}>(data, csv_with_header));

      expect(data.size() == 3) << "Should read 3 data points after skipping header";
      expect(data[0].id == 100) << "First id should be 100";
      expect(data[0].name == "Header Test A") << "First name should be correct";
      expect(data[2].id == 300) << "Last id should be 300";
   };

   "empty_vector_data_point_no_headers"_test = [] {
      std::vector<data_point> data;
      std::string output;

      expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));
      expect(output == "") << "Empty vector should produce empty output";

      // For structs/objects, empty CSV is an error (unlike whitespace-only which is valid)
      std::vector<data_point> result;
      std::string test_input = ""; // Explicitly using empty string
      auto ec = glz::read<glz::opts_csv{.use_headers = false}>(result, test_input);
      expect(bool(ec)) << "Empty CSV should be an error for struct types";
      expect(static_cast<int>(ec.ec) == 12) << "Should be no_read_input error";
   };

   "single_element_vector_data_point"_test = [] {
      std::vector<data_point> data = {{42, 42.42f, "Single"}};

      std::string output;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));
      expect(output == "42,42.42,Single\n") << "Single element should write correctly";

      std::vector<data_point> result;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, output));
      expect(result.size() == 1) << "Should read single element";
      expect(result[0].id == 42) << "ID should match";
      expect(result[0].value == 42.42f) << "Value should match";
      expect(result[0].name == "Single") << "Name should match";
   };

   "quoted_strings_vector_data_point"_test = [] {
      std::vector<data_point> data = {
         {1, 1.1f, "Name, with comma"}, {2, 2.2f, "Name \"with\" quotes"}, {3, 3.3f, "Multi\nline\nname"}};

      std::string output;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));

      // Verify the output has proper quoting
      expect(output.find("\"Name, with comma\"") != std::string::npos) << "Should quote strings with commas";
      expect(output.find("\"Name \"\"with\"\" quotes\"") != std::string::npos)
         << "Should escape quotes in quoted strings";

      // Read it back
      std::vector<data_point> result;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, output));

      expect(result.size() == 3) << "Should read all elements";
      expect(result[0].name == "Name, with comma") << "Should preserve comma in string";
      expect(result[1].name == "Name \"with\" quotes") << "Should preserve quotes";
      expect(result[2].name == "Multi\nline\nname") << "Should preserve newlines";
   };

   "vector_of_structs_with_skipped_column"_test = [] {
      const std::string csv =
         "id,unused,name\r\n"
         "1,foo,Alice\r\n"
         "2,\"multi\r\nline\",Bob\r\n"
         "3,,Charlie\r\n";

      std::vector<data_with_skip> parsed;
      expect(!glz::read<glz::opts_csv{.layout = glz::colwise}>(parsed, csv));

      expect(parsed.size() == 3);
      expect(parsed[0].id == 1);
      expect(parsed[0].name == "Alice");
      expect(parsed[1].id == 2);
      expect(parsed[1].name == "Bob");
      expect(parsed[2].id == 3);
      expect(parsed[2].name == "Charlie");
   };

   "rowwise_object_with_skipped_row"_test = [] {
      const std::string csv =
         "id,1,2,3\r\n"
         "unused,foo,bar,baz\r\n"
         "name,Alice,Bob,Charlie\r\n";

      rowwise_data_with_skip parsed;
      expect(!glz::read<glz::opts_csv{.layout = glz::rowwise, .use_headers = false}>(parsed, csv));

      expect(parsed.id == std::vector<int>{1, 2, 3});
      expect(parsed.name == std::vector<std::string>{"Alice", "Bob", "Charlie"});
   };

   "csv_string_edge_cases"_test = [] {
      // Test empty strings
      {
         std::vector<data_point> data = {{1, 1.0f, ""}};
         std::string output;
         expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));
         expect(output == "1,1,\n") << "Empty string should not be quoted";

         std::vector<data_point> result;
         expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, output));
         expect(result[0].name == "") << "Should preserve empty string";
      }

      // Test strings with only quotes
      {
         std::vector<data_point> data = {{2, 2.0f, "\"\"\""}};
         std::string output;
         expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));
         expect(output.find("\"\"\"\"\"\"\"") != std::string::npos)
            << "Three quotes should become six quotes inside quotes";
      }

      // Test strings without special characters (should not be quoted)
      {
         std::vector<data_point> data = {{3, 3.0f, "NormalString"}};
         std::string output;
         expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));
         expect(output == "3,3,NormalString\n") << "Normal strings should not be quoted";
      }

      // Test roundtrip with mixed strings
      {
         std::vector<data_point> data = {
            {1, 1.0f, "Normal"}, {2, 2.0f, "Has,comma"}, {3, 3.0f, ""}, {4, 4.0f, "Has\r\nCRLF"}};

         std::string output;
         expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, output));

         std::vector<data_point> result;
         expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, output));

         expect(result.size() == 4) << "Should read all items";
         expect(result[0].name == "Normal");
         expect(result[1].name == "Has,comma");
         expect(result[2].name == "");
         expect(result[3].name == "Has\r\nCRLF");
      }
   };

   "append_mode_vector_data_point"_test = [] {
      std::vector<data_point> initial = {{1, 1.0f, "First"}};

      std::string more_data = R"(2,2.0,Second
3,3.0,Third)";

      struct no_headers_append_opts : glz::opts_csv
      {
         bool use_headers = false;
         bool append_arrays = true;
      };
      expect(!glz::read<no_headers_append_opts{}>(initial, more_data));

      expect(initial.size() == 3) << "Should have 3 elements after appending";
      expect(initial[0].id == 1) << "Original data preserved";
      expect(initial[1].id == 2) << "First appended element";
      expect(initial[2].id == 3) << "Second appended element";
   };

   "mixed_numeric_formats_data_point"_test = [] {
      std::string csv_data = R"(1,1e2,Scientific
2,-3.14,Negative
3,0.0001,Small
4,9999999.9,Large)";

      std::vector<data_point> data;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(data, csv_data));

      expect(data.size() == 4) << "Should read all numeric formats";
      expect(data[0].value == 100.0f) << "Should parse scientific notation";
      expect(data[1].value == -3.14f) << "Should parse negative numbers";
      expect(data[2].value == 0.0001f) << "Should parse small numbers";
      expect(data[3].value == 9999999.9f) << "Should parse large numbers";
   };

   "trailing_comma_data_point"_test = [] {
      // Note: This might be an error case depending on the struct
      // since data_point has exactly 3 fields
      std::string csv_with_trailing = R"(1,1.0,Test,
2,2.0,Test2,)";

      std::vector<data_point> data;
      auto error = glz::read<glz::opts_csv{.use_headers = false}>(data, csv_with_trailing);

      // This should either parse successfully (ignoring extra field)
      // or fail gracefully - but shouldn't crash
      if (!error) {
         expect(data.size() <= 2) << "Should handle trailing commas gracefully";
      }
   };
};

struct person_data
{
   std::vector<int> id{};
   std::vector<std::string> name{};
   std::vector<std::string> description{};
};

struct simple_data
{
   std::vector<int> num1{};
   std::vector<int> num2{};

   struct glaze
   {
      using T = simple_data;
      static constexpr auto value = glz::object(&T::num1, &T::num2);
   };
};

struct test_data
{
   std::vector<int> id{};
   std::vector<std::string> name{};
   std::vector<std::string> value{};
};

struct unicode_data
{
   std::vector<std::string> field1{};
   std::vector<std::string> field2{};
};

struct large_data
{
   std::vector<int> id{};
   std::vector<int> value{};
};

struct mixed_data
{
   std::vector<int> a{};
   std::vector<int> b{};
};

struct field_data
{
   std::vector<std::string> field{};
};

struct num_data
{
   std::vector<int> num{};
};

suite non_null_terminated_buffer_tests = [] {
   // Helper function to create a non-null-terminated buffer from a string
   auto make_buffer = [](const std::string& str) -> std::vector<char> {
      std::vector<char> buffer;
      buffer.reserve(str.size());
      for (char c : str) {
         buffer.push_back(c);
      }
      return buffer;
   };

   "basic_colwise_non_null_buffer"_test = [&] {
      std::string csv_data =
         R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4)";

      auto buffer = make_buffer(csv_data);
      my_struct obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer)) << "Should parse non-null-terminated buffer\n";

      expect(obj.num1[0] == 11) << "First num1 value should be 11\n";
      expect(obj.num2[2] == 66) << "Third num2 value should be 66\n";
      expect(obj.maybe[3] == false) << "Fourth maybe value should be false\n";
      expect(obj.v3s[0] == std::array{1, 1, 1}) << "First v3s should be {1,1,1}\n";
      expect(obj.v3s[3] == std::array{4, 4, 4}) << "Fourth v3s should be {4,4,4}\n";
   };

   "basic_rowwise_non_null_buffer"_test = [&] {
      std::string csv_data =
         R"(num1,11,33,55,77
num2,22,44,66,88
maybe,1,1,0,0
v3s[0],1,2,3,4
v3s[1],1,2,3,4
v3s[2],1,2,3,4)";

      auto buffer = make_buffer(csv_data);
      my_struct obj{};

      expect(!glz::read_csv(obj, buffer)) << "Should parse rowwise non-null-terminated buffer\n";

      expect(obj.num1[0] == 11) << "First num1 value should be 11\n";
      expect(obj.num2[2] == 66) << "Third num2 value should be 66\n";
      expect(obj.maybe[3] == false) << "Fourth maybe value should be false\n";
      expect(obj.v3s[0][2] == 1) << "v3s[0][2] should be 1\n";
   };

   "string_fields_non_null_buffer"_test = [&] {
      std::string csv_data =
         R"(id,udl
1,BRN
2,STR
3,ASD
4,USN)";

      auto buffer = make_buffer(csv_data);
      string_elements obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer))
         << "Should parse string fields from non-null-terminated buffer\n";

      expect(obj.id[0] == 1) << "First ID should be 1\n";
      expect(obj.id[3] == 4) << "Fourth ID should be 4\n";
      expect(obj.udl[0] == "BRN") << "First UDL should be 'BRN'\n";
      expect(obj.udl[3] == "USN") << "Fourth UDL should be 'USN'\n";
   };

   "quoted_strings_non_null_buffer"_test = [&] {
      std::string csv_data =
         R"(id,name,description
1,"John Doe","Software Engineer"
2,"Jane Smith","Product Manager"
3,"Bob ""Bobby"" Jones","Has quotes in name")";

      auto buffer = make_buffer(csv_data);

      person_data obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer))
         << "Should parse quoted strings from non-null-terminated buffer\n";

      expect(obj.id.size() == 3) << "Should have 3 records\n";
      expect(obj.name[0] == "John Doe") << "First name should be 'John Doe'\n";
      expect(obj.name[1] == "Jane Smith") << "Second name should be 'Jane Smith'\n";
      expect(obj.name[2] == "Bob \"Bobby\" Jones") << "Third name should handle escaped quotes\n";
      expect(obj.description[0] == "Software Engineer") << "First description should be 'Software Engineer'\n";
   };

   "carriage_return_non_null_buffer"_test = [&] {
      std::string csv_data = "num1,num2\r\n11,22\r\n33,44\r\n55,66";

      auto buffer = make_buffer(csv_data);

      simple_data obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer)) << "Should handle CRLF in non-null-terminated buffer\n";

      expect(obj.num1.size() == 3) << "Should have 3 num1 values\n";
      expect(obj.num2.size() == 3) << "Should have 3 num2 values\n";
      expect(obj.num1[0] == 11 && obj.num2[0] == 22) << "First row should be 11,22\n";
      expect(obj.num1[2] == 55 && obj.num2[2] == 66) << "Third row should be 55,66\n";
   };

   "map_colwise_non_null_buffer"_test = [&] {
      std::string csv_data =
         R"(x,y
0,1
1,2
2,3
3,4)";

      auto buffer = make_buffer(csv_data);
      std::map<std::string, std::vector<uint64_t>> m;

      expect(!glz::read_csv<glz::colwise>(m, buffer)) << "Should parse map from non-null-terminated buffer\n";

      expect(m["x"].size() == 4) << "Should have 4 x values\n";
      expect(m["y"].size() == 4) << "Should have 4 y values\n";
      expect(m["x"][0] == 0 && m["y"][0] == 1) << "First row should be x=0, y=1\n";
      expect(m["x"][3] == 3 && m["y"][3] == 4) << "Fourth row should be x=3, y=4\n";
   };

   "vector_of_structs_non_null_buffer"_test = [&] {
      std::string csv_data =
         R"(id,value,name
1,10.5,Point A
2,20.3,Point B
3,15.7,Point C)";

      auto buffer = make_buffer(csv_data);
      std::vector<data_point> data{};

      expect(!glz::read_csv<glz::colwise>(data, buffer))
         << "Should parse vector of structs from non-null-terminated buffer\n";

      expect(data.size() == 3) << "Should have 3 data points\n";
      expect(data[0].id == 1 && data[0].value == 10.5f && data[0].name == "Point A")
         << "First point should be correct\n";
      expect(data[2].id == 3 && data[2].value == 15.7f && data[2].name == "Point C")
         << "Third point should be correct\n";
   };

   "empty_fields_non_null_buffer"_test = [&] {
      std::string csv_data =
         R"(id,name,value
1,,10.5
2,Test,
3,"",15.7)";

      auto buffer = make_buffer(csv_data);

      test_data obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer)) << "Should handle empty fields in non-null-terminated buffer\n";

      expect(obj.id.size() == 3) << "Should have 3 records\n";
      expect(obj.name[0] == "") << "First name should be empty\n";
      expect(obj.name[1] == "Test") << "Second name should be 'Test'\n";
      expect(obj.value[1] == "") << "Second value should be empty\n";
   };

   "truncated_buffer_error"_test = [&] {
      std::string csv_data = "num1,num2\n11,22\n33,44\n55"; // Truncated - missing last value

      auto buffer = make_buffer(csv_data);

      simple_data obj{};

      expect(bool(glz::read_csv<glz::colwise>(obj, buffer)));
   };

   "single_character_buffer"_test = [&] {
      std::vector<char> buffer = {'a'};

      field_data obj{};

      expect(bool(glz::read_csv<glz::colwise>(obj, buffer)));
   };

   "empty_buffer"_test = [&] {
      std::vector<char> buffer{};

      num_data obj{};

      expect(bool(glz::read_csv<glz::colwise>(obj, buffer)));
   };

   "unicode_in_non_null_buffer"_test = [&] {
      std::string csv_data = "field1,field2\n简体汉字,😄\n漢字,💔";

      auto buffer = make_buffer(csv_data);

      unicode_data obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer)) << "Should handle Unicode in non-null-terminated buffer\n";

      expect(obj.field1.size() == 2) << "Should have 2 field1 values\n";
      expect(obj.field2.size() == 2) << "Should have 2 field2 values\n";
      expect(obj.field1[0] == "简体汉字") << "First field1 should be Chinese characters\n";
      expect(obj.field2[0] == "😄") << "First field2 should be emoji\n";
   };

   "large_buffer_stress_test"_test = [&] {
      // Create a large CSV with many rows to test buffer handling
      std::string csv_data = "id,value\n";
      for (int i = 0; i < 1000; ++i) {
         csv_data += std::to_string(i) + "," + std::to_string(i * 2) + "\n";
      }

      auto buffer = make_buffer(csv_data);

      large_data obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer)) << "Should handle large non-null-terminated buffer\n";

      expect(obj.id.size() == 1000) << "Should have 1000 ID values\n";
      expect(obj.value.size() == 1000) << "Should have 1000 value values\n";
      expect(obj.id[999] == 999) << "Last ID should be 999\n";
      expect(obj.value[999] == 1998) << "Last value should be 1998\n";
   };

   "mixed_line_endings_non_null_buffer"_test = [&] {
      // Mix of \n and \r\n line endings
      std::string csv_data = "a,b\n1,2\r\n3,4\n5,6\r\n";

      auto buffer = make_buffer(csv_data);

      mixed_data obj{};

      expect(!glz::read_csv<glz::colwise>(obj, buffer))
         << "Should handle mixed line endings in non-null-terminated buffer\n";

      expect(obj.a.size() == 3) << "Should have 3 'a' values\n";
      expect(obj.b.size() == 3) << "Should have 3 'b' values\n";
      expect(obj.a[0] == 1 && obj.b[0] == 2) << "First row should be 1,2\n";
      expect(obj.a[2] == 5 && obj.b[2] == 6) << "Third row should be 5,6\n";
   };
};

struct KeyframeData
{
   std::vector<int> start_ind;
   std::vector<int> duration;
   std::vector<int> delay;
   std::vector<std::string> renderHandle;
   std::vector<int> renderArgument_1;
   std::vector<int> renderArgument_2;
};

suite odd_csv_test = [] {
   "odd_string"_test = [] {
      KeyframeData obj{};
      std::string buffer{};
      std::string csv_data = R"(start_ind,duration,delay,renderHandle,renderArgument_1,renderArgument_2
0,400,30,gray::SimplePushPull,0,0
400,250,40,gray::SimplePushPull,1,0
650,300,80,gray::SimplePushPull,2,0)";
      auto ec = glz::read<glz::opts_csv{.layout = glz::colwise}>(obj, csv_data);
      expect(not ec) << glz::format_error(ec, buffer) << '\n';
      // this passed.
   };
   "odd_files"_test = [] {
      KeyframeData obj{};
      std::string buffer{};
      auto ec = glz::read_file_csv<glz::colwise>(obj, GLZ_TEST_DIRECTORY "/kf-data.csv", buffer);
      expect(not ec) << glz::format_error(ec, buffer) << '\n';
   };
};

struct overflow_struct
{
   std::vector<int> num1{};
   std::deque<float> num2{};
   std::vector<bool> maybe{};
   std::vector<std::array<int, 3>> v3s{};
};

suite fuzzfailures = [] {
   "fuzz1"_test = [] {
      std::string_view csv_data{"6  [5\n0"};

      overflow_struct obj{};
      [[maybe_unused]] auto parsed = glz::read_csv<glz::colwise>(obj, csv_data);
   };

   "fuzz2"_test = [] {
      std::string_view b64 =
         "IBCPAAoxMDY3ODg4NDUyMTMyMTA4Njk5NmUrMTEzNzI0NzEyMDQ5NDIzLjE0NTIxNTJCMzIxMDg2OTk2ZS05MTEKMzIANLaztqfgDQ==";

      auto input = glz::read_base64(b64);
      std::vector<uint8_t> s;
      s.resize(input.size());
      std::memcpy(s.data(), input.data(), s.size());
      my_struct obj;
      expect(glz::read_csv<glz::colwise>(obj, s));
   };

   "fuzz3"_test = [] {
      std::string_view b64 =
         "/BAACjY0OQo0OTk5OTk5MjkwMDAwODQ4MzY1M0UrMDAyNDk5OTk5OTk5Nwo5NAo5NDQ0NDQ0NDQ0NDQ0Cjk0CjkyCjYyAAAAAAA4OA==";

      auto input = glz::read_base64(b64);
      std::vector<uint8_t> s;
      s.resize(input.size());
      std::memcpy(s.data(), input.data(), s.size());
      my_struct obj;
      expect(glz::read_csv<glz::colwise>(obj, s));
   };

   "fuzz4"_test = [] {
      std::string_view b64 = "MCAgWzQsNA==";
      const auto input = glz::read_base64(b64);
      std::vector<uint8_t> s(begin(input), end(input));
      my_struct obj;
      auto ec = glz::read_csv<glz::rowwise>(obj, s);
      expect(ec == glz::error_code::unknown_key);
   };
};

// 2D Array CSV Tests
suite csv_2d_array_tests = [] {
   // Basic 2D Array Operations
   "basic_2d_array_read_numeric"_test = [] {
      std::string csv_data = R"(1,2,3
4,5,6
7,8,9)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[0].size() == 3) << "First row should have 3 columns";
      expect(matrix[0] == std::vector<int>{1, 2, 3}) << "First row data";
      expect(matrix[1] == std::vector<int>{4, 5, 6}) << "Second row data";
      expect(matrix[2] == std::vector<int>{7, 8, 9}) << "Third row data";
   };

   "basic_2d_array_write_numeric"_test = [] {
      std::vector<std::vector<int>> matrix = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

      std::string output;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(matrix, output));

      expect(output == R"(1,2,3
4,5,6
7,8,9
)") << "Output should match expected format";
   };

   "2d_array_roundtrip_numeric"_test = [] {
      std::vector<std::vector<int>> original = {{10, 20}, {30, 40}, {50, 60}};

      std::string buffer;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(original, buffer));

      std::vector<std::vector<int>> result;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, buffer));

      expect(result.size() == original.size()) << "Same number of rows";
      expect(result == original) << "Roundtrip should preserve data";
   };

   "2d_array_float_values"_test = [] {
      std::vector<std::vector<float>> matrix = {{1.5f, 2.7f}, {3.14f, 4.0f}};

      std::string buffer;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(matrix, buffer));

      std::vector<std::vector<float>> result;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, buffer));

      expect(result.size() == 2) << "Should have 2 rows";
      expect(result[0][0] == 1.5f) << "First element should be 1.5";
      expect(result[0][1] == 2.7f) << "Second element should be 2.7";
      expect(result[1][0] == 3.14f) << "Third element should be 3.14";
   };

   // Edge Cases
   "2d_array_empty_csv"_test = [] {
      std::string csv_data = "";

      std::vector<std::vector<int>> matrix;
      auto ec = glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data);
      expect(bool(ec)) << "Should fail on empty CSV";
   };

   "2d_array_single_value"_test = [] {
      std::string csv_data = "42";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 1) << "Should have 1 row";
      expect(matrix[0].size() == 1) << "Should have 1 column";
      expect(matrix[0][0] == 42) << "Should contain the single value";
   };

   "2d_array_irregular_rows_no_validation"_test = [] {
      std::string csv_data = R"(1,2,3
4,5
6,7,8,9)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[0].size() == 3) << "First row: 3 columns";
      expect(matrix[1].size() == 2) << "Second row: 2 columns";
      expect(matrix[2].size() == 4) << "Third row: 4 columns";
   };

   "2d_array_irregular_rows_with_validation"_test = [] {
      std::string csv_data = R"(1,2,3
4,5
6,7,8)";

      std::vector<std::vector<int>> matrix;
      auto ec = glz::read<glz::opts_csv{.use_headers = false, .validate_rectangular = true}>(matrix, csv_data);

      expect(bool(ec)) << "Should fail validation";
      expect(ec == glz::error_code::constraint_violated) << "Should be rectangular constraint violation";
   };

   "2d_array_trailing_comma"_test = [] {
      std::string csv_data = R"(1,2,3,
4,5,6,
7,8,9,)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[0].size() == 4) << "Should have 4 columns (including empty)";
      expect(matrix[0][3] == 0) << "Trailing comma should create empty field (parsed as 0)";
   };

   // String and Special Characters
   "2d_array_strings"_test = [] {
      std::string csv_data = R"(hello,world
foo,bar
test,data)";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[0] == std::vector<std::string>{"hello", "world"}) << "First row";
      expect(matrix[1] == std::vector<std::string>{"foo", "bar"}) << "Second row";
      expect(matrix[2] == std::vector<std::string>{"test", "data"}) << "Third row";
   };

   "2d_array_quoted_strings"_test = [] {
      std::string csv_data = R"("hello, world","simple"
"quoted ""text""","normal")";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0] == "hello, world") << "Should preserve comma in quoted field";
      expect(matrix[0][1] == "simple") << "Normal field";
      expect(matrix[1][0] == "quoted \"text\"") << "Should handle escaped quotes";
   };

   "2d_array_strings_with_commas"_test = [] {
      std::string csv_data = R"("Smith, John","Engineer"
"Doe, Jane","Manager")";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0] == "Smith, John") << "Should preserve comma in name";
      expect(matrix[1][0] == "Doe, Jane") << "Should preserve comma in name";
   };

   "2d_array_multiline_strings"_test = [] {
      std::string csv_data = R"("Line 1
Line 2","Simple"
"Another
Multi-line","Text")";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0] == "Line 1\nLine 2") << "Should preserve newlines in quoted field";
      expect(matrix[1][0] == "Another\nMulti-line") << "Should preserve newlines in quoted field";
   };

   // Configuration Options Tests
   "2d_array_skip_header_row"_test = [] {
      std::string csv_data = R"(col1,col2,col3
1,2,3
4,5,6
7,8,9)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false, .skip_header_row = true}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows (header skipped)";
      expect(matrix[0] == std::vector<int>{1, 2, 3}) << "First data row";
      expect(matrix[1] == std::vector<int>{4, 5, 6}) << "Second data row";
      expect(matrix[2] == std::vector<int>{7, 8, 9}) << "Third data row";
   };

   "2d_array_skip_header_row_strings"_test = [] {
      std::string csv_data = R"(Name,Role,Department
Alice,Engineer,Tech
Bob,Manager,Sales)";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false, .skip_header_row = true}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows (header skipped)";
      expect(matrix[0] == std::vector<std::string>{"Alice", "Engineer", "Tech"}) << "First data row";
      expect(matrix[1] == std::vector<std::string>{"Bob", "Manager", "Sales"}) << "Second data row";
   };

   "2d_array_validate_rectangular_success"_test = [] {
      std::string csv_data = R"(1,2,3
4,5,6
7,8,9)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false, .validate_rectangular = true}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[0].size() == 3) << "All rows should have 3 columns";
      expect(matrix[1].size() == 3) << "All rows should have 3 columns";
      expect(matrix[2].size() == 3) << "All rows should have 3 columns";
   };

   "2d_array_validate_rectangular_failure"_test = [] {
      std::string csv_data = R"(1,2,3
4,5
6,7,8,9)";

      std::vector<std::vector<int>> matrix;
      auto ec = glz::read<glz::opts_csv{.use_headers = false, .validate_rectangular = true}>(matrix, csv_data);

      expect(bool(ec)) << "Should fail validation";
      expect(ec == glz::error_code::constraint_violated) << "Should be rectangular constraint violation";
   };

   "2d_array_combined_options"_test = [] {
      std::string csv_data = R"(col1,col2,col3
1,2,3
4,5,6
7,8,9)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false, .skip_header_row = true, .validate_rectangular = true}>(
         matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows after skipping header";
      expect(matrix[0].size() == 3) << "All rows should have same column count";
   };

   // Mixed Container Types
   "2d_array_vector_array_mixed"_test = [] {
      std::string csv_data = R"(1,2,3
4,5,6)";

      std::vector<std::array<int, 3>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0] == std::array<int, 3>{1, 2, 3}) << "First row data";
      expect(matrix[1] == std::array<int, 3>{4, 5, 6}) << "Second row data";
   };

   "2d_array_array_vector_mixed"_test = [] {
      std::string csv_data = R"(1,2
3,4
5,6)";

      // Array of vectors is not supported for CSV reading (outer container must be resizable)
      // This test is commented out as std::array is not resizable
      // std::array<std::vector<int>, 3> matrix;
      // The CSV reader requires the outer container to be resizable (e.g., std::vector)
   };

   // Column-wise Layout Tests
   "2d_array_column_wise_read"_test = [] {
      // CSV data in row format: each row represents sequential data
      std::string csv_data = R"(1,2,3
4,5,6
7,8,9)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(matrix, csv_data));

      // With column-wise reading, the data should be transposed
      // Original row 1: [1,2,3] becomes column 1: [1,4,7]
      expect(matrix.size() == 3) << "Should have 3 columns (was 3 rows)";
      expect(matrix[0] == std::vector<int>{1, 4, 7}) << "First column";
      expect(matrix[1] == std::vector<int>{2, 5, 8}) << "Second column";
      expect(matrix[2] == std::vector<int>{3, 6, 9}) << "Third column";
   };

   "2d_array_column_wise_write"_test = [] {
      // Data organized by columns
      std::vector<std::vector<int>> matrix = {
         {1, 4, 7}, // Column 1
         {2, 5, 8}, // Column 2
         {3, 6, 9} // Column 3
      };

      std::string buffer;
      expect(!glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(matrix, buffer));

      // Column-wise write should transpose: columns become rows
      expect(buffer == "1,2,3\n4,5,6\n7,8,9\n") << "Column-wise write should transpose";
   };

   "2d_array_column_wise_roundtrip"_test = [] {
      // Original data in column format
      std::vector<std::vector<double>> original = {
         {1.1, 2.2, 3.3}, // Column 1
         {4.4, 5.5, 6.6}, // Column 2
         {7.7, 8.8, 9.9} // Column 3
      };

      std::string buffer;
      expect(!glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(original, buffer));

      std::vector<std::vector<double>> result;
      expect(!glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(result, buffer));

      expect(result.size() == original.size()) << "Same number of columns";
      for (size_t i = 0; i < original.size(); ++i) {
         expect(result[i] == original[i]) << "Column " << i << " should match";
      }
   };

   "2d_array_column_wise_non_square"_test = [] {
      // Non-square matrix (2x4)
      std::string csv_data = R"(1,2,3,4
5,6,7,8)";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(matrix, csv_data));

      // After transpose: 4 columns with 2 elements each
      expect(matrix.size() == 4) << "Should have 4 columns";
      expect(matrix[0] == std::vector<int>{1, 5}) << "First column";
      expect(matrix[1] == std::vector<int>{2, 6}) << "Second column";
      expect(matrix[2] == std::vector<int>{3, 7}) << "Third column";
      expect(matrix[3] == std::vector<int>{4, 8}) << "Fourth column";
   };

   "2d_array_column_wise_string_data"_test = [] {
      std::string csv_data = R"("a","b","c"
"d","e","f")";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 columns";
      expect(matrix[0] == std::vector<std::string>{"a", "d"}) << "First column";
      expect(matrix[1] == std::vector<std::string>{"b", "e"}) << "Second column";
      expect(matrix[2] == std::vector<std::string>{"c", "f"}) << "Third column";
   };

   "2d_array_row_vs_column_comparison"_test = [] {
      std::string csv_data = R"(1,2,3
4,5,6)";

      std::vector<std::vector<int>> row_wise;
      std::vector<std::vector<int>> col_wise;

      expect(!glz::read<glz::opts_csv{.layout = glz::rowwise, .use_headers = false}>(row_wise, csv_data));
      expect(!glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(col_wise, csv_data));

      // Row-wise: 2x3 matrix
      expect(row_wise.size() == 2) << "Row-wise should have 2 rows";
      expect(row_wise[0].size() == 3) << "Each row should have 3 elements";

      // Column-wise: 3x2 matrix (transposed)
      expect(col_wise.size() == 3) << "Column-wise should have 3 columns";
      expect(col_wise[0].size() == 2) << "Each column should have 2 elements";

      // Verify transpose relationship
      for (size_t i = 0; i < row_wise.size(); ++i) {
         for (size_t j = 0; j < row_wise[i].size(); ++j) {
            expect(row_wise[i][j] == col_wise[j][i])
               << "Element at [" << i << "][" << j << "] should match transposed position";
         }
      }
   };

   // Performance and Large Dataset Tests
   "2d_array_large_dataset"_test = [] {
      // Create a large matrix (100x50)
      std::vector<std::vector<int>> original;
      for (int i = 0; i < 100; ++i) {
         std::vector<int> row;
         for (int j = 0; j < 50; ++j) {
            row.push_back(i * 50 + j);
         }
         original.push_back(row);
      }

      std::string buffer;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(original, buffer));

      std::vector<std::vector<int>> result;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(result, buffer));

      expect(result.size() == 100) << "Should have 100 rows";
      expect(result[0].size() == 50) << "Should have 50 columns";
      expect(result[99][49] == 4999) << "Last element should be correct";
   };

   // Error Handling Tests
   "2d_array_malformed_csv"_test = [] {
      std::string csv_data = R"(1,2,3
4,5,6
7,8,)"; // Last field is empty but should parse as 0 for integers

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[2][2] == 0) << "Empty field should parse as 0";
   };

   "2d_array_invalid_data_type"_test = [] {
      std::string csv_data = R"(1,2,3
4,invalid,6
7,8,9)";

      std::vector<std::vector<int>> matrix;
      auto ec = glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data);
      expect(bool(ec)) << "Should fail on invalid integer";
   };

   "2d_array_carriage_return_handling"_test = [] {
      std::string csv_data = "1,2,3\r\n4,5,6\r\n7,8,9";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should handle CRLF line endings";
      expect(matrix[0] == std::vector<int>{1, 2, 3}) << "First row";
      expect(matrix[2] == std::vector<int>{7, 8, 9}) << "Last row";
   };

   "2d_array_mixed_line_endings"_test = [] {
      std::string csv_data = "1,2,3\n4,5,6\r\n7,8,9\n";

      std::vector<std::vector<int>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should handle mixed line endings";
   };

   // Memory Management Tests
   "2d_array_memory_efficiency"_test = [] {
      // Test that containers are properly resized and not over-allocated
      std::vector<std::vector<int>> data = {{1, 2}, {3, 4}};

      std::string buffer;
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(data, buffer));

      std::vector<std::vector<int>> data2 = {{100, 200, 300}, {400, 500, 600}};
      expect(!glz::write<glz::opts_csv{.use_headers = false}>(data2, buffer));

      expect(buffer.find("100,200,300") != std::string::npos) << "Should contain second dataset";
   };

   // Append Arrays Tests
   "2d_array_append_arrays"_test = [] {
      std::vector<std::vector<int>> initial = {{1, 2}, {3, 4}};

      std::string csv_data = R"(5,6
7,8)";

      struct no_headers_append_opts : glz::opts_csv
      {
         bool use_headers = false;
         bool append_arrays = true;
      };
      expect(!glz::read<no_headers_append_opts{}>(initial, csv_data));

      expect(initial.size() == 4) << "Should have 4 rows after append";
      expect(initial[0] == std::vector<int>{1, 2}) << "Original first row preserved";
      expect(initial[2] == std::vector<int>{5, 6}) << "Appended first row";
      expect(initial[3] == std::vector<int>{7, 8}) << "Appended second row";
   };
};

// Additional edge case tests for comprehensive coverage
suite csv_2d_array_edge_cases = [] {
   "2d_array_empty_fields"_test = [] {
      std::string csv_data = R"(1,,3
,5,
7,8,)";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 3) << "Should have 3 rows";
      expect(matrix[0][1] == "") << "Empty field should be empty string";
      expect(matrix[1][0] == "") << "Empty field should be empty string";
      expect(matrix[2][2] == "") << "Empty field should be empty string";
   };

   "2d_array_whitespace_handling"_test = [] {
      std::string csv_data = R"( 1 , 2 , 3 
 4 , 5 , 6 )";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0] == " 1 ") << "Should preserve whitespace";
      expect(matrix[0][1] == " 2 ") << "Should preserve whitespace";
   };

   "2d_array_unicode_content"_test = [] {
      std::string csv_data = R"(简体汉字,😄,Test
漢字,💔,Data)";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0] == "简体汉字") << "Should handle Chinese characters";
      expect(matrix[0][1] == "😄") << "Should handle emojis";
      expect(matrix[1][1] == "💔") << "Should handle emojis";
   };

   "2d_array_very_long_fields"_test = [] {
      std::string long_text(1000, 'A');
      std::string csv_data = R"(")" + long_text + R"(",short
normal,")" + long_text + R"(")";

      std::vector<std::vector<std::string>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0].length() == 1000) << "Should handle very long fields";
      expect(matrix[1][1].length() == 1000) << "Should handle very long fields";
   };

   "2d_array_boolean_values"_test = [] {
      std::string csv_data = R"(true,false,1
0,true,false)";

      std::vector<std::vector<bool>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0] == true) << "Should parse 'true' as boolean";
      expect(matrix[0][1] == false) << "Should parse 'false' as boolean";
      expect(matrix[1][0] == false) << "Should parse '0' as false";
   };

   "2d_array_scientific_notation"_test = [] {
      std::string csv_data = R"(1.23e4,5.67e-2
9.87E+3,1.0E-5)";

      std::vector<std::vector<double>> matrix;
      expect(!glz::read<glz::opts_csv{.use_headers = false}>(matrix, csv_data));

      expect(matrix.size() == 2) << "Should have 2 rows";
      expect(matrix[0][0] == 12300.0) << "Should parse scientific notation";
      expect(matrix[0][1] == 0.0567) << "Should parse negative exponent";
   };
};

int main() { return 0; }
