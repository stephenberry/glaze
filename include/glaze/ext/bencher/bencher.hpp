#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <format>
#include <iostream>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "bencher/cache_clearer.hpp"
#include "bencher/config.hpp"
#include "bencher/do_not_optimize.hpp"
#include "bencher/event_counter.hpp"
#include "bencher/file.hpp"

namespace bencher
{
   struct performance_metrics
   {
      double throughput_mb_per_sec{};
      std::optional<double> instructions_percentage_deviation;
      // Renamed field from throughput_percentage_deviation to throughput_median_percentage_deviation
      std::optional<double> throughput_median_percentage_deviation;
      std::optional<double> cycles_percentage_deviation;
      std::optional<double> instructions_per_execution;
      std::optional<double> branch_misses_per_execution;
      std::optional<uint64_t> total_iteration_count;
      std::optional<double> instructions_per_cycle;
      std::optional<double> branches_per_execution;
      std::optional<double> instructions_per_byte;
      std::optional<double> cycles_per_execution;
      std::optional<double> bytes_processed;
      std::optional<double> cycles_per_byte;
      std::optional<double> frequency_ghz;
      std::string name{};
      double time_in_ns{};

      BENCH_ALWAYS_INLINE bool operator>(const performance_metrics& other) const
      {
         return throughput_mb_per_sec > other.throughput_mb_per_sec;
      }
   };

   namespace stats
   {
      // Calculate mean of a vector of doubles
      inline double mean(const std::vector<double>& data)
      {
         if (data.empty()) throw std::invalid_argument("Data vector is empty.");
         double sum = std::accumulate(data.begin(), data.end(), 0.0);
         return sum / double(data.size());
      }

      inline double median(std::vector<double> data)
      {
         if (data.empty()) throw std::invalid_argument("Data vector is empty.");
         size_t n = data.size();
         size_t mid = n / 2;
         std::nth_element(data.begin(), data.begin() + mid, data.end());
         if (n % 2 == 1) {
            return data[mid];
         }
         else {
            // For even n, nth_element guarantees elements before mid are <= data[mid]
            // but they're not sorted, so find max of lower half
            double upper = data[mid];
            double lower = *std::max_element(data.begin(), data.begin() + mid);
            return (lower + upper) / 2.0;
         }
      }

      inline double standard_deviation(const std::vector<double>& data, double mean_val)
      {
         if (data.size() < 2) throw std::invalid_argument("At least two data points are required.");
         double accum = 0.0;
         for (const auto& val : data) {
            double diff = val - mean_val;
            accum += diff * diff;
         }
         return std::sqrt(accum / double(data.size() - 1));
      }

      // Calculate Median Absolute Deviation (MAD)
      inline double median_absolute_deviation(const std::vector<double>& data, double median_val)
      {
          std::vector<double> deviations;
          deviations.reserve(data.size());
          for (const auto& val : data) {
              deviations.emplace_back(std::abs(val - median_val));
          }
          return median(deviations);
      }

      // Fixed z-score for 95% confidence
      constexpr double z_score_95 = 1.96;
   }

   struct stage
   {
      std::string name{};
      uint64_t min_execution_count = 30;
      uint64_t max_execution_count = 1000;

      // Threshold for relative half-width of 95% CI, e.g. 2.0% means we stop
      // once the ±CI is within ±2% of the mean throughput.
      double confidence_interval_threshold = 2.0;

      // Warmup duration in milliseconds to stabilize CPU frequency
      uint32_t warmup_duration_ms = 1000;

      // If true, evict L1 cache between runs for cold-cache measurements
      // Set to false for warm-cache (steady-state) benchmarks
      bool cold_cache = true;

      // Baseline for comparison (empty string means compare to slowest)
      std::string baseline{};

      // Tracks whether warmup has been performed for this stage
      mutable bool warmed_up = false;

      std::vector<performance_metrics> results{};

      std::vector<event_count> events = std::vector<event_count>(max_execution_count);

      void warmup() const
      {
         if (warmed_up) return;

         using namespace std::chrono;
         auto start = steady_clock::now();
         while (duration_cast<duration<float, std::milli>>(steady_clock::now() - start) <
                duration<float, std::milli>{static_cast<float>(warmup_duration_ms)}) {
            cache_clearer::evict_l1_cache();
         }
         warmed_up = true;
      }

