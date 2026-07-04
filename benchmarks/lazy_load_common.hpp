// Shared generators and ingest loops for lazy_load_benchmark / lazy_load_scalar.
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>

#include "glaze/json.hpp"
#if __has_include("glaze/core/lazy_streaming_cursor_policy.hpp")
#include "glaze/core/lazy_streaming_cursor_policy.hpp"
#define GLZ_LAZY_LOAD_HAS_STREAMING_POLICY 1
#else
#define GLZ_LAZY_LOAD_HAS_STREAMING_POLICY 0
#endif

namespace lazy_load_bench
{
   // --- Row shape ---
   inline constexpr std::size_t k_default_cols_per_row = 12;
   using numeric_row = std::array<double, k_default_cols_per_row>;

   // Rough per-row `reserve()` size estimates (chars).
   inline constexpr std::size_t k_mixed_row_estimated_cell_chars = 14;
   inline constexpr std::size_t k_mixed_row_estimated_framing_chars = 4; // `[` `]` commas
   inline constexpr std::size_t k_numeric_row_estimated_chars = 128;
   inline constexpr std::size_t k_object_row_estimated_chars = 120;
   inline constexpr std::size_t k_entity_header_estimated_chars = 384;
   inline constexpr std::size_t k_split_catalog_cell_chars_estimate = 10;
   inline constexpr std::size_t k_split_catalog_row_framing_chars = 8;

   // Heavy-entity row counts derived from target byte budget.
   inline constexpr std::size_t k_scalar_row_bytes_estimate = 110;
   inline constexpr std::size_t k_object_row_bytes_estimate = 900;
   inline constexpr std::size_t k_numeric_row_bytes_estimate = 160;

   // Deterministic mixed-row cell patterns.
   inline constexpr std::size_t k_null_cell_row_period = 19;
   inline constexpr std::size_t k_string_cell_col_stride = 4;

   // Legacy flat-tables cell patterns.
   inline constexpr std::size_t k_legacy_null_cell_row_period = 17;
   inline constexpr std::size_t k_legacy_string_cell_col_stride = 3;
   inline constexpr double k_legacy_numeric_cell_col_scale = 0.25;

   // Entity / cell value encoding.
   inline constexpr std::size_t k_label_group_modulus = 17;
   inline constexpr std::size_t k_category_modulus = 64;
   inline constexpr std::size_t k_disabled_row_period = 3;
   inline constexpr std::size_t k_object_fixed_payload_chars = 1024;
   inline constexpr std::size_t k_score_row_multiplier = 10;
   inline constexpr std::int64_t k_entity_id_scale = 1'000'000;
   inline constexpr std::int64_t k_numeric_cell_entity_scale = 100'000;
   // Row stride in numeric cell values — one char shy of mixed cell estimate.
   inline constexpr std::size_t k_numeric_cell_row_stride = k_mixed_row_estimated_cell_chars - 1;

   // Variable-length object payloads.
   inline constexpr std::size_t k_variable_payload_base_chars = 256;
   inline constexpr std::size_t k_variable_payload_span_chars = 1792;

   // Catalog layout.
   inline constexpr std::size_t k_row_kind_count = 3;
   inline constexpr std::size_t k_payload_id_base = 10'000;
   inline constexpr std::size_t k_mixed_warehouse_prefix_reserve = 4096;
   inline constexpr std::size_t k_mixed_warehouse_payload_reserve = 512;

   // Materialization heap / checksum helpers.
   inline constexpr std::size_t k_heap_words_base = 8;
   inline constexpr std::size_t k_heap_words_span = 24;
   inline constexpr std::uint64_t k_heap_lcg_multiplier = 6364136223846793005ULL;
   inline constexpr std::size_t k_heap_checksum_stride = 64;
   inline constexpr std::uint64_t k_checksum_low_bits_mask = 0xFFFFULL;
   inline constexpr double k_number_checksum_scale = 1000.0;

   // Cache-churn scratch stride (one cache line).
   inline constexpr std::size_t k_cache_line_bytes = 4096;
   inline constexpr std::size_t k_bytes_per_kib = 1024;
   inline constexpr std::size_t k_bytes_per_mib = k_bytes_per_kib * k_bytes_per_kib;

   // Legacy flat tables[] smoke fixture sizing.
   inline constexpr std::size_t k_flat_smoke_num_tables = 20;
   inline constexpr std::size_t k_flat_smoke_rows_per_table = 2000;
   inline constexpr std::size_t k_flat_smoke_cols_per_row = 8;

