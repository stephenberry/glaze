#include <string>
#include <string_view>
#include <vector>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct BinaryData
{
   char data[4];
};

template <>
struct glz::meta<BinaryData>
{
   using T = BinaryData;
   static constexpr auto value = glz::object("data", glz::escape_bytes<&T::data>);
};

suite byte_array_tests = [] {
   "check_concepts"_test = [] {
      using Arr = char[4];
      using Wrapper = glz::escape_bytes_t<Arr>;
      // Check if from specialization is found
      static_assert(requires { glz::from<glz::JSON, Wrapper>{}; }, "from specialization not found");
      static_assert(glz::read_supported<Wrapper, glz::JSON>, "read_supported failed");
   };

   "default_char_array_behavior"_test = [] {
      char arr[4] = {0, 0, 1, 0};
      std::string json;
      expect(not glz::write_json(arr, json));
      // arr is treated as null-terminated string, so it stops at first null.
      expect(json == "\"\"") << "Default char arrays stop at null";
   };

   "escape_bytes_t_wrapper"_test = [] {
      char arr[4] = {0, 0, 1, 0};
      std::string json;

      expect(not glz::write_json(glz::escape_bytes_t{arr}, json));

      expect(json == R"("\u0000\u0000\u0001\u0000")") << "escape_bytes_t wrapper serializes all bytes with escaping";
   };

   "escape_bytes_t_roundtrip"_test = [] {
      char original[4] = {0, 0, 1, 0};
      std::string json;
      expect(not glz::write_json(glz::escape_bytes_t{original}, json));

      char result[4] = {1, 1, 1, 1}; // Initialize with garbage

      auto wrapper = glz::escape_bytes_t{result};
      auto ec = glz::read_json(wrapper, json);
      expect(ec == glz::error_code::none) << "Wrapper read error: " << int(ec);

      expect(result[0] == 0);
      expect(result[1] == 0);
      expect(result[2] == 1);
      expect(result[3] == 0);
   };

   "escape_bytes_t_vector_roundtrip"_test = [] {
      std::vector<char> original = {0, 0, 1, 0};
      std::string json;
      // vector serialization usually produces array [0,0,1,0].
      // But with escape_bytes it should produce string "\u0000..."
      expect(not glz::write_json(glz::escape_bytes_t{original}, json));
      expect(json == R"("\u0000\u0000\u0001\u0000")") << "vector with wrapper serializes to string";

      std::vector<char> result;
      auto wrapper = glz::escape_bytes_t{result};
      expect(glz::read_json(wrapper, json) == glz::error_code::none);
      expect(result.size() == 4);
      expect(result == original);
   };

   "escape_bytes_t_mixed_content"_test = [] {
      char arr[4] = {'a', 0, 'b', '\n'};
      std::string json;
      expect(not glz::write_json(glz::escape_bytes_t{arr}, json));

      // "a\u0000b\n"
      expect(json == R"("a\u0000b\n")") << json;

      char result[4];
      auto wrapper = glz::escape_bytes_t{result};
      expect(glz::read_json(wrapper, json) == glz::error_code::none);
      expect(result[0] == 'a');
      expect(result[1] == 0);
      expect(result[2] == 'b');
      expect(result[3] == '\n');
   };

   "escape_bytes_meta"_test = [] {
      BinaryData obj;
      obj.data[0] = 0;
      obj.data[1] = 1;
      obj.data[2] = 0;
      obj.data[3] = 2;

      std::string json;
      expect(not glz::write_json(obj, json));

      std::string expected = R"({"data":"\u0000\u0001\u0000\u0002"})";
      expect(json.size() == expected.size()) << "Size mismatch: " << json.size() << " vs " << expected.size();
      expect(json == expected) << "Meta wrapper works. Got: " << json << " Expected: " << expected;

      BinaryData obj2;
      expect(glz::read_json(obj2, json) == glz::error_code::none);
      expect(std::memcmp(obj.data, obj2.data, 4) == 0);
   };
};

int main() { return 0; }
