// Benchmark for glz::lazy_json parsing patterns

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"

#include "glaze/json.hpp"

#include <string>

// Struct for deserialization benchmarks
struct BenchUser {
   int64_t id{};
   std::string name{};
   std::string email{};
   int64_t age{};
   bool active{};
   int64_t score{};
};

// Test JSON data
inline const std::string small_json = R"({"name":"John Doe","age":30,"active":true,"balance":12345.67})";

inline const std::string nested_json = R"({
   "user": {
      "id": 12345,
      "profile": {
         "name": "Alice Smith",
         "email": "alice@example.com",
         "verified": true
      },
      "settings": {
         "theme": "dark",
         "notifications": true
      }
   },
   "timestamp": 1699999999
})";

inline const std::string array_json = R"({
   "items": [
      {"id": 1, "value": 100},
      {"id": 2, "value": 200},
      {"id": 3, "value": 300},
      {"id": 4, "value": 400},
      {"id": 5, "value": 500},
      {"id": 6, "value": 600},
      {"id": 7, "value": 700},
      {"id": 8, "value": 800},
      {"id": 9, "value": 900},
      {"id": 10, "value": 1000}
   ],
   "total": 10
})";

// Generate a larger JSON for throughput testing
inline std::string generate_large_json(size_t count)
{
   std::string json = R"({"users":[)";
   for (size_t i = 0; i < count; ++i) {
      if (i > 0) json += ",";
      json += R"({"id":)";
      json += std::to_string(i);
      json += R"(,"name":"User )";
      json += std::to_string(i);
      json += R"(","email":"user)";
      json += std::to_string(i);
      json += R"(@test.com","age":)";
      json += std::to_string(20 + (i % 50));
      json += R"(,"active":)";
      json += (i % 2 == 0) ? "true" : "false";
      json += R"(,"score":)";
      json += std::to_string(i * 10);
      json += "}";
   }
   json += R"(],"count":)";
   json += std::to_string(count);
   json += "}";
   return json;
}

// Generate JSON with whitespace (pretty-printed style)
inline std::string generate_large_json_with_whitespace(size_t count)
{
   std::string json = "{\n  \"users\": [\n";
   for (size_t i = 0; i < count; ++i) {
      if (i > 0) json += ",\n";
      json += "    { \"id\": ";
      json += std::to_string(i);
      json += ", \"name\": \"User ";
      json += std::to_string(i);
      json += "\", \"email\": \"user";
      json += std::to_string(i);
      json += "@test.com\", \"age\": ";
      json += std::to_string(20 + (i % 50));
      json += ", \"active\": ";
      json += (i % 2 == 0) ? "true" : "false";
      json += ", \"score\": ";
      json += std::to_string(i * 10);
      json += " }";
   }
   json += "\n  ],\n  \"count\": ";
   json += std::to_string(count);
   json += "\n}\n";
   return json;
}