      template <class Function, class... Args>
      const performance_metrics& run(const std::string_view subject_name, Function&& function,
                                     Args&&... args)
      {
         warmup();

         event_collector collector{};

         if (auto ec = collector.error()) {
            std::cerr << ec.message() << '\n';
         }

         // We will collect individual throughput measurements (MB/s) here
         // for each run to compute a confidence interval.
         std::vector<double> throughput_values;
         throughput_values.reserve(max_execution_count);

         size_t final_iteration = 0;

         // We need bytes_processed for single-run throughput. We'll fetch it
         // after each collector.start(...) call.
         // Because we don't know bytes_processed until we run the function at least once,
         // we assume bytes_processed is constant across runs.
         double bytes_processed = 0.0;

         for (size_t i = 0; i < max_execution_count; ++i) {
            if (cold_cache) {
               cache_clearer::evict_l1_cache();
            }

            std::ignore = collector.start(events[i], function, args...);

            // For a single run:
            double run_ns = events[i].elapsed_ns();
            bytes_processed = static_cast<double>(events[i].bytes_processed);

            // Convert ns -> s and bytes -> MB
            // single-run throughput = MB/s
            double mb_processed = bytes_processed / (1024.0 * 1024.0);
            double run_throughput_mb_per_s = (run_ns > 0.0) ? (mb_processed * 1e9 / run_ns) : 0.0;

            throughput_values.emplace_back(run_throughput_mb_per_s);

            // Only start checking CI after we've met min_execution_count
            if (i >= min_execution_count - 1 && throughput_values.size() > 1) {
               double mean_val = stats::mean(throughput_values);
               double stdev_val = stats::standard_deviation(throughput_values, mean_val);

               // 95% CI half-width
               double half_ci = stats::z_score_95 * (stdev_val / std::sqrt(double(throughput_values.size())));

               // Relative half CI in %
               double rel_half_ci_percent = (mean_val != 0.0) ? (half_ci / mean_val * 100.0) : 0.0;

               // If the half-width is below our threshold, we can stop
               if (rel_half_ci_percent < confidence_interval_threshold) {
                  final_iteration = i;
                  break;
               }
            }

            final_iteration = i; // If we never break, this ends up as max_execution_count - 1
         }

         // Now compute final metrics from iteration 0..final_iteration
         return results.emplace_back(collect_metrics(subject_name, final_iteration, throughput_values));
      }

      /// Run a benchmark with multiple parameter values.
      /// The function should take a single parameter and return bytes_processed.
      /// Results are stored with names formatted as "base_name/param_value".
      ///
      /// Example:
      ///   stage.run_with("sort", [](size_t n) {
      ///      std::vector<int> v(n);
      ///      std::sort(v.begin(), v.end());
      ///      return n * sizeof(int);
      ///   }, {1000, 10000, 100000});
      template <class Function, class T>
      void run_with(std::string_view base_name, Function&& function, std::initializer_list<T> params)
      {
         for (const auto& param : params) {
            std::string name = std::format("{}/{}", base_name, param);
            run(name, function, param);
         }
      }

      /// Run a benchmark with multiple parameter values from a range.
      /// Accepts any range type (vector, array, span, etc.).
      ///
      /// Example:
      ///   std::vector<size_t> sizes = {1000, 10000, 100000};
      ///   stage.run_with("sort", sort_func, sizes);
      template <class Function, class Range>
         requires std::ranges::input_range<Range>
      void run_with(std::string_view base_name, Function&& function, const Range& params)
      {
         for (const auto& param : params) {
            std::string name = std::format("{}/{}", base_name, param);
            run(name, function, param);
         }
      }

      /// Run a benchmark with per-iteration setup.
      /// The setup function is called before each iteration (untimed) and returns
      /// state that is passed to the benchmark function.
      ///
      /// Use this when the benchmark mutates its input and you need fresh state
      /// for each iteration (e.g., sorting, in-place algorithms, consuming data structures).
      ///
      /// Example:
      ///   stage.run_with_setup("sort",
      ///      [] { return generate_random_data(10000); },  // setup (untimed)
      ///      [](auto& data) {                             // benchmark (timed)
      ///         std::sort(data.begin(), data.end());
      ///         return data.size() * sizeof(int);
      ///      }
      ///   );
      template <class Setup, class Function>
      const performance_metrics& run_with_setup(const std::string_view subject_name, Setup&& setup, Function&& function)
      {
         warmup();

         event_collector collector{};

         if (auto ec = collector.error()) {
            std::cerr << ec.message() << '\n';
         }

         std::vector<double> throughput_values;
         throughput_values.reserve(max_execution_count);

         size_t final_iteration = 0;
         double bytes_processed = 0.0;

         for (size_t i = 0; i < max_execution_count; ++i) {
            if (cold_cache) {
               cache_clearer::evict_l1_cache();
            }

            // Call setup before timing starts (untimed)
            auto state = setup();

            // Time only the benchmark function, passing the setup state
            std::ignore = collector.start(events[i], [&]() {
               return function(state);
            });

            double run_ns = events[i].elapsed_ns();
            bytes_processed = static_cast<double>(events[i].bytes_processed);

            double mb_processed = bytes_processed / (1024.0 * 1024.0);
            double run_throughput_mb_per_s = (run_ns > 0.0) ? (mb_processed * 1e9 / run_ns) : 0.0;

            throughput_values.emplace_back(run_throughput_mb_per_s);

            if (i >= min_execution_count - 1 && throughput_values.size() > 1) {
               double mean_val = stats::mean(throughput_values);
               double stdev_val = stats::standard_deviation(throughput_values, mean_val);

               double half_ci = stats::z_score_95 * (stdev_val / std::sqrt(double(throughput_values.size())));
               double rel_half_ci_percent = (mean_val != 0.0) ? (half_ci / mean_val * 100.0) : 0.0;

               if (rel_half_ci_percent < confidence_interval_threshold) {
                  final_iteration = i;
                  break;
               }
            }

            final_iteration = i;
         }

         return results.emplace_back(collect_metrics(subject_name, final_iteration, throughput_values));
      }

