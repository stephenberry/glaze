// Scalar-ingest only — builds against fork or upstream Glaze headers for A/B comparison.
#include <cstdint>
#include <string>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "lazy_load_common.hpp"

namespace
{
   using namespace lazy_load_bench;

#if defined(GLAZE_LAZY_LOAD_FORK)
   struct streaming_enabled_opts_fork : end_bounded_opts {
      glz::lazy_streaming_cursor_policy lazy_streaming_cursor = glz::lazy_streaming_cursor_policy::enabled;
   };
   inline constexpr streaming_enabled_opts_fork kStreaming{};
#endif
} // namespace

int main()
{
   constexpr std::size_t k_prefix = 120;
   constexpr std::size_t k_prefix_rows = lazy_load_bench::k_default_cols_per_row;
   constexpr std::size_t k_payload = 500;
   constexpr std::size_t k_heavy = 10;
   constexpr std::size_t k_heavy_rows = 1200000;
   constexpr std::size_t k_cols = lazy_load_bench::k_default_cols_per_row;
   const auto json = generate_split_catalog_json(k_prefix, k_prefix_rows, k_payload, k_heavy, k_heavy_rows, k_cols);

#if defined(GLAZE_LAZY_LOAD_FORK)
   std::int64_t expected_stream{};
   {
      auto doc = glz::lazy_json<kStreaming>(json);
      if (!doc) std::abort();
      expected_stream = scan_headers_skip_rows<kStreaming>(*doc);
   }
#endif

   std::int64_t expected_base{};
   {
      auto doc = glz::lazy_json<kEndBounded>(json);
      if (!doc) std::abort();
      expected_base = scan_headers_skip_rows<kEndBounded>(*doc);
   }

   {
      bencher::stage stage;
      stage.min_execution_count = 1;
      stage.max_execution_count = 1;
      stage.warmup_duration_ms = 0;
      stage.name = "Split-catalog header scan A/B (~1.2 GB fixture)";

      stage.run("streaming off (default opts)", [&] {
         auto result = glz::lazy_json<kEndBounded>(json);
         if (!result) std::abort();
         const auto sum = scan_headers_skip_rows<kEndBounded>(*result);
         if (sum != expected_base) std::abort();
         return json.size();
      });

#if defined(GLAZE_LAZY_LOAD_FORK)
      stage.run("streaming on (enabled)", [&] {
         auto result = glz::lazy_json<kStreaming>(json);
         if (!result) std::abort();
         const auto sum = scan_headers_skip_rows<kStreaming>(*result);
         if (sum != expected_stream) std::abort();
         return json.size();
      });
#endif

      bencher::print_results(stage);
   }

   return 0;
}
