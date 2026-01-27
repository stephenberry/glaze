#pragma once

#if !defined(_MSC_VER)
#include <dirent.h>
#endif

#include <chrono>
#include <optional>

#include "bencher/config.hpp"
#include "bencher/counters/apple_arm_perf_events.hpp"
#include "bencher/counters/linux_perf_events.hpp"
#include "bencher/counters/windows_perf_events.hpp"

namespace bencher
{
   struct event_count
   {
      double elapsed_ns() const noexcept
      {
         return std::chrono::duration<double, std::nano>(elapsed).count();
      }

      std::optional<uint64_t> missed_branches{};
      uint64_t bytes_processed{};
      std::optional<uint64_t> instructions{};
      std::chrono::duration<double> elapsed{};
      std::optional<uint64_t> branches{};
      std::optional<uint64_t> cycles{};
   };

   using event_collector = event_collector_type<event_count>;
}