      /**
       *  Compute metrics for all runs from index 0..i (inclusive).
       *  So if i=10, we gather 11 runs in total.
       */
      performance_metrics collect_metrics(const std::string_view subject_name, size_t i, const std::vector<double>& throughput_values)
      {
         performance_metrics pm{};
         pm.name = subject_name;

         // The total number of completed runs so far is i + 1
         size_t run_count = i + 1;
         assert(run_count <= events.size() && "run_count exceeds events.size()");

         pm.total_iteration_count = run_count;

         // Vectors to store individual metric values
         std::vector<double> ns_values;
         ns_values.reserve(run_count);

         std::vector<double> cycles_values;
         cycles_values.reserve(run_count);

         std::vector<double> instr_values;
         instr_values.reserve(run_count);

         std::vector<double> br_values;
         br_values.reserve(run_count);

         std::vector<double> missed_values;
         missed_values.reserve(run_count);

         double bytes_processed = 0.0;

         // Gather individual metrics from events[0..i]
         for (size_t idx = 0; idx < run_count; ++idx) {
            const auto& e = events[idx];

            double t_ns = e.elapsed_ns();
            ns_values.emplace_back(t_ns);

            double cyc = e.cycles.value_or(0.0);
            cycles_values.emplace_back(cyc);

            double instr = e.instructions.value_or(0.0);
            instr_values.emplace_back(instr);

            double br = e.branches.value_or(0.0);
            br_values.emplace_back(br);

            double brmiss = e.missed_branches.value_or(0.0);
            missed_values.emplace_back(brmiss);

            // bytes_processed is typically the same each run,
            // but if it can vary, additional logic would be needed.
            bytes_processed = double(e.bytes_processed);
         }

         double min_ns = *std::min_element(ns_values.begin(), ns_values.end());

         pm.bytes_processed = bytes_processed;

         // Compute medians
         [[maybe_unused]] double median_ns = stats::median(ns_values);
         double median_cycles = stats::median(cycles_values);
         double median_instr = stats::median(instr_values);
         double median_br = stats::median(br_values);
         double median_missed = stats::median(missed_values);

         // Sum of times across these runs
         double total_ns = std::accumulate(ns_values.begin(), ns_values.end(), 0.0);
         pm.time_in_ns = total_ns;

         // Compute throughput based on provided throughput_values
         double median_throughput = stats::median(throughput_values);
         pm.throughput_mb_per_sec = median_throughput;

         // Calculate Median Absolute Deviation (MAD) for throughput percentages of median
         std::vector<double> pct_diffs;
         pct_diffs.reserve(run_count);
         for (const auto& throughput : throughput_values) {
             if (median_throughput > 0.0) {
                 double pct_diff = std::abs(throughput - median_throughput) / median_throughput * 100.0;
                 pct_diffs.emplace_back(pct_diff);
             }
             else {
                 pct_diffs.emplace_back(0.0);
             }
         }
         double mad = stats::median(pct_diffs);
         pm.throughput_median_percentage_deviation = mad;

         // Instructions
         if (median_instr != 0.0) {
            // For instructions per byte
            if (bytes_processed != 0.0) {
               pm.instructions_per_byte = median_instr / bytes_processed;
            }

            // For instructions per cycle
            if (median_cycles != 0.0) {
               pm.instructions_per_cycle = median_instr / median_cycles;
            }

            pm.instructions_per_execution = median_instr;

            // Calculate deviation based on median
            double min_instr = *std::min_element(instr_values.begin(), instr_values.end());
            double instr_diff = median_instr - min_instr;
            double instr_dev = (median_instr > 0.0) ? (instr_diff * 100.0 / median_instr) : 0.0;
            pm.instructions_percentage_deviation = instr_dev;
         }

         // Cycles
         if (median_cycles != 0.0) {
            if (bytes_processed != 0.0) {
               pm.cycles_per_byte = median_cycles / bytes_processed;
            }
            pm.cycles_per_execution = median_cycles;
            double min_cycles = *std::min_element(cycles_values.begin(), cycles_values.end());
            double cyc_diff = median_cycles - min_cycles;
            double cyc_dev = (median_cycles > 0.0) ? (cyc_diff * 100.0 / median_cycles) : 0.0;
            pm.cycles_percentage_deviation = cyc_dev;

            pm.frequency_ghz = min_cycles / min_ns;
         }

         // Branches
         if (median_br != 0.0) {
            pm.branches_per_execution = median_br;
            pm.branch_misses_per_execution = median_missed;
         }

         return pm;
      }
   };
}
