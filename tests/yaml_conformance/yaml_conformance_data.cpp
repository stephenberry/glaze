// Data-driven YAML conformance test.
// Iterates over yaml-test-suite case directories at runtime,
// comparing glaze output against expected JSON / YAML / error results.
//
// Requires YAML_TEST_SUITE_DIR to be defined at compile time,
// pointing to the yaml-test-suite data directory.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/yaml.hpp"
#include "ut/ut.hpp"

using namespace ut;
namespace fs = std::filesystem;

namespace
{
   std::string read_file(const fs::path& path)
   {
      std::ifstream f(path, std::ios::binary);
      if (!f) return {};
      std::ostringstream ss;
      ss << f.rdbuf();
      return ss.str();
   }

   // Normalize JSON by parsing and re-serializing through glaze.
   std::string normalize_json(const std::string& json)
   {
      glz::generic val{};
      if (auto ec = glz::read_json(val, json); bool(ec)) {
         return {};
      }
      std::string out;
      (void)glz::write_json(val, out);
      return out;
   }

   // Cases where glaze is known to not yet conform.
   const std::set<std::string> skip_cases = {};

   // Some cases have multiple sub-cases (in.yaml, in.yaml:1, etc.)
   // The yaml-test-suite data branch stores one sub-case per directory.

   struct test_result
   {
      std::string id{};
      bool passed{};
      bool skipped{};
      std::string detail{};
   };
}

suite yaml_conformance_data_tests = [] {
   "yaml_test_suite_data_driven"_test = [] {
#ifndef YAML_TEST_SUITE_DIR
      expect(false) << "YAML_TEST_SUITE_DIR not defined";
      return;
#else
      const fs::path suite_dir{YAML_TEST_SUITE_DIR};
      expect(fs::exists(suite_dir)) << "yaml-test-suite directory not found: " << suite_dir.string();
      if (!fs::exists(suite_dir)) return;

      std::vector<test_result> results;
      int total = 0;
      int passed = 0;
      int failed = 0;
      int skipped = 0;
      int error_pass = 0;
      int error_fail = 0;
      int json_pass = 0;
      int json_fail = 0;
      int yaml_pass = 0;
      int yaml_fail = 0;

      // Collect and sort case directories
      std::vector<fs::path> case_dirs;
      for (const auto& entry : fs::directory_iterator(suite_dir)) {
         if (entry.is_directory()) {
            const auto name = entry.path().filename().string();
            // yaml-test-suite case IDs are 4 alphanumeric characters
            if (name.size() == 4) {
               case_dirs.push_back(entry.path());
            }
         }
      }
      std::sort(case_dirs.begin(), case_dirs.end());

      for (const auto& case_dir : case_dirs) {
         const auto id = case_dir.filename().string();
         const auto in_yaml_path = case_dir / "in.yaml";

         if (!fs::exists(in_yaml_path)) continue;
         ++total;

         if (skip_cases.count(id)) {
            ++skipped;
            results.push_back({id, false, true, "skipped"});
            continue;
         }

         const bool expect_error = fs::exists(case_dir / "error");
         const auto in_yaml = read_file(in_yaml_path);

         glz::generic parsed{};
         auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, in_yaml);
         const bool parse_failed = bool(ec);

         if (expect_error) {
            if (parse_failed) {
               ++passed;
               ++error_pass;
               results.push_back({id, true, false, "correctly rejected"});
            }
            else {
               ++failed;
               ++error_fail;
               results.push_back({id, false, false, "should have failed but parsed successfully"});
            }
            continue;
         }

         if (parse_failed) {
            ++failed;
            results.push_back({id, false, false, "parse error: " + glz::format_error(ec, in_yaml)});
            continue;
         }

         // Compare against expected JSON if available
         const auto in_json_path = case_dir / "in.json";
         if (fs::exists(in_json_path)) {
            const auto expected_json_raw = read_file(in_json_path);
            const auto expected_json = normalize_json(expected_json_raw);
            if (expected_json.empty()) {
               // Can't normalize expected JSON — count as parsed but not a JSON match
               ++passed;
               results.push_back({id, true, false, "parsed (expected JSON couldn't be normalized)"});
               continue;
            }

            std::string actual_json;
            (void)glz::write_json(parsed, actual_json);

            if (actual_json == expected_json) {
               ++passed;
               ++json_pass;
               results.push_back({id, true, false, "JSON match"});
            }
            else {
               ++failed;
               ++json_fail;
               results.push_back(
                  {id, false, false, "JSON mismatch\n  actual:   " + actual_json + "\n  expected: " + expected_json});
            }
            continue;
         }

         // Compare via YAML roundtrip if out.yaml is available
         const auto out_yaml_path = case_dir / "out.yaml";
         if (fs::exists(out_yaml_path)) {
            const auto expected_yaml = read_file(out_yaml_path);

            glz::generic expected_parsed{};
            auto ec2 = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(expected_parsed, expected_yaml);

            std::string actual_yaml;
            (void)glz::write_yaml(parsed, actual_yaml);

            std::string expected_yaml_normalized;
            (void)glz::write_yaml(expected_parsed, expected_yaml_normalized);

            if (actual_yaml == expected_yaml_normalized) {
               ++passed;
               ++yaml_pass;
               results.push_back({id, true, false, "YAML roundtrip match"});
            }
            else {
               ++failed;
               ++yaml_fail;
               results.push_back({id, false, false,
                                  "YAML roundtrip mismatch\n  actual:\n" + actual_yaml +
                                     "\n  expected:\n" + expected_yaml_normalized +
                                     (bool(ec2) ? "\n  (out.yaml parse error: " + glz::format_error(ec2, expected_yaml) +
                                                     ")"
                                                : "")});
            }
            continue;
         }

         // No expected output to compare — just verify it parsed
         ++passed;
         results.push_back({id, true, false, "parsed (no expected output to compare)"});
      }

      // Print summary
      std::string summary = "\n=== YAML Test Suite Conformance ===\n";
      summary += "Total: " + std::to_string(total) + "\n";
      summary += "Passed: " + std::to_string(passed) + "\n";
      summary += "Failed: " + std::to_string(failed) + "\n";
      summary += "Skipped: " + std::to_string(skipped) + "\n";
      summary += "\nBreakdown:\n";
      summary += "  Error cases: " + std::to_string(error_pass) + " pass, " + std::to_string(error_fail) + " fail\n";
      summary += "  JSON match:  " + std::to_string(json_pass) + " pass, " + std::to_string(json_fail) + " fail\n";
      summary += "  YAML match:  " + std::to_string(yaml_pass) + " pass, " + std::to_string(yaml_fail) + " fail\n";

      // Print failures
      bool has_failures = false;
      for (const auto& r : results) {
         if (!r.passed && !r.skipped) {
            if (!has_failures) {
               summary += "\nFailures:\n";
               has_failures = true;
            }
            summary += "  " + r.id + ": " + r.detail + "\n";
         }
      }

      // Use expect to report so the test output is visible
      expect(failed == 0) << summary;

      // Ensure we actually tested a reasonable number of cases
      expect(passed >= 330) << "Expected to pass at least 330 cases, only passed " << passed;
#endif
   };
};

int main() { return 0; }
