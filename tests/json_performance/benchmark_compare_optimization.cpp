#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <iomanip>

#include "glaze/glaze.hpp"
#include "simdjson/simdjson.h"

struct MyStruct {
    int id;
    double value;
    std::string name;
    bool active;
    std::vector<int> scores;
};

template <>
struct glz::meta<MyStruct> {
    using T = MyStruct;
    static constexpr auto value = object(
        "id", &T::id,
        "value", &T::value,
        "name", &T::name,
        "active", &T::active,
        "scores", &T::scores
    );
};

struct Root {
    std::vector<MyStruct> data;
};

template <>
struct glz::meta<Root> {
    using T = Root;
    static constexpr auto value = object("data", &T::data);
};

// Generate random data
Root generate_data(size_t count) {
    Root root;
    root.data.reserve(count);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> id_dist(1, 1000000);
    std::uniform_real_distribution<double> val_dist(0.0, 1000.0);
    std::uniform_int_distribution<int> bool_dist(0, 1);
    std::uniform_int_distribution<int> char_dist(32, 126);
    std::uniform_int_distribution<int> len_dist(5, 20);
    std::uniform_int_distribution<int> score_len_dist(0, 10);
    std::uniform_int_distribution<int> score_dist(0, 100);

    for (size_t i = 0; i < count; ++i) {
        MyStruct s;
        s.id = id_dist(rng);
        s.value = val_dist(rng);
        s.active = bool_dist(rng);

        int len = len_dist(rng);
        s.name.reserve(len);
        for (int j = 0; j < len; ++j) {
            s.name.push_back(static_cast<char>(char_dist(rng)));
        }

        int scores_len = score_len_dist(rng);
        s.scores.reserve(scores_len);
        for (int j = 0; j < scores_len; ++j) {
            s.scores.push_back(score_dist(rng));
        }
        root.data.push_back(s);
    }
    return root;
}

// Simdjson parsing logic
// Returns true on success
bool parse_simdjson(simdjson::ondemand::parser& parser, simdjson::padded_string& json, Root& root) {
    try {
        auto doc = parser.iterate(json);
        simdjson::ondemand::object obj = doc.get_object();
        // Root has "data" field
        for (auto field : obj) {
            if (field.key() == "data") {
                for (auto elem : field.value().get_array()) {
                    simdjson::ondemand::object s_obj = elem.get_object();
                    MyStruct s;
                    // Assuming fields come in order or we iterate through them
                    for (auto s_field : s_obj) {
                        std::string_view key = s_field.unescaped_key().value();
                        if (key == "id") { s.id = int64_t(s_field.value().get_int64()); }
                        else if (key == "value") { s.value = double(s_field.value().get_double()); }
                        else if (key == "name") { std::string_view sv = s_field.value().get_string().value(); s.name = sv; }
                        else if (key == "active") { s.active = bool(s_field.value().get_bool()); }
                        else if (key == "scores") {
                            for (auto score : s_field.value().get_array()) {
                                s.scores.push_back(int64_t(score.get_int64()));
                            }
                        }
                    }
                    root.data.push_back(std::move(s));
                }
            }
        }
        return true;
    } catch (simdjson::simdjson_error& e) {
        std::cerr << "Simdjson error: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    const size_t N_ITEMS = 100000;
    const size_t ITERATIONS = 50;

    std::cout << "Generating data..." << std::endl;
    Root original = generate_data(N_ITEMS);

    std::string json_str;
    glz::write<glz::opts{}>(original, json_str);

    std::cout << "JSON size: " << json_str.size() / (1024.0 * 1024.0) << " MB" << std::endl;

    // Glaze Benchmark
    {
        Root dest;
        // Warmup
        if (glz::read<glz::opts{}>(dest, json_str)) {}
        dest = {};

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ITERATIONS; ++i) {
            dest.data.clear();
            if (glz::read<glz::opts{}>(dest, json_str)) {
                std::cerr << "Glaze failed to read!" << std::endl;
                return 1;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        std::cout << "Glaze Read Time: " << duration << "s (" << (ITERATIONS * json_str.size() / (1024.0 * 1024.0)) / duration << " MB/s)" << std::endl;
    }

    // Simdjson Benchmark
    {
        simdjson::padded_string p_json(json_str);
        simdjson::ondemand::parser parser;
        Root dest;

        // Warmup
        parse_simdjson(parser, p_json, dest);
        dest = {};

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ITERATIONS; ++i) {
            dest.data.clear();
            if (!parse_simdjson(parser, p_json, dest)) {
                 std::cerr << "Simdjson failed to read!" << std::endl;
                 return 1;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();
        std::cout << "Simdjson Read Time: " << duration << "s (" << (ITERATIONS * json_str.size() / (1024.0 * 1024.0)) / duration << " MB/s)" << std::endl;
    }

    return 0;
}
