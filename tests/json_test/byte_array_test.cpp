#include "glaze/glaze.hpp"
#include "ut/ut.hpp"
#include <string_view>
#include <string>
#include <vector>

using namespace ut;

suite byte_array_tests = [] {
    "default_char_array_behavior"_test = [] {
        char arr[4] = {0, 0, 1, 0};
        std::string json;
        glz::write_json(arr, json);
        // arr is treated as null-terminated string, so it stops at first null.
        expect(json == "\"\"") << "Default char arrays stop at null";
    };

    "escape_bytes_wrapper"_test = [] {
        char arr[4] = {0, 0, 1, 0};
        std::string json;
        
        glz::write_json(glz::escape_bytes{arr}, json);
        
        expect(json == R"("\u0000\u0000\u0001\u0000")") << "escape_bytes wrapper serializes all bytes with escaping";
    };

    "escape_bytes_roundtrip"_test = [] {
        char original[4] = {0, 0, 1, 0};
        std::string json;
        glz::write_json(glz::escape_bytes{original}, json);
        
        char result[4] = {1, 1, 1, 1}; // Initialize with garbage
        
        auto wrapper = glz::escape_bytes{result};
        auto ec = glz::read_json(wrapper, json);
        expect(ec == glz::error_code::none) << "Wrapper read error: " << int(ec);
        
        expect(result[0] == 0);
        expect(result[1] == 0);
        expect(result[2] == 1);
        expect(result[3] == 0);
    };

    "escape_bytes_vector_roundtrip"_test = [] {
        std::vector<char> original = {0, 0, 1, 0};
        std::string json;
        // vector serialization usually produces array [0,0,1,0].
        // But with escape_bytes it should produce string "\u0000..."
        glz::write_json(glz::escape_bytes{original}, json);
        expect(json == R"("\u0000\u0000\u0001\u0000")") << "vector with wrapper serializes to string";
        
        std::vector<char> result;
        auto wrapper = glz::escape_bytes{result};
        expect(glz::read_json(wrapper, json) == glz::error_code::none);
        expect(result.size() == 4);
        expect(result == original);
    };
    
    "escape_bytes_mixed_content"_test = [] {
       char arr[4] = {'a', 0, 'b', '\n'};
       std::string json;
       glz::write_json(glz::escape_bytes{arr}, json);
       
       // "a\u0000b\n"
       expect(json == R"("a\u0000b\n")") << json;
       
       char result[4];
       auto wrapper = glz::escape_bytes{result};
       expect(glz::read_json(wrapper, json) == glz::error_code::none);
       expect(result[0] == 'a');
       expect(result[1] == 0);
       expect(result[2] == 'b');
       expect(result[3] == '\n');
    };
};

int main() {
    return 0;
}
