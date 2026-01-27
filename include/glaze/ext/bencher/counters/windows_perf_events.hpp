#pragma once

#include "bencher/config.hpp"

#if defined(BENCH_WIN)

#include <cerrno> // for errno
#include <chrono>
#include <cstring> // for memset
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <vector>

namespace bencher
{
   BENCH_ALWAYS_INLINE size_t rdtsc() { return __rdtsc(); }

   template <class event_count>
   struct event_collector_type
   {
      [[nodiscard]] std::error_condition error()
      {
         return {}; // No error on Windows, always works
      }

      template <class Function, class... FuncArgs>
      [[nodiscard]] BENCH_ALWAYS_INLINE std::error_condition start(event_count& count, Function&& function, FuncArgs&&... func_args)
      {
         const auto start_clock = std::chrono::steady_clock::now();
         volatile uint64_t cycleStart = rdtsc();
         if constexpr (std::is_void_v<std::invoke_result_t<Function, FuncArgs...>>) {
            std::forward<Function>(function)(std::forward<FuncArgs>(func_args)...);
            count.bytes_processed = 0;
         }
         else {
            count.bytes_processed = std::forward<Function>(function)(std::forward<FuncArgs>(func_args)...);
         }
         volatile uint64_t cycleEnd = rdtsc();
         const auto end_clock = std::chrono::steady_clock::now();
         count.cycles.emplace(cycleEnd - cycleStart);
         count.elapsed = end_clock - start_clock;
         return {};
      }
   };
}
#endif