   // Payload string fill alphabet.
   inline constexpr std::size_t k_alphabet_size = 26;
   inline constexpr char k_alphabet_base = 'a';

   enum class row_kind : std::uint8_t { scalar = 0, object = 1, numeric = 2 };

   struct end_bounded_opts : glz::opts {
      bool null_terminated = false;
   };

   struct streaming_enabled_opts : end_bounded_opts {
#if GLZ_LAZY_LOAD_HAS_STREAMING_POLICY
      glz::lazy_streaming_cursor_policy lazy_streaming_cursor = glz::lazy_streaming_cursor_policy::enabled;
#endif
   };

   // Same as streaming_enabled_opts but opts into the slim (24-byte) lazy_json_view.
   // Used for the size/throughput A/B against the full 48-byte layout.
   struct streaming_enabled_slim_opts : streaming_enabled_opts {
#if GLZ_LAZY_LOAD_HAS_STREAMING_POLICY
      bool lazy_slim_view = true;
#endif
   };

   inline constexpr end_bounded_opts kEndBounded{};
   inline constexpr streaming_enabled_opts kStreamingEnabled{};
   inline constexpr streaming_enabled_slim_opts kStreamingEnabledSlim{};

   struct generic_record
   {
      std::int64_t id{};
      std::string name{};
      std::string label{};
      std::int64_t category{};
      bool enabled{};
      std::int64_t score{};
   };

   inline void append_row_array(std::string& json, std::size_t rows, std::size_t cols, std::size_t entity_idx)
   {
      json += R"(,"rows":[)";
      if (rows == 0) {
         json += ']';
         return;
      }

      json.reserve(json.size() + rows * (cols * k_mixed_row_estimated_cell_chars + k_mixed_row_estimated_framing_chars));
      for (std::size_t r = 0; r < rows; ++r) {
         if (r > 0) json += ',';
         json += '[';
         for (std::size_t c = 0; c < cols; ++c) {
            if (c > 0) json += ',';
            if (c == 0 && (r % k_null_cell_row_period == 0)) {
               json += "null";
            }
            else if (c % k_string_cell_col_stride == 0) {
               json += "\"c";
               json += std::to_string(entity_idx);
               json += '_';
               json += std::to_string(r);
               json += '"';
            }
            else {
               json += std::to_string(r + c + entity_idx);
            }
         }
         json += ']';
      }
      json += ']';
   }

   inline void append_numeric_row_array(std::string& json, std::size_t rows, std::size_t entity_idx)
   {
      json += R"(,"rows":[)";
      if (rows == 0) {
         json += ']';
         return;
      }

      json.reserve(json.size() + rows * k_numeric_row_estimated_chars);
      for (std::size_t r = 0; r < rows; ++r) {
         if (r > 0) json += ',';
         json += '[';
         for (std::size_t c = 0; c < numeric_row{}.size(); ++c) {
            if (c > 0) json += ',';
            json += std::to_string((entity_idx + 1) * k_numeric_cell_entity_scale + r * k_numeric_cell_row_stride + c);
         }
         json += ']';
      }
      json += ']';
   }

