#pragma once

#include <iostream>

#include "bencher/config.hpp"

#if defined(BENCH_WIN)
#include <Windows.h>

#elif defined(BENCH_LINUX)
#include <xmmintrin.h>

#include <fstream>
#include <string>

#elif defined(BENCH_MAC)
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#endif

namespace bencher
{

#if defined(BENCH_WIN)

   inline size_t get_l1_cache_size()
   {
      DWORD buffer_size = 0;
      std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer{};

      GetLogicalProcessorInformation(nullptr, &buffer_size);
      buffer.resize(buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));

      if (!GetLogicalProcessorInformation(buffer.data(), &buffer_size)) {
         std::cerr << "Failed to retrieve processor information!" << std::endl;
         return 0;
      }

      size_t l1_cache_size = 0;
      const auto info_count = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
      for (size_t i = 0; i < info_count; ++i) {
         if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1 &&
             buffer[i].Cache.Type == CacheData) {
            l1_cache_size = buffer[i].Cache.Size;
            break;
         }
      }

      return l1_cache_size;
   }

#elif defined(BENCH_LINUX)

   inline size_t get_l1_cache_size()
   {
      const std::string cache_file = "/sys/devices/system/cpu/cpu0/cache/index0/size";
      std::ifstream file(cache_file);
      if (!file.is_open()) {
         std::cerr << "Failed to open cache info file: " << cache_file << std::endl;
         return 0;
      }

      std::string size_str;
      file >> size_str;
      file.close();

      if (size_str.back() == 'K') {
         return std::stoi(size_str) * 1024;
      }
      else if (size_str.back() == 'M') {
         return std::stoi(size_str) * 1024 * 1024;
      }

      return std::stoi(size_str);
   }

#elif defined(BENCH_MAC)

   inline size_t get_l1_cache_size()
   {
      size_t l1_cache_size = 0;
      size_t size = sizeof(l1_cache_size);

      if (sysctlbyname("hw.l1dcachesize", &l1_cache_size, &size, nullptr, 0) != 0) {
         std::cerr << "Failed to retrieve L1 cache size using sysctl!" << std::endl;
         return 0;
      }

      return l1_cache_size;
   }

#else

   inline size_t get_l1_cache_size()
   {
      std::cerr << "L1 cache size detection is not supported on this platform!" << std::endl;
      return 0;
   }
#endif

   struct cache_clearer
   {
      inline static size_t cache_line_size = 64;
      inline static size_t l1_cache_size{get_l1_cache_size()};

#if defined(BENCH_WIN)
      BENCH_ALWAYS_INLINE static void flush_cache(void* ptr, size_t size)
      {
         char* buffer = static_cast<char*>(ptr);
         for (size_t i = 0; i < size; i += cache_line_size) {
            _mm_clflush(buffer + i);
         }
         _mm_sfence();
      }

#elif defined(BENCH_LINUX) || defined(BENCH_MAC)
      BENCH_ALWAYS_INLINE static void flush_cache(void* ptr, size_t size)
      {
         char* buffer = static_cast<char*>(ptr);
#if defined(__x86_64__) || defined(__i386__)
         for (size_t i = 0; i < size; i += cache_line_size) {
            __builtin_ia32_clflush(buffer + i);
         }
         _mm_sfence();
#elif defined(__arm__) || defined(__aarch64__)
         for (size_t i = 0; i < size; i += cache_line_size) {
            asm volatile("dc cvac, %0" ::"r"(buffer + i) : "memory");
         }
         asm volatile("dsb sy" ::: "memory");
#else
         std::cerr << "Flush cache is not supported on this architecture!" << std::endl;
#endif
      }

#else
      BENCH_ALWAYS_INLINE static void flush_cache(void* ptr, size_t size)
      {
         (void)ptr;
         (void)size;
         std::cerr << "Flush cache is not supported on this platform!" << std::endl;
      }

#endif

      BENCH_ALWAYS_INLINE static void evict_l1_cache()
      {
         std::vector<char> evict_buffer(l1_cache_size + cache_line_size);
         for (size_t i = 0; i < evict_buffer.size(); i += cache_line_size) {
            evict_buffer[i] = static_cast<char>(i);
         }
         flush_cache(evict_buffer.data(), evict_buffer.size());
      }
   };

}
