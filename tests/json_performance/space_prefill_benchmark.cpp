#include <iostream>
#include <string>
#include <vector>
#include "glaze/glaze.hpp"
#include "bencher/bencher.hpp"

// We use a simple struct for benchmarking
struct MyStruct {
    int id;
    std::string name;
    std::vector<double> values;
    struct Nested {
        bool flag;
        std::string desc;
    } nested;
};

template <>
struct glz::meta<MyStruct::Nested> {
    using T = MyStruct::Nested;
    static constexpr auto value = object(
        "flag", &T::flag,
        "desc", &T::desc
    );
};

template <>
struct glz::meta<MyStruct> {
    using T = MyStruct;
    static constexpr auto value = object(
        "id", &T::id,
        "name", &T::name,
        "values", &T::values,
        "nested", &T::nested
    );
};

int main() {
    MyStruct obj{1, "test", {1.1, 2.2, 3.3}, {true, "nested"}};
    std::vector<MyStruct> data(1000, obj);

    std::string buffer;

    bencher::benchmark(100, 1, [&](){ // 100 iterations
        buffer.clear();
        glz::write<glz::opts{.prettify = true}>(data, buffer);
    });

    return 0;
}
