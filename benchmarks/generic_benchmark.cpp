// Benchmark: glz::generic (ordered_small_map) vs glz::generic with std::map
// Tests read_json, write_json, and key lookup performance

#include <map>
#include <string>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/json.hpp"

// User-defined std::map adapter for generic_json
template <class T>
using std_map = std::map<std::string, T, std::less<>>;

using generic_default = glz::generic; // ordered_small_map
using generic_std_map = glz::generic_json<glz::num_mode::f64, std_map>;

// Small JSON object (8 keys)
inline const std::string small_json = R"({
   "id": 12345,
   "name": "Alice Smith",
   "email": "alice@example.com",
   "age": 30,
   "active": true,
   "score": 98.5,
   "role": "admin",
   "verified": false
})";

// Medium JSON object (nested, ~20 keys total)
inline const std::string medium_json = R"({
   "user": {
      "id": 12345,
      "name": "Alice Smith",
      "email": "alice@example.com",
      "age": 30,
      "active": true
   },
   "settings": {
      "theme": "dark",
      "language": "en",
      "notifications": true,
      "timezone": "UTC",
      "currency": "USD"
   },
   "scores": [95, 87, 92, 88, 91],
   "metadata": {
      "created": "2024-01-15",
      "updated": "2024-06-20",
      "version": 3,
      "source": "api",
      "format": "json"
   },
   "tags": ["premium", "verified", "active"]
})";

// Generate a large JSON array of objects
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

