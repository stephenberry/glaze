// Benchmark lazy JSON bulk ingest: heterogeneous split-catalog JSON with mixed row
// kinds (scalar cells, object records, numeric rows) and per-row materialization heap
// traffic between parse steps — closer to warehouse build than a uniform micro-loop.

#include <cstdint>
#include <iostream>
#include <string>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "lazy_load_common.hpp"

namespace
{
   using namespace lazy_load_bench;

   template <auto Opts, typename LoadFn>
   void bench_scalar_ingest(bencher::stage& stage, const char* label, const std::string& json, std::int64_t expected,
                            LoadFn load_fn)
   {
      stage.run(label, [&] {
         auto result = glz::lazy_json<Opts>(json);
         if (!result) std::abort();
         const auto sum = load_fn(*result);
         if (sum != expected) std::abort();
         return json.size();
      });
   }

   void configure_large_load_stage(bencher::stage& stage)
   {
      stage.min_execution_count = 1;
      stage.max_execution_count = 1;
      stage.warmup_duration_ms = 0;
   }

   inline std::string format_megabytes(std::size_t bytes)
   {
      const double mb = static_cast<double>(bytes) / static_cast<double>(lazy_load_bench::k_bytes_per_mib);
      return std::to_string(static_cast<int>(mb + 0.5)) + " MB";
   }
} // namespace

