
#include <string>
#include <vector>
#include <iostream>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/glaze.hpp"

struct Object {
    int id;
    double value;
    std::string name;
    std::vector<int> data;
};

struct Root {
    std::string title;
    std::vector<Object> items;
};

// Generate a large structure
Root generate_data(int count) {
    Root root;
    root.title = "Prettify Benchmark";
    root.items.reserve(count);
    for (int i = 0; i < count; ++i) {
        Object obj;
        obj.id = i;
        obj.value = i * 1.234;
        obj.name = "Item " + std::to_string(i);
        obj.data = {i, i+1, i+2, i+3, i+4};
        root.items.push_back(std::move(obj));
    }
    return root;
}

int main() {
    const int item_count = 10000;
    const auto data = generate_data(item_count);

    bencher::stage stage;
    stage.name = "Prettify Write Benchmark";

    stage.run("write_json prettify=true", [&] {
        std::string buffer;
        glz::write<glz::opts{.prettify = true}>(data, buffer);
        return buffer.size();
    });

    stage.run("write_json prettify=false", [&] {
        std::string buffer;
        glz::write<glz::opts{.prettify = false}>(data, buffer);
        return buffer.size();
    });

    // Also benchmark reused buffer
    std::string reused_buffer;
    reused_buffer.reserve(1024 * 1024 * 5); // Reserve 5MB

    stage.run("write_json prettify=true (reused buffer)", [&] {
        reused_buffer.clear(); // Does not free memory
        glz::write<glz::opts{.prettify = true}>(data, reused_buffer);
        return reused_buffer.size();
    });

    bencher::print_results(stage);
    return 0;
}