   inline void append_object_row_array(std::string& json, std::size_t rows, std::size_t entity_idx)
   {
      json += R"(,"rows":[)";
      if (rows == 0) {
         json += ']';
         return;
      }

      json.reserve(json.size() + rows * k_object_row_estimated_chars);
      for (std::size_t r = 0; r < rows; ++r) {
         if (r > 0) json += ',';
         json += R"({"id":)";
         json += std::to_string(entity_idx * k_entity_id_scale + r);
         json += R"(,"name":"record )";
         json += std::to_string(r);
         json += R"(","label":"group )";
         json += std::to_string(entity_idx % k_label_group_modulus);
         json += R"(","category":)";
         json += std::to_string((entity_idx + r) % k_category_modulus);
         json += R"(,"enabled":)";
         json += (r % k_disabled_row_period == 0) ? "false" : "true";
         json += R"(,"payload":")";
         for (std::size_t i = 0; i < k_object_fixed_payload_chars; ++i) {
            json += static_cast<char>(k_alphabet_base + ((r + i + entity_idx) % k_alphabet_size));
         }
         json += '"';
         json += R"(,"score":)";
         json += std::to_string(static_cast<std::int64_t>(r * k_score_row_multiplier + entity_idx));
         json += '}';
      }
      json += ']';
   }

   // Generic object header: int + strings + bools + int[] + row_kind before nested rows.
   inline void append_entity_header(std::string& json, std::size_t id, std::size_t entity_idx, bool extended,
                                    row_kind kind)
   {
      json += R"({"id":)";
      json += std::to_string(id);
      json += R"(,"name":"item_ )";
      json += std::to_string(entity_idx);
      json += R"(","note":"note for )";
      json += std::to_string(entity_idx);
      json += R"(","caption":"sample text block )";
      json += std::to_string(entity_idx);
      json += R"JSON(","kind":1,"enabled":true,"locked":false)JSON";
      if (extended) {
         json += R"JSON(,"opt_a":false,"opt_b":true)JSON";
      }
      json += R"(,"refs":[0,1,2,3],"row_kind":)";
      json += std::to_string(static_cast<unsigned>(kind));
   }

   inline void append_entity_header(std::string& json, std::size_t id, std::size_t entity_idx, bool extended)
   {
      append_entity_header(json, id, entity_idx, extended, row_kind::scalar);
   }

   inline void append_object_row_array_variable(std::string& json, std::size_t rows, std::size_t entity_idx)
   {
      json += R"(,"rows":[)";
      if (rows == 0) {
         json += ']';
         return;
      }

      for (std::size_t r = 0; r < rows; ++r) {
         if (r > 0) json += ',';
         const std::size_t payload_len =
            k_variable_payload_base_chars + ((r * k_label_group_modulus + entity_idx) % k_variable_payload_span_chars);
         json += R"({"id":)";
         json += std::to_string(entity_idx * k_entity_id_scale + r);
         json += R"(,"name":"record )";
         json += std::to_string(r);
         json += R"(","label":"group )";
         json += std::to_string(entity_idx % k_label_group_modulus);
         json += R"(","category":)";
         json += std::to_string((entity_idx + r) % k_category_modulus);
         json += R"(,"enabled":)";
         json += (r % k_disabled_row_period == 0) ? "false" : "true";
         json += R"(,"payload":")";
         for (std::size_t i = 0; i < payload_len; ++i) {
            json += static_cast<char>(k_alphabet_base + ((r + i + entity_idx) % k_alphabet_size));
         }
         json += '"';
         json += R"(,"score":)";
         json += std::to_string(static_cast<std::int64_t>(r * k_score_row_multiplier + entity_idx));
         json += '}';
      }
      json += ']';
   }

   inline void append_rows_for_kind(std::string& json, row_kind kind, std::size_t rows, std::size_t entity_idx,
                                    std::size_t cols = k_default_cols_per_row)
   {
      switch (kind) {
      case row_kind::scalar:
         append_row_array(json, rows, cols, entity_idx);
         break;
      case row_kind::object:
         append_object_row_array_variable(json, rows, entity_idx);
         break;
      case row_kind::numeric:
         append_numeric_row_array(json, rows, entity_idx);
         break;
      default:
         break;
      }
   }

   inline std::size_t heavy_rows_for_kind(row_kind kind, std::size_t target_entity_bytes)
   {
      switch (kind) {
      case row_kind::scalar:
         return target_entity_bytes / k_scalar_row_bytes_estimate;
      case row_kind::object:
         return target_entity_bytes / k_object_row_bytes_estimate;
      case row_kind::numeric:
         return target_entity_bytes / k_numeric_row_bytes_estimate;
      default:
         return 0;
      }
   }

   // Mixed row kinds per entity + split prefix/payload catalog. Heavy payload entities
   // rotate scalar / object / numeric matrices to mimic heterogeneous warehouse dumps.
   inline std::string generate_mixed_warehouse_json(std::size_t num_prefix, std::size_t prefix_rows_per_entity,
                                                    std::size_t num_payload, std::size_t heavy_payload,
                                                    std::size_t heavy_entity_bytes)
   {
      std::string json;
      json.reserve(num_prefix * k_mixed_warehouse_prefix_reserve + num_payload * k_mixed_warehouse_payload_reserve +
                   heavy_payload * heavy_entity_bytes);

      json += R"({"prefix":[)";
      for (std::size_t e = 0; e < num_prefix; ++e) {
         if (e > 0) json += ',';
         const auto kind = static_cast<row_kind>(e % k_row_kind_count);
         append_entity_header(json, e, e, false, kind);
         append_rows_for_kind(json, kind, prefix_rows_per_entity, e);
         json += '}';
      }

      json += R"(],"payload":[)";
      for (std::size_t e = 0; e < num_payload; ++e) {
         if (e > 0) json += ',';
         const auto kind = static_cast<row_kind>((e + 1) % k_row_kind_count);
         append_entity_header(json, k_payload_id_base + e, e, true, kind);
         const std::size_t rows =
            (e >= num_payload - heavy_payload) ? heavy_rows_for_kind(kind, heavy_entity_bytes) : 0;
         append_rows_for_kind(json, kind, rows, e);
         json += '}';
      }
      json += "]}";
      return json;
   }

   template <auto Opts>
   void ingest_entity_header(glz::lazy_json_view<Opts> entity, std::int64_t& checksum)
   {
      std::int64_t id{};
      if (!entity["id"].read_into(id)) {
         checksum += id;
      }

      std::string name{};
      if (!entity["name"].read_into(name)) {
         checksum += static_cast<std::int64_t>(name.size());
      }

      std::string caption{};
      if (!entity["caption"].read_into(caption)) {
         checksum += static_cast<std::int64_t>(caption.size());
      }

      bool enabled{};
      if (!entity["enabled"].read_into(enabled)) {
         checksum += enabled ? 1 : 0;
      }

      for (auto& ref : entity["refs"]) {
         std::int32_t v{};
         if (!ref.read_into(v)) {
            checksum += v;
         }
      }
   }

   template <auto Opts>
   std::int64_t ingest_cell(glz::lazy_json_view<Opts> cell, std::size_t& nulls)
   {
      if (cell.is_null()) {
         ++nulls;
         return 0;
      }
      if (cell.is_number()) {
         double v{};
         if (!cell.read_into(v)) {
            return static_cast<std::int64_t>(v * k_number_checksum_scale);
         }
      }
      else if (cell.is_string()) {
         std::string s{};
         if (!cell.read_into(s)) {
            return static_cast<std::int64_t>(s.size());
         }
      }
      return 0;
   }

   struct materialization_heap
   {
      std::vector<std::uint64_t> store{};

      void touch(std::int64_t seed, std::size_t weight)
      {
         const std::size_t words = k_heap_words_base + (weight % k_heap_words_span);
         const std::size_t base = store.size();
         store.resize(base + words);
         for (std::size_t i = 0; i < words; ++i) {
            store[base + i] =
               static_cast<std::uint64_t>(seed + static_cast<std::int64_t>(i)) * k_heap_lcg_multiplier;
         }
      }

      [[nodiscard]] std::int64_t checksum() const noexcept
      {
         std::int64_t sum{};
         for (std::size_t i = 0; i < store.size(); i += k_heap_checksum_stride) {
            sum += static_cast<std::int64_t>(store[i] & k_checksum_low_bits_mask);
         }
         return sum;
      }
   };

   template <auto Opts>
   std::int64_t load_entity_rows(glz::lazy_json_view<Opts> entity, row_kind kind, materialization_heap& heap)
   {
      std::int64_t checksum{};
      std::size_t nulls{};
      std::size_t row_count{};

      switch (kind) {
      case row_kind::scalar: {
         for (auto& row : entity["rows"]) {
            ++row_count;
            for (auto& cell : row) {
               checksum += ingest_cell<Opts>(cell, nulls);
            }
            heap.touch(checksum, row_count);
         }
         break;
      }
      case row_kind::object: {
         // Consume object records key-by-key via operator[] rather than
         // read_into<struct>. This mirrors a materialization layer that maps
         // named JSON fields onto its own storage (positional/columnar) instead
         // of Glaze reflection — the streaming cursor does not accelerate this
         // shape, so keeping it here prevents the headline from being inflated
         // by a read_into pattern such a consumer never exercises.
         for (auto& row : entity["rows"]) {
            std::int64_t   score{};
            std::string    name{};
            std::string    label{};
            row["score"].read_into(score);
            row["name" ].read_into(name);
            row["label"].read_into(label);
            ++row_count;
            checksum += score + static_cast<std::int64_t>(name.size() + label.size());
            heap.touch(checksum, name.size() + label.size());
         }
         break;
      }
      case row_kind::numeric: {
         for (auto& row : entity["rows"]) {
            numeric_row values{};
            if (!row.read_into(values)) {
               ++row_count;
               checksum += static_cast<std::int64_t>(values[0] + values.back());
               heap.touch(checksum, static_cast<std::size_t>(values[0]));
            }
         }
         break;
      }
      default:
         break;
      }

      checksum += static_cast<std::int64_t>(nulls);
      checksum += static_cast<std::int64_t>(row_count);
      checksum += heap.checksum();
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_mixed_warehouse_catalog(glz::lazy_json_view<Opts> root)
   {
      std::int64_t checksum{};
      materialization_heap heap{};
      for (const char* section : {"prefix", "payload"}) {
         for (auto& entity : root[section]) {
            ingest_entity_header<Opts>(entity, checksum);
            std::uint64_t kind_u{};
            if (!entity["row_kind"].read_into(kind_u)) {
               checksum += load_entity_rows<Opts>(entity, static_cast<row_kind>(kind_u), heap);
            }
         }
      }
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_mixed_warehouse_catalog(const glz::lazy_document<Opts>& doc)
   {
      return load_mixed_warehouse_catalog<Opts>(doc.root());
   }

   // Two top-level arrays: a smaller prefix section, then a payload section where
   // most entries have empty rows and a few carry large row matrices.
   inline std::string generate_split_catalog_json(std::size_t num_prefix, std::size_t prefix_rows_per_entity,
                                                  std::size_t num_payload, std::size_t heavy_payload,
                                                  std::size_t heavy_rows_per_entity, std::size_t cols_per_row)
   {
      std::string json;
      const std::size_t row_chars =
         cols_per_row * k_split_catalog_cell_chars_estimate + k_split_catalog_row_framing_chars;
      json.reserve(num_prefix * (prefix_rows_per_entity * row_chars + k_entity_header_estimated_chars) +
                   num_payload * k_entity_header_estimated_chars + heavy_payload * heavy_rows_per_entity * row_chars);

      json += R"({"prefix":[)";
      for (std::size_t e = 0; e < num_prefix; ++e) {
         if (e > 0) json += ',';
         append_entity_header(json, e, e, false);
         append_row_array(json, prefix_rows_per_entity, cols_per_row, e);
         json += '}';
      }

      json += R"(],"payload":[)";
      for (std::size_t e = 0; e < num_payload; ++e) {
         if (e > 0) json += ',';
         append_entity_header(json, k_payload_id_base + e, e, true);
         const std::size_t rows = (e >= num_payload - heavy_payload) ? heavy_rows_per_entity : 0;
         append_row_array(json, rows, cols_per_row, e);
         json += '}';
      }
      json += "]}";
      return json;
   }

   // Same generic envelope, but rows are fixed-width numeric records consumed
   // as whole row values. This isolates the read_into + iterator-advance win:
   // without a streaming cursor, advancing the row iterator re-skips the row
   // array that read_into just parsed.
   inline std::string generate_record_rows_json(std::size_t num_prefix, std::size_t prefix_rows_per_entity,
                                                std::size_t num_payload, std::size_t heavy_payload,
                                                std::size_t heavy_rows_per_entity)
   {
      std::string json;
      json.reserve(num_prefix * (prefix_rows_per_entity * k_numeric_row_estimated_chars + k_entity_header_estimated_chars) +
                   num_payload * k_entity_header_estimated_chars +
                   heavy_payload * heavy_rows_per_entity * k_numeric_row_estimated_chars);

      json += R"({"prefix":[)";
      for (std::size_t e = 0; e < num_prefix; ++e) {
         if (e > 0) json += ',';
         append_entity_header(json, e, e, false);
         append_numeric_row_array(json, prefix_rows_per_entity, e);
         json += '}';
      }

      json += R"(],"payload":[)";
      for (std::size_t e = 0; e < num_payload; ++e) {
         if (e > 0) json += ',';
         append_entity_header(json, k_payload_id_base + e, e, true);
         const std::size_t rows = (e >= num_payload - heavy_payload) ? heavy_rows_per_entity : 0;
         append_numeric_row_array(json, rows, e);
         json += '}';
      }
      json += "]}";
      return json;
   }

   inline std::string generate_object_record_rows_json(std::size_t num_prefix, std::size_t prefix_rows_per_entity,
                                                       std::size_t num_payload, std::size_t heavy_payload,
                                                       std::size_t heavy_rows_per_entity)
   {
      std::string json;
      json.reserve(num_prefix * (prefix_rows_per_entity * k_object_row_estimated_chars + k_entity_header_estimated_chars) +
                   num_payload * k_entity_header_estimated_chars +
                   heavy_payload * heavy_rows_per_entity * k_object_row_estimated_chars);

      json += R"({"prefix":[)";
      for (std::size_t e = 0; e < num_prefix; ++e) {
         if (e > 0) json += ',';
         append_entity_header(json, e, e, false);
         append_object_row_array(json, prefix_rows_per_entity, e);
         json += '}';
      }

      json += R"(],"payload":[)";
      for (std::size_t e = 0; e < num_payload; ++e) {
         if (e > 0) json += ',';
         append_entity_header(json, k_payload_id_base + e, e, true);
         const std::size_t rows = (e >= num_payload - heavy_payload) ? heavy_rows_per_entity : 0;
         append_object_row_array(json, rows, e);
         json += '}';
      }
      json += "]}";
      return json;
   }

   // Legacy flat tables[] fixture (~3.8 MB) — kept for quick smoke / historical comparison.
   inline std::string generate_flat_tables_json(std::size_t num_tables, std::size_t rows_per_table,
                                                std::size_t cols_per_row)
   {
      std::string json = R"({"tables":[)";
      for (std::size_t t = 0; t < num_tables; ++t) {
         if (t > 0) json += ',';
         json += R"({"name":"T)";
         json += std::to_string(t);
         json += R"(","rows":[)";
         for (std::size_t r = 0; r < rows_per_table; ++r) {
            if (r > 0) json += ',';
            json += '[';
            for (std::size_t c = 0; c < cols_per_row; ++c) {
               if (c > 0) json += ',';
               if (c == 0 && (r % k_legacy_null_cell_row_period == 0)) {
                  json += "null";
               }
               else if (c % k_legacy_string_cell_col_stride == 0) {
                  json += '"';
                  json += "cell_";
                  json += std::to_string(r);
                  json += '_';
                  json += std::to_string(c);
                  json += '"';
               }
               else {
                  json += std::to_string(static_cast<double>(r) +
                                         static_cast<double>(c) * k_legacy_numeric_cell_col_scale);
               }
            }
            json += ']';
         }
         json += "]}";
      }
      json += "]}";
      return json;
   }

   template <auto Opts>
   std::int64_t load_split_catalog_scalars(glz::lazy_json_view<Opts> root)
   {
      std::int64_t checksum{};
      std::size_t nulls{};
      std::size_t row_count{};
      for (const char* section : {"prefix", "payload"}) {
         for (auto& entity : root[section]) {
            ingest_entity_header<Opts>(entity, checksum);
            auto rows = entity["rows"];
            for (auto& row : rows) {
               ++row_count;
               for (auto& cell : row) {
                  checksum += ingest_cell<Opts>(cell, nulls);
               }
            }
         }
      }
      checksum += static_cast<std::int64_t>(nulls);
      checksum += static_cast<std::int64_t>(row_count);
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_split_catalog_scalars(const glz::lazy_document<Opts>& doc)
   {
      return load_split_catalog_scalars<Opts>(doc.root());
   }

   template <auto Opts>
   std::int64_t load_record_rows(glz::lazy_json_view<Opts> root)
   {
      std::int64_t checksum{};
      std::size_t row_count{};
      for (const char* section : {"prefix", "payload"}) {
         for (auto& entity : root[section]) {
            ingest_entity_header<Opts>(entity, checksum);
            for (auto& row : entity["rows"]) {
               numeric_row values{};
               if (!row.read_into(values)) {
                  ++row_count;
                  checksum += static_cast<std::int64_t>(values[0] + values[values.size() - 1]);
               }
            }
         }
      }
      checksum += static_cast<std::int64_t>(row_count);
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_record_rows(const glz::lazy_document<Opts>& doc)
   {
      return load_record_rows<Opts>(doc.root());
   }

   template <auto Opts>
   std::int64_t load_object_record_rows(glz::lazy_json_view<Opts> root)
   {
      std::int64_t checksum{};
      std::size_t row_count{};
      for (const char* section : {"prefix", "payload"}) {
         for (auto& entity : root[section]) {
            ingest_entity_header<Opts>(entity, checksum);
            for (auto& row : entity["rows"]) {
               generic_record record{};
               if (!row.read_into(record)) {
                  ++row_count;
                  checksum += record.score + static_cast<std::int64_t>(record.name.size() + record.label.size());
               }
            }
         }
      }
      checksum += static_cast<std::int64_t>(row_count);
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_object_record_rows(const glz::lazy_document<Opts>& doc)
   {
      return load_object_record_rows<Opts>(doc.root());
   }

   // Touch a separate working-set buffer between entity reads to approximate DB
   // materialization evicting the JSON buffer from CPU cache between parse steps.
   inline void churn_working_set(std::vector<std::uint64_t>& scratch, std::size_t entity_idx, std::int64_t& checksum)
   {
      if (scratch.empty()) {
         return;
      }
      std::uint64_t acc{};
      const std::size_t stride = k_cache_line_bytes / sizeof(std::uint64_t);
      for (std::size_t i = entity_idx % stride; i < scratch.size(); i += stride) {
         scratch[i] = scratch[i] * k_heap_lcg_multiplier + static_cast<std::uint64_t>(i + entity_idx);
         acc += scratch[i];
      }
      checksum += static_cast<std::int64_t>(acc & k_checksum_low_bits_mask);
   }

   template <auto Opts>
   std::int64_t load_object_record_rows_with_cache_churn(glz::lazy_json_view<Opts> root, std::size_t scratch_mb)
   {
      std::vector<std::uint64_t> scratch((scratch_mb * k_bytes_per_mib) / sizeof(std::uint64_t), 1ULL);
      std::int64_t checksum{};
      std::size_t row_count{};
      std::size_t entity_idx{};
      for (const char* section : {"prefix", "payload"}) {
         for (auto& entity : root[section]) {
            ingest_entity_header<Opts>(entity, checksum);
            for (auto& row : entity["rows"]) {
               generic_record record{};
               if (!row.read_into(record)) {
                  ++row_count;
                  checksum += record.score + static_cast<std::int64_t>(record.name.size() + record.label.size());
               }
            }
            churn_working_set(scratch, entity_idx++, checksum);
         }
      }
      checksum += static_cast<std::int64_t>(row_count);
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_object_record_rows_with_cache_churn(const glz::lazy_document<Opts>& doc, std::size_t scratch_mb)
   {
      return load_object_record_rows_with_cache_churn<Opts>(doc.root(), scratch_mb);
   }

   template <auto Opts>
   std::int64_t scan_headers_skip_rows(glz::lazy_json_view<Opts> root)
   {
      std::int64_t checksum{};
      for (const char* section : {"prefix", "payload"}) {
         for (auto& entity : root[section]) {
            ingest_entity_header<Opts>(entity, checksum);
         }
      }
      return checksum;
   }

   template <auto Opts>
   std::int64_t scan_headers_skip_rows(const glz::lazy_document<Opts>& doc)
   {
      return scan_headers_skip_rows<Opts>(doc.root());
   }

   template <auto Opts>
   std::int64_t load_flat_tables_scalars(glz::lazy_json_view<Opts> tables)
   {
      std::int64_t checksum{};
      std::size_t nulls{};
      std::size_t row_count{};
      for (auto& table : tables) {
         auto rows = table["rows"];
         for (auto& row : rows) {
            ++row_count;
            for (auto& cell : row) {
               checksum += ingest_cell<Opts>(cell, nulls);
            }
         }
      }
      checksum += static_cast<std::int64_t>(nulls);
      checksum += static_cast<std::int64_t>(row_count);
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_payload_row_rewind(glz::lazy_json_view<Opts> root)
   {
      std::int64_t checksum{};
      std::size_t row_count{};
      for (auto& entity : root["payload"]) {
         ingest_entity_header<Opts>(entity, checksum);
         auto rows = entity["rows"];
         for (auto& row : rows) {
            ++row_count;
         }
         rows = entity["rows"];
         for (auto& row : rows) {
            for (auto& cell : row) {
               if (cell.is_number()) {
                  double v{};
                  if (!cell.read_into(v)) {
                     checksum += static_cast<std::int64_t>(v);
                  }
               }
            }
         }
      }
      checksum += static_cast<std::int64_t>(row_count);
      return checksum;
   }

   template <auto Opts>
   std::int64_t load_payload_row_rewind(const glz::lazy_document<Opts>& doc)
   {
      return load_payload_row_rewind<Opts>(doc.root());
   }
} // namespace lazy_load_bench
