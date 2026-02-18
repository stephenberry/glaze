#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/containers/ordered_map.hpp"
#include "tsl/ordered_map.h"

// Generate realistic JSON-like keys: "id", "name", "email", "address_0", etc.
std::vector<std::string> generate_keys(size_t n)
{
   // Start with common short JSON keys, then generate longer ones
   std::vector<std::string> base = {"id",        "name",    "email",   "age",       "active",  "role",
                                     "created",   "updated", "type",    "status",    "title",   "body",
                                     "url",       "path",    "method",  "headers",   "params",  "query",
                                     "page",      "limit",   "offset",  "total",     "count",   "data",
                                     "error",     "message", "code",    "timestamp", "version", "format",
                                     "encoding",  "length",  "width",   "height",    "color",   "font",
                                     "size",      "weight",  "opacity", "visible",   "enabled", "locked",
                                     "readonly",  "required","optional","default",   "min",     "max",
                                     "pattern",   "prefix",  "suffix",  "separator", "locale",  "timezone",
                                     "currency",  "country", "region",  "city",      "street",  "zip"};

   std::vector<std::string> keys;
   keys.reserve(n);
   for (size_t i = 0; i < n; ++i) {
      if (i < base.size()) {
         keys.push_back(base[i]);
      }
      else {
         keys.push_back("field_" + std::to_string(i));
      }
   }
   return keys;
}

struct bench_row
{
   size_t n;
   double glz_lookup;
   double tsl_lookup;
   double glz_insert;
   double tsl_insert;
   double glz_iteration;
   double tsl_iteration;
};

void write_markdown(const std::vector<bench_row>& rows, const char* path)
{
   FILE* f = std::fopen(path, "w");
   if (!f) return;

   std::fprintf(f, "# glz::ordered_map vs tsl::ordered_map\n\n");

   // Lookup table
   std::fprintf(f, "## Lookup (MB/s)\n\n");
   std::fprintf(f, "| n | glz | tsl | winner |\n");
   std::fprintf(f, "|--:|----:|----:|--------|\n");
   for (const auto& r : rows) {
      const char* winner = r.glz_lookup >= r.tsl_lookup ? "glz" : "tsl";
      std::fprintf(f, "| %zu | %.0f | %.0f | %s |\n", r.n, r.glz_lookup, r.tsl_lookup, winner);
   }

   // Insert table
   std::fprintf(f, "\n## Insert (MB/s)\n\n");
   std::fprintf(f, "| n | glz | tsl | winner |\n");
   std::fprintf(f, "|--:|----:|----:|--------|\n");
   for (const auto& r : rows) {
      const char* winner = r.glz_insert >= r.tsl_insert ? "glz" : "tsl";
      std::fprintf(f, "| %zu | %.0f | %.0f | %s |\n", r.n, r.glz_insert, r.tsl_insert, winner);
   }

   // Iteration table
   std::fprintf(f, "\n## Iteration (MB/s)\n\n");
   std::fprintf(f, "| n | glz | tsl | winner |\n");
   std::fprintf(f, "|--:|----:|----:|--------|\n");
   for (const auto& r : rows) {
      const char* winner = r.glz_iteration >= r.tsl_iteration ? "glz" : "tsl";
      std::fprintf(f, "| %zu | %.0f | %.0f | %s |\n", r.n, r.glz_iteration, r.tsl_iteration, winner);
   }

   std::fclose(f);
}

int main()
{
   std::vector<bench_row> rows;

   for (size_t n : {8, 16, 32, 64, 128, 256}) {
      auto keys = generate_keys(n);
      const size_t num_lookups = 10000;

      // Pick random lookup keys (all exist)
      std::mt19937 rng(42);
      std::uniform_int_distribution<size_t> dist(0, n - 1);
      std::vector<std::string> lookup_keys;
      lookup_keys.reserve(num_lookups);
      for (size_t i = 0; i < num_lookups; ++i) {
         lookup_keys.push_back(keys[dist(rng)]);
      }

      // Pre-build both maps
      glz::ordered_map<int> glz_map;
      tsl::ordered_map<std::string, int> tsl_map;
      for (size_t i = 0; i < n; ++i) {
         glz_map[keys[i]] = static_cast<int>(i);
         tsl_map[keys[i]] = static_cast<int>(i);
      }

      bench_row row{};
      row.n = n;

      // --- Lookup benchmark ---
      {
         bencher::stage stage;
         stage.name = "Lookup (n=" + std::to_string(n) + ")";
         stage.min_execution_count = 100;
         stage.cold_cache = false;

         stage.run("glz::ordered_map", [&] {
            uint64_t sum = 0;
            for (const auto& k : lookup_keys) {
               auto it = glz_map.find(k);
               sum += it->second;
            }
            bencher::do_not_optimize(sum);
            return num_lookups * sizeof(int);
         });

         stage.run("tsl::ordered_map", [&] {
            uint64_t sum = 0;
            for (const auto& k : lookup_keys) {
               auto it = tsl_map.find(k);
               sum += it->second;
            }
            bencher::do_not_optimize(sum);
            return num_lookups * sizeof(int);
         });

         bencher::print_results(stage);
         row.glz_lookup = stage.results[0].throughput_mb_per_sec;
         row.tsl_lookup = stage.results[1].throughput_mb_per_sec;
      }

      // --- Insert benchmark (into fresh map each iteration) ---
      {
         bencher::stage stage;
         stage.name = "Insert (n=" + std::to_string(n) + ")";
         stage.min_execution_count = 100;
         stage.cold_cache = false;

         stage.run("glz::ordered_map", [&] {
            glz::ordered_map<int> m;
            for (size_t i = 0; i < n; ++i) {
               m[keys[i]] = static_cast<int>(i);
            }
            bencher::do_not_optimize(m);
            return n * sizeof(int);
         });

         stage.run("tsl::ordered_map", [&] {
            tsl::ordered_map<std::string, int> m;
            for (size_t i = 0; i < n; ++i) {
               m[keys[i]] = static_cast<int>(i);
            }
            bencher::do_not_optimize(m);
            return n * sizeof(int);
         });

         bencher::print_results(stage);
         row.glz_insert = stage.results[0].throughput_mb_per_sec;
         row.tsl_insert = stage.results[1].throughput_mb_per_sec;
      }

      // --- Iteration benchmark ---
      {
         bencher::stage stage;
         stage.name = "Iteration (n=" + std::to_string(n) + ")";
         stage.min_execution_count = 100;
         stage.cold_cache = false;

         stage.run("glz::ordered_map", [&] {
            uint64_t sum = 0;
            for (const auto& [key, value] : glz_map) {
               sum += value;
            }
            bencher::do_not_optimize(sum);
            return n * (sizeof(std::string) + sizeof(int));
         });

         stage.run("tsl::ordered_map", [&] {
            uint64_t sum = 0;
            for (const auto& [key, value] : tsl_map) {
               sum += value;
            }
            bencher::do_not_optimize(sum);
            return n * (sizeof(std::string) + sizeof(int));
         });

         bencher::print_results(stage);
         row.glz_iteration = stage.results[0].throughput_mb_per_sec;
         row.tsl_iteration = stage.results[1].throughput_mb_per_sec;
      }

      rows.push_back(row);
   }

   write_markdown(rows, "ordered_map_results.md");
   std::printf("\nResults written to ordered_map_results.md\n");

   return 0;
}