int main()
{
   const auto large_json = generate_large_json(1000);

   // ========================================================================
   // Benchmark: Parse only (no field access)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Parse Only (no field access)";

      stage.run("glz::lazy_json (small)", [&] {
         auto result = glz::lazy_json(small_json);
         if (!result) std::abort();
         return small_json.size();
      });

      stage.run("glz::lazy_json (nested)", [&] {
         auto result = glz::lazy_json(nested_json);
         if (!result) std::abort();
         return nested_json.size();
      });

      stage.run("glz::lazy_json (large)", [&] {
         auto result = glz::lazy_json(large_json);
         if (!result) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Single field access
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Single Field Access";

      stage.run("glz::lazy_json", [&] {
         auto result = glz::lazy_json(small_json);
         auto name = (*result)["name"].get<std::string_view>();
         if (!name || name.value() != "John Doe") std::abort();
         return small_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Nested field access
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Nested Field Access (3 levels deep)";

      stage.run("glz::lazy_json", [&] {
         auto result = glz::lazy_json(nested_json);
         auto email = (*result)["user"]["profile"]["email"].get<std::string_view>();
         if (!email || email.value() != "alice@example.com") std::abort();
         return nested_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Multiple field access
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Multiple Field Access (4 fields)";

      stage.run("glz::lazy_json", [&] {
         auto result = glz::lazy_json(small_json);
         auto& doc = *result;
         auto name = doc["name"].get<std::string_view>();
         auto age = doc["age"].get<int64_t>();
         auto active = doc["active"].get<bool>();
         auto balance = doc["balance"].get<double>();
         if (!name || !age || !active || !balance) std::abort();
         return small_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Array iteration with field access (using iterator)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Array Iteration (10 elements, sum values)";

      stage.run("glz::lazy_json (iterator)", [&] {
         auto result = glz::lazy_json(array_json);
         auto items = (*result)["items"];
         int64_t sum = 0;
         for (auto& item : items) {  // auto& enables parse_pos_ optimization
            auto val = item["value"].get<int64_t>();
            if (val) sum += *val;
         }
         if (sum != 5500) std::abort();
         return array_json.size();
      });

      stage.run("glz::lazy_json (indexed)", [&] {
         auto result = glz::lazy_json(array_json);
         auto items = (*result)["items"].index();  // Build index once
         int64_t sum = 0;
         for (auto& item : items) {  // O(1) iteration
            auto val = item["value"].get<int64_t>();
            if (val) sum += *val;
         }
         if (sum != 5500) std::abort();
         return array_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Large JSON - access first and last elements
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Large JSON (1000 users) - First/Last Access";

      stage.run("glz::lazy_json", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         auto first_id = users[0]["id"].get<int64_t>();
         if (!first_id || *first_id != 0) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Large JSON - iterate all and sum (using iterator)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Large JSON (1000 users) - Iterate All & Sum Scores";

      stage.run("glz::lazy_json (iterator)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         for (auto& user : users) {  // auto& enables parse_pos_ optimization
            auto score = user["score"].get<int64_t>();
            if (score) sum += *score;
         }
         // Sum of 0*10 + 1*10 + ... + 999*10 = 10 * (999*1000/2) = 4995000
         if (sum != 4995000) std::abort();
         return large_json.size();
      });

      stage.run("glz::lazy_json (indexed)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"].index();  // Build index once - O(n)
         int64_t sum = 0;
         for (auto& user : users) {  // O(1) iteration
            auto score = user["score"].get<int64_t>();
            if (score) sum += *score;
         }
         if (sum != 4995000) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Large JSON - indexed random access
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Large JSON (1000 users) - Random Access (element 500)";

      stage.run("glz::lazy_json (no index)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         auto id = users[500]["id"].get<int64_t>();
         if (!id || *id != 500) std::abort();
         return large_json.size();
      });

      stage.run("glz::lazy_json (indexed)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"].index();
         auto id = users[500]["id"].get<int64_t>();
         if (!id || *id != 500) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Multiple random accesses (where indexing helps)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Large JSON - 10 Random Accesses (indexed amortizes)";

      stage.run("glz::lazy_json (no index)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         // 10 random accesses - each must scan from beginning
         for (int i = 0; i < 10; ++i) {
            auto id = users[i * 100]["id"].get<int64_t>();
            if (id) sum += *id;
         }
         if (sum != 4500) std::abort();  // 0+100+200+...+900 = 4500
         return large_json.size();
      });

      stage.run("glz::lazy_json (indexed)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"].index();  // Build index once
         int64_t sum = 0;
         // 10 random accesses - O(1) each after index built
         for (int i = 0; i < 10; ++i) {
            auto id = users[i * 100]["id"].get<int64_t>();
            if (id) sum += *id;
         }
         if (sum != 4500) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Minified option performance (minified JSON)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Minified JSON - default vs minified option";

      stage.run("glz::lazy_json (default)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         for (auto& user : users) {
            auto score = user["score"].get<int64_t>();
            if (score) sum += *score;
         }
         if (sum != 4995000) std::abort();
         return large_json.size();
      });

      stage.run("glz::lazy_json (minified=true)", [&] {
         constexpr auto opts = glz::opts{.minified = true};
         auto result = glz::lazy_json<opts>(large_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         for (auto& user : users) {
            auto score = user["score"].get<int64_t>();
            if (score) sum += *score;
         }
         if (sum != 4995000) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Pretty-printed JSON (whitespace overhead)
   // ========================================================================
   {
      const auto pretty_json = generate_large_json_with_whitespace(1000);

      bencher::stage stage;
      stage.name = "Pretty-printed JSON (with whitespace)";

      stage.run("glz::lazy_json", [&] {
         auto result = glz::lazy_json(pretty_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         for (auto& user : users) {
            auto score = user["score"].get<int64_t>();
            if (score) sum += *score;
         }
         if (sum != 4995000) std::abort();
         return pretty_json.size();
      });

      // Compare with minified JSON throughput
      stage.run("glz::lazy_json (minified JSON)", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         for (auto& user : users) {
            auto score = user["score"].get<int64_t>();
            if (score) sum += *score;
         }
         if (sum != 4995000) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: read_into vs raw_json + read_json (single-pass efficiency)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Struct Deserialization: read_into vs raw_json+read_json";

      stage.run("raw_json() + read_json() [double-pass]", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         size_t count = 0;
         for (auto& user : users) {
            BenchUser u{};
            // raw_json() scans to find value end, then read_json parses again
            auto ec = glz::read_json(u, user.raw_json());
            if (!ec) {
               sum += u.score;
               ++count;
            }
         }
         if (sum != 4995000 || count != 1000) std::abort();
         return large_json.size();
      });

      stage.run("read_into() [single-pass]", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"];
         int64_t sum = 0;
         size_t count = 0;
         for (auto& user : users) {
            BenchUser u{};
            // read_into directly parses from value start to doc end - no pre-scan
            auto ec = user.read_into(u);
            if (!ec) {
               sum += u.score;
               ++count;
            }
         }
         if (sum != 4995000 || count != 1000) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: read_into with indexed view (random struct access)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Indexed Struct Access: raw_json+read_json vs read_into";

      stage.run("indexed + raw_json() + read_json()", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"].index();
         int64_t sum = 0;
         // Access 10 random elements
         for (int i = 0; i < 10; ++i) {
            BenchUser u{};
            auto ec = glz::read_json(u, users[i * 100].raw_json());
            if (!ec) sum += u.score;
         }
         // Scores: 0*10, 100*10, 200*10, ..., 900*10 = 10*(0+100+200+...+900) = 10*4500 = 45000
         if (sum != 45000) std::abort();
         return large_json.size();
      });

      stage.run("indexed + read_into()", [&] {
         auto result = glz::lazy_json(large_json);
         auto users = (*result)["users"].index();
         int64_t sum = 0;
         // Access 10 random elements
         for (int i = 0; i < 10; ++i) {
            BenchUser u{};
            auto ec = users[i * 100].read_into(u);
            if (!ec) sum += u.score;
         }
         if (sum != 45000) std::abort();
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   return 0;
}