int main()
{
   const auto large_json = generate_large_json(100);

   // ========================================================================
   // Benchmark: read_json (small object)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "read_json (small, 8 keys)";

      stage.run("ordered_small_map", [&] {
         generic_default json{};
         auto ec = glz::read_json(json, small_json);
         if (ec) std::abort();
         bencher::do_not_optimize(json);
         return small_json.size();
      });

      stage.run("std::map", [&] {
         generic_std_map json{};
         auto ec = glz::read_json(json, small_json);
         if (ec) std::abort();
         bencher::do_not_optimize(json);
         return small_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: read_json (medium object)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "read_json (medium, nested)";

      stage.run("ordered_small_map", [&] {
         generic_default json{};
         auto ec = glz::read_json(json, medium_json);
         if (ec) std::abort();
         bencher::do_not_optimize(json);
         return medium_json.size();
      });

      stage.run("std::map", [&] {
         generic_std_map json{};
         auto ec = glz::read_json(json, medium_json);
         if (ec) std::abort();
         bencher::do_not_optimize(json);
         return medium_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: read_json (large, 100 objects)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "read_json (large, 100 objects)";

      stage.run("ordered_small_map", [&] {
         generic_default json{};
         auto ec = glz::read_json(json, large_json);
         if (ec) std::abort();
         bencher::do_not_optimize(json);
         return large_json.size();
      });

      stage.run("std::map", [&] {
         generic_std_map json{};
         auto ec = glz::read_json(json, large_json);
         if (ec) std::abort();
         bencher::do_not_optimize(json);
         return large_json.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: write_json (small object)
   // ========================================================================
   {
      generic_default json_default{};
      glz::read_json(json_default, small_json);
      generic_std_map json_stdmap{};
      glz::read_json(json_stdmap, small_json);

      bencher::stage stage;
      stage.name = "write_json (small, 8 keys)";

      stage.run("ordered_small_map", [&] {
         std::string out{};
         auto ec = glz::write_json(json_default, out);
         if (ec) std::abort();
         bencher::do_not_optimize(out);
         return out.size();
      });

      stage.run("std::map", [&] {
         std::string out{};
         auto ec = glz::write_json(json_stdmap, out);
         if (ec) std::abort();
         bencher::do_not_optimize(out);
         return out.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: write_json (small, reused buffer)
   // ========================================================================
   {
      generic_default json_default{};
      glz::read_json(json_default, small_json);
      generic_std_map json_stdmap{};
      glz::read_json(json_stdmap, small_json);

      std::string out_default, out_stdmap;
      out_default.reserve(512);
      out_stdmap.reserve(512);

      bencher::stage stage;
      stage.name = "write_json (small, 8 keys, reused buffer)";
      stage.cold_cache = false;

      stage.run("ordered_small_map", [&] {
         out_default.clear();
         auto ec = glz::write_json(json_default, out_default);
         if (ec) std::abort();
         bencher::do_not_optimize(out_default);
         return out_default.size();
      });

      stage.run("std::map", [&] {
         out_stdmap.clear();
         auto ec = glz::write_json(json_stdmap, out_stdmap);
         if (ec) std::abort();
         bencher::do_not_optimize(out_stdmap);
         return out_stdmap.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: write_json (medium object)
   // ========================================================================
   {
      generic_default json_default{};
      glz::read_json(json_default, medium_json);
      generic_std_map json_stdmap{};
      glz::read_json(json_stdmap, medium_json);

      bencher::stage stage;
      stage.name = "write_json (medium, nested)";

      stage.run("ordered_small_map", [&] {
         std::string out{};
         auto ec = glz::write_json(json_default, out);
         if (ec) std::abort();
         bencher::do_not_optimize(out);
         return out.size();
      });

      stage.run("std::map", [&] {
         std::string out{};
         auto ec = glz::write_json(json_stdmap, out);
         if (ec) std::abort();
         bencher::do_not_optimize(out);
         return out.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: write_json (medium, reused buffer)
   // ========================================================================
   {
      generic_default json_default{};
      glz::read_json(json_default, medium_json);
      generic_std_map json_stdmap{};
      glz::read_json(json_stdmap, medium_json);

      std::string out_default, out_stdmap;
      out_default.reserve(512);
      out_stdmap.reserve(512);

      bencher::stage stage;
      stage.name = "write_json (medium, nested, reused buffer)";
      stage.cold_cache = false;

      stage.run("ordered_small_map", [&] {
         out_default.clear();
         auto ec = glz::write_json(json_default, out_default);
         if (ec) std::abort();
         bencher::do_not_optimize(out_default);
         return out_default.size();
      });

      stage.run("std::map", [&] {
         out_stdmap.clear();
         auto ec = glz::write_json(json_stdmap, out_stdmap);
         if (ec) std::abort();
         bencher::do_not_optimize(out_stdmap);
         return out_stdmap.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Key lookup (operator[])
   // ========================================================================
   {
      generic_default json_default{};
      glz::read_json(json_default, small_json);
      generic_std_map json_stdmap{};
      glz::read_json(json_stdmap, small_json);

      bencher::stage stage;
      stage.name = "Key lookup (8 keys, access all)";

      stage.run("ordered_small_map", [&] {
         double sum = 0;
         sum += json_default["id"].get<double>();
         sum += json_default["age"].get<double>();
         sum += json_default["score"].get<double>();
         sum += json_default["active"].get<bool>() ? 1.0 : 0.0;
         bencher::do_not_optimize(sum);
         return 4 * sizeof(double);
      });

      stage.run("std::map", [&] {
         double sum = 0;
         sum += json_stdmap["id"].get<double>();
         sum += json_stdmap["age"].get<double>();
         sum += json_stdmap["score"].get<double>();
         sum += json_stdmap["active"].get<bool>() ? 1.0 : 0.0;
         bencher::do_not_optimize(sum);
         return 4 * sizeof(double);
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark: Roundtrip (read + write)
   // ========================================================================
   {
      bencher::stage stage;
      stage.name = "Roundtrip read+write (medium)";

      stage.run("ordered_small_map", [&] {
         generic_default json{};
         auto ec = glz::read_json(json, medium_json);
         if (ec) std::abort();
         std::string out{};
         auto ec2 = glz::write_json(json, out);
         if (ec2) std::abort();
         bencher::do_not_optimize(out);
         return medium_json.size() + out.size();
      });

      stage.run("std::map", [&] {
         generic_std_map json{};
         auto ec = glz::read_json(json, medium_json);
         if (ec) std::abort();
         std::string out{};
         auto ec2 = glz::write_json(json, out);
         if (ec2) std::abort();
         bencher::do_not_optimize(out);
         return medium_json.size() + out.size();
      });

      bencher::print_results(stage);
   }

   return 0;
}
