// Single-config GB-scale flat-tables ingest, selected by argv[1], for interleaved
// separate-process A/B measurement (avoids the sequential-arm ordering/thermal confound of
// running all four configs in one process). Prints one line: "<cfg> cpu_ms=<n> MB/s=<n>".
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include "lazy_load_common.hpp"

namespace
{
   using namespace lazy_load_bench;

   template <auto Opts>
   long timed_ingest(const std::string& json, double& ms)
   {
      auto doc = glz::lazy_json<Opts>(json);
      if (!doc) {
         std::abort();
      }
      const auto t0 = std::chrono::steady_clock::now();
      const auto sum = load_flat_tables_scalars<Opts>((*doc)["tables"]);
      const auto t1 = std::chrono::steady_clock::now();
      ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      return static_cast<long>(sum);
   }

   // Models RAMA config A: iterate with the full-view iterator, but per cell round-trip the
   // value through a 24-byte {doc,data,parse_pos} handle and reconstruct a FULL view to read
   // through (as RAMA's makeValue/GlzViewHandle::view() does). Opts must be a FULL-view opts.
   template <auto Opts>
   long timed_ingest_recon(const std::string& json, double& ms)
   {
      auto doc = glz::lazy_json<Opts>(json);
      if (!doc) {
         std::abort();
      }
      struct SlimHandle
      {
         const glz::lazy_document<Opts>* d{};
         const char* data{};
         const char* pp{};
      };
      long sum = 0;
      std::size_t nulls = 0;
      const auto t0 = std::chrono::steady_clock::now();
      for (auto& table : (*doc)["tables"]) {
         auto rows = table["rows"];
         for (auto& row : rows) {
            for (auto& cell : row) {
               // deref -> slim 24-byte handle -> reconstruct full view -> read (RAMA's path)
               const SlimHandle h{cell.document(), cell.data(), cell.parse_pos()};
               glz::lazy_json_view<Opts> v{h.d, h.data};
               v.set_parse_pos(h.pp);
               sum += ingest_cell<Opts>(v, nulls);
            }
         }
      }
      const auto t1 = std::chrono::steady_clock::now();
      ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      return sum + static_cast<long>(nulls);
   }
} // namespace

int main(int argc, char** argv)
{
   const char* cfg = (argc > 1) ? argv[1] : "full-on";

   // ~1.14 GB flat cell-heavy fixture (same shape as lazy_load_benchmark's GB-scale stage).
   const auto json = generate_flat_tables_json(60, 120000, lazy_load_bench::k_default_cols_per_row);

   double ms = 0;
   long sum = 0;
   if (std::strcmp(cfg, "full-off") == 0) {
      sum = timed_ingest<kEndBounded>(json, ms);
   }
   else if (std::strcmp(cfg, "full-on") == 0) {
      sum = timed_ingest<kStreamingEnabled>(json, ms);
   }
   else if (std::strcmp(cfg, "slim-off") == 0) {
      sum = timed_ingest<kEndBoundedSlim>(json, ms);
   }
   else if (std::strcmp(cfg, "slim-on") == 0) {
      sum = timed_ingest<kStreamingEnabledSlim>(json, ms);
   }
   else if (std::strcmp(cfg, "recon-off") == 0) {
      sum = timed_ingest_recon<kEndBounded>(json, ms); // RAMA slim-handle + fat view, streaming off
   }
   else if (std::strcmp(cfg, "recon-on") == 0) {
      sum = timed_ingest_recon<kStreamingEnabled>(json, ms); // RAMA slim-handle + fat view, streaming on
   }
   else {
      std::printf("unknown cfg '%s'\n", cfg);
      return 2;
   }

   const double mb = static_cast<double>(json.size()) / (1024.0 * 1024.0);
   std::printf("%s cpu_ms=%.1f MB/s=%.1f sum=%ld\n", cfg, ms, mb / (ms / 1000.0), sum);
   return 0;
}