int main()
{
   // Confirm the slim opts still carry the streaming cursor (rule out silent streaming-off).
   static_assert(glz::lazy_streaming_cursor_active(lazy_load_bench::kStreamingEnabled), "full: streaming expected ON");
   static_assert(glz::lazy_streaming_cursor_active(lazy_load_bench::kStreamingEnabledSlim),
                 "slim: streaming expected ON");
   static_assert(!glz::lazy_streaming_cursor_active(lazy_load_bench::kEndBoundedSlim), "slim end-bounded: streaming OFF");

   std::cerr << "sizeof(lazy_json_view) full = " << sizeof(glz::lazy_json_view<lazy_load_bench::kStreamingEnabled>)
             << " bytes, slim = " << sizeof(glz::lazy_json_view<lazy_load_bench::kStreamingEnabledSlim>) << " bytes\n";

   constexpr std::size_t k_prefix = 120;
   constexpr std::size_t k_prefix_rows = lazy_load_bench::k_default_cols_per_row;
   constexpr std::size_t k_payload = 500;
   constexpr std::size_t k_heavy = 12;
   constexpr std::size_t k_heavy_entity_bytes = 180 * lazy_load_bench::k_bytes_per_mib;
   constexpr std::size_t k_scalar_heavy_rows = 12000;
   constexpr std::size_t k_cols = lazy_load_bench::k_default_cols_per_row;

   std::cerr << "generating mixed-warehouse fixture...\n";
   const auto warehouse_json =
      generate_mixed_warehouse_json(k_prefix, k_prefix_rows, k_payload, k_heavy, k_heavy_entity_bytes);
   const auto warehouse_label = format_megabytes(warehouse_json.size());
   std::cerr << "mixed-warehouse fixture ready: " << warehouse_label << " (" << warehouse_json.size() << " bytes)\n";

   const auto catalog_json =
      generate_split_catalog_json(k_prefix, k_prefix_rows, k_payload, k_heavy, k_scalar_heavy_rows, k_cols);
   const auto catalog_label = format_megabytes(catalog_json.size());

   const auto flat_json = generate_flat_tables_json(lazy_load_bench::k_flat_smoke_num_tables,
                                                    lazy_load_bench::k_flat_smoke_rows_per_table,
                                                    lazy_load_bench::k_flat_smoke_cols_per_row);

   // GB-scale, pure-scalar-cell fixture: millions of small numeric/string cells, no giant
   // blobs, out of cache — isolates the effect of lazy_json_view size on cell-heavy ingest.
   constexpr std::size_t k_flat_big_num_tables = 60;
   constexpr std::size_t k_flat_big_rows_per_table = 120000;
   constexpr std::size_t k_flat_big_cols_per_row = lazy_load_bench::k_default_cols_per_row; // 12
   std::cerr << "generating GB-scale flat-tables fixture (" << k_flat_big_num_tables << " tables x "
             << k_flat_big_rows_per_table << " rows x " << k_flat_big_cols_per_row << " cols)...\n";
   const auto flat_big_json =
      generate_flat_tables_json(k_flat_big_num_tables, k_flat_big_rows_per_table, k_flat_big_cols_per_row);
   const auto flat_big_label = format_megabytes(flat_big_json.size());
   std::cerr << "GB-scale flat-tables fixture ready: " << flat_big_label << " (" << flat_big_json.size()
             << " bytes)\n";

   std::int64_t expected_warehouse{};
   {
      auto doc = glz::lazy_json<kStreamingEnabled>(warehouse_json);
      if (!doc) std::abort();
      expected_warehouse = load_mixed_warehouse_catalog<kStreamingEnabled>(*doc);
   }

   std::int64_t expected_catalog{};
   {
      auto doc = glz::lazy_json<kStreamingEnabled>(catalog_json);
      if (!doc) std::abort();
      expected_catalog = load_split_catalog_scalars<kStreamingEnabled>(*doc);
   }

   std::int64_t expected_flat{};
   {
      auto doc = glz::lazy_json<kEndBounded>(flat_json);
      if (!doc) std::abort();
      expected_flat = load_flat_tables_scalars<kEndBounded>((*doc)["tables"]);
   }

   std::int64_t expected_flat_big{};
   {
      auto doc = glz::lazy_json<kEndBounded>(flat_big_json);
      if (!doc) std::abort();
      expected_flat_big = load_flat_tables_scalars<kEndBounded>((*doc)["tables"]);
   }

   {
      bencher::stage stage;
      configure_large_load_stage(stage);
      stage.name = std::string("Mixed-warehouse ingest (~") + warehouse_label +
                   "): heterogeneous rows + materialization heap, streaming off vs on";

      bench_scalar_ingest<kEndBounded>(stage, "streaming off (default opts)", warehouse_json, expected_warehouse,
                                       [](const glz::lazy_document<kEndBounded>& doc) {
                                          return load_mixed_warehouse_catalog<kEndBounded>(doc);
                                       });
      bench_scalar_ingest<kStreamingEnabled>(stage, "streaming on (enabled)", warehouse_json, expected_warehouse,
                                             [](const glz::lazy_document<kStreamingEnabled>& doc) {
                                                return load_mixed_warehouse_catalog<kStreamingEnabled>(doc);
                                             });

      bencher::print_results(stage);
   }

   {
      bencher::stage stage;
      configure_large_load_stage(stage);
      stage.name = std::string("Split-catalog scalar-cell load (~") + catalog_label +
                   "): streaming cursor off vs on";

      bench_scalar_ingest<kEndBounded>(stage, "streaming off (default opts)", catalog_json, expected_catalog,
                                       [](const glz::lazy_document<kEndBounded>& doc) {
                                          return load_split_catalog_scalars<kEndBounded>(doc);
                                       });
      bench_scalar_ingest<kStreamingEnabled>(stage, "streaming on (enabled)", catalog_json, expected_catalog,
                                             [](const glz::lazy_document<kStreamingEnabled>& doc) {
                                                return load_split_catalog_scalars<kStreamingEnabled>(doc);
                                             });

      bencher::print_results(stage);
   }

   {
      bencher::stage stage;
      stage.name = "Flat tables smoke (~3.8 MB): streaming off vs on";

      bench_scalar_ingest<kEndBounded>(stage, "streaming off", flat_json, expected_flat,
                                       [](const glz::lazy_document<kEndBounded>& doc) {
                                          return load_flat_tables_scalars<kEndBounded>(doc["tables"]);
                                       });
      bench_scalar_ingest<kStreamingEnabled>(stage, "streaming on (enabled)", flat_json, expected_flat,
                                               [](const glz::lazy_document<kStreamingEnabled>& doc) {
                                                  return load_flat_tables_scalars<kStreamingEnabled>(doc["tables"]);
                                               });

      bencher::print_results(stage);
   }

   {
      bencher::stage stage;
      configure_large_load_stage(stage);
      stage.name = std::string("GB-scale flat-tables cell-heavy ingest (~") + flat_big_label +
                   "): pure scalar cells, out of cache, streaming off vs on";

      bench_scalar_ingest<kEndBounded>(stage, "streaming off (default opts)", flat_big_json, expected_flat_big,
                                       [](const glz::lazy_document<kEndBounded>& doc) {
                                          return load_flat_tables_scalars<kEndBounded>(doc["tables"]);
                                       });
      bench_scalar_ingest<kStreamingEnabled>(stage, "streaming on (enabled)", flat_big_json, expected_flat_big,
                                             [](const glz::lazy_document<kStreamingEnabled>& doc) {
                                                return load_flat_tables_scalars<kStreamingEnabled>(doc["tables"]);
                                             });

      bencher::print_results(stage);
   }

   // Full (48-byte) vs slim (24-byte) lazy_json_view on the same realistic ingest loop
   // (streaming on for both, so the only variable is the value view size). This is the
   // faithful A/B: the actual read_into-per-cell workload, no synthetic copy harness.
   {
      bencher::stage stage;
      configure_large_load_stage(stage);
      stage.name = std::string("Split-catalog scalar-cell load (~") + catalog_label +
                   "): full 48B view vs slim 24B view, streaming on";
      bench_scalar_ingest<kStreamingEnabled>(stage, "full view (48 B)", catalog_json, expected_catalog,
                                             [](const glz::lazy_document<kStreamingEnabled>& doc) {
                                                return load_split_catalog_scalars<kStreamingEnabled>(doc);
                                             });
      bench_scalar_ingest<kStreamingEnabledSlim>(stage, "slim view (24 B)", catalog_json, expected_catalog,
                                                 [](const glz::lazy_document<kStreamingEnabledSlim>& doc) {
                                                    return load_split_catalog_scalars<kStreamingEnabledSlim>(doc);
                                                 });
      bencher::print_results(stage);
   }

   {
      bencher::stage stage;
      configure_large_load_stage(stage);
      stage.name = std::string("GB-scale flat-tables cell-heavy ingest (~") + flat_big_label +
                   "): full vs slim view x streaming off/on (4-way isolation)";
      bench_scalar_ingest<kEndBounded>(stage, "full 48B, streaming OFF", flat_big_json, expected_flat_big,
                                       [](const glz::lazy_document<kEndBounded>& doc) {
                                          return load_flat_tables_scalars<kEndBounded>(doc["tables"]);
                                       });
      bench_scalar_ingest<kStreamingEnabled>(stage, "full 48B, streaming ON", flat_big_json, expected_flat_big,
                                             [](const glz::lazy_document<kStreamingEnabled>& doc) {
                                                return load_flat_tables_scalars<kStreamingEnabled>(doc["tables"]);
                                             });
      bench_scalar_ingest<kEndBoundedSlim>(stage, "slim 24B, streaming OFF", flat_big_json, expected_flat_big,
                                           [](const glz::lazy_document<kEndBoundedSlim>& doc) {
                                              return load_flat_tables_scalars<kEndBoundedSlim>(doc["tables"]);
                                           });
      bench_scalar_ingest<kStreamingEnabledSlim>(stage, "slim 24B, streaming ON", flat_big_json, expected_flat_big,
                                                 [](const glz::lazy_document<kStreamingEnabledSlim>& doc) {
                                                    return load_flat_tables_scalars<kStreamingEnabledSlim>(doc["tables"]);
                                                 });
      bencher::print_results(stage);
   }

   return 0;
}
