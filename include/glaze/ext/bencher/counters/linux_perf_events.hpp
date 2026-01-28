#pragma once

#include "bencher/config.hpp"

#if defined(BENCH_LINUX)

#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <unistd.h>
#include <iostream>
#include <libgen.h>
#include <cstring>
#include <cerrno>
#include <vector>

namespace bencher
{

   __inline__ size_t rdtsc()
   {
#if defined(__x86_64__)
      uint32_t a, d;
      __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
      return (unsigned long)a | ((unsigned long)d << 32);
#elif defined(__i386__)
      size_t x;
      __asm__ volatile("rdtsc" : "=A"(x));
      return x;
#else
      return 0;
#endif
   }

   template <int32_t TYPE = PERF_TYPE_HARDWARE>
   class linux_events
   {
     protected:
      std::vector<uint64_t> temp_result_vec{};
      std::vector<uint64_t> ids{};
      std::vector<int32_t> fds{}; // All file descriptors
      perf_event_attr attribs{};
      bool working{true};
      std::error_condition last_error{};
      size_t num_events{};
      int32_t fd{}; // Group leader fd (used for ioctl operations)

     public:
      linux_events(std::vector<int32_t> config_vec)
      {
         memset(&attribs, 0, sizeof(attribs));
         attribs.type = TYPE;
         attribs.size = sizeof(attribs);
         attribs.disabled = 1;
         attribs.exclude_kernel = 1;
         attribs.exclude_hv = 1;

         attribs.sample_period = 0;
         attribs.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
         const int32_t pid = 0; // the current process
         const int32_t cpu = -1; // all CPUs
         const unsigned long flags = 0;

         int32_t group = -1; // no group
         num_events = config_vec.size();
         ids.resize(config_vec.size());
         fds.reserve(config_vec.size());
         uint32_t i = 0;
         for (auto config : config_vec) {
            attribs.config = config;
            int32_t _fd = static_cast<int32_t>(syscall(__NR_perf_event_open, &attribs, pid, cpu, group, flags));
            if (_fd == -1) {
               report_error("perf_event_open");
            }
            else {
               fds.push_back(_fd);
            }
            ioctl(_fd, PERF_EVENT_IOC_ID, &ids[i++]);
            if (group == -1) {
               group = _fd;
               fd = _fd;
            }
         }

         temp_result_vec.resize(num_events * 2 + 1);
      }

      ~linux_events()
      {
         for (int32_t file_desc : fds) {
            close(file_desc);
         }
      }

      BENCH_ALWAYS_INLINE void start()
      {
         if (fd != -1) {
            if (ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) == -1) {
               report_error("ioctl(PERF_EVENT_IOC_RESET)");
            }

            if (ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) == -1) {
               report_error("ioctl(PERF_EVENT_IOC_ENABLE)");
            }
         }
      }

      BENCH_ALWAYS_INLINE void end(std::vector<uint64_t>& results)
      {
         if (fd != -1) {
            if (ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) == -1) {
               report_error("ioctl(PERF_EVENT_IOC_DISABLE)");
            }

            if (read(fd, temp_result_vec.data(), temp_result_vec.size() * 8) == -1) {
               report_error("read");
            }
         }
         // our actual results are in slots 1,3,5, ... of this structure
         for (uint32_t i = 1; i < temp_result_vec.size(); i += 2) {
            results[i / 2] = temp_result_vec[i];
         }
         for (uint32_t i = 2; i < temp_result_vec.size(); i += 2) {
            if (ids[i / 2 - 1] != temp_result_vec[i]) {
               report_error("event mismatch");
            }
         }
      }

      BENCH_ALWAYS_INLINE bool is_working() { return working; }

     protected:
      void report_error([[maybe_unused]] const std::string& operation)
      {
         working = false;
         last_error = std::system_category().default_error_condition(errno);
      }
   };

   template <class event_count>
   struct event_collector_type : public linux_events<>
   {
      event_collector_type()
         : linux_events<>{std::vector<int32_t>{PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS,
                                               PERF_COUNT_HW_BRANCH_INSTRUCTIONS, PERF_COUNT_HW_BRANCH_MISSES}} {}

      BENCH_ALWAYS_INLINE bool has_events() { return linux_events<>::is_working(); }

      [[nodiscard]] std::error_condition error()
      {
         if (!has_events()) {
            return linux_events<>::last_error;
         }
         return {};
      }

      template <class Function, class... FuncArgs>
      [[nodiscard]] BENCH_ALWAYS_INLINE std::error_condition start(event_count& count, Function&& function, FuncArgs&&... func_args)
      {
         std::vector<uint64_t> results{};
         if (has_events()) {
            linux_events<>::start();
         }
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
         count.elapsed = end_clock - start_clock;
         if (has_events()) {
            if (results.size() != linux_events<>::temp_result_vec.size()) {
               results.resize(linux_events<>::temp_result_vec.size());
            }
            linux_events<>::end(results);
            count.cycles.emplace(results[0]);  // Use perf_event cycles (works on ARM64)
            count.instructions.emplace(results[1]);
            count.branches.emplace(results[2]);
            count.missed_branches.emplace(results[3]);
         }
         else {
            // Fallback to RDTSC when perf_event not available (x86 only, returns 0 on ARM)
            count.cycles.emplace(cycleEnd - cycleStart);
         }
         return error();
      }
   };
}

#endif
