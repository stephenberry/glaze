#include <malloc/malloc.h> // macOS malloc_size

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <new>
#include <string>
#include <vector>

// Global allocation tracker
static std::atomic<int64_t> g_allocated{0};

void* operator new(std::size_t size)
{
   void* p = std::malloc(size);
   if (!p) throw std::bad_alloc();
   g_allocated += static_cast<int64_t>(malloc_size(p));
   return p;
}

void operator delete(void* p) noexcept
{
   if (p) {
      g_allocated -= static_cast<int64_t>(malloc_size(p));
      std::free(p);
   }
}

void operator delete(void* p, std::size_t) noexcept
{
   if (p) {
      g_allocated -= static_cast<int64_t>(malloc_size(p));
      std::free(p);
   }
}

#include "glaze/containers/ordered_map.hpp"
#include "glaze/containers/ordered_small_map.hpp"

std::vector<std::string> generate_keys(size_t n)
{
   std::vector<std::string> base = {
      "id",        "name",    "email",   "age",      "active",  "role",   "created",   "updated",  "type",
      "status",    "title",   "body",    "url",      "path",    "method", "headers",   "params",   "query",
      "page",      "limit",   "offset",  "total",    "count",   "data",   "error",     "message",  "code",
      "timestamp", "version", "format",  "encoding", "length",  "width",  "height",    "color",    "font",
      "size",      "weight",  "opacity", "visible",  "enabled", "locked", "readonly",  "required", "optional",
      "default",   "min",     "max",     "pattern",  "prefix",  "suffix", "separator", "locale",   "timezone",
      "currency",  "country", "region",  "city",     "street",  "zip"};

   std::vector<std::string> keys;
   keys.reserve(n);
   for (size_t i = 0; i < n; ++i) {
      if (i < base.size()) {
         keys.push_back(base[i]);
      }
      else {
         keys.push_back("field_" + std::to_string(i));
      }
   }
   return keys;
}

int main()
{
   std::cout << "sizeof(std::string) = " << sizeof(std::string) << "\n";
   std::cout << "sizeof(glz::ordered_small_map<int>) = " << sizeof(glz::ordered_small_map<int>) << "\n";
   std::cout << "sizeof(glz::ordered_map<std::string, int>) = " << sizeof(glz::ordered_map<std::string, int>) << "\n";
   std::cout << "sizeof(std::map<std::string, int>) = " << sizeof(std::map<std::string, int>) << "\n";
   std::cout << "\n";

   // Pre-generate keys outside measurement
   std::vector<std::vector<std::string>> all_keys;
   for (size_t n : {8, 16, 32, 64, 128, 256}) {
      all_keys.push_back(generate_keys(n));
   }

   std::cout
      << "  n  | small_map bytes | small/entry | ordered_map bytes | ordered/entry | std::map bytes | std/entry | "
         "ordered/small | std/small\n";
   std::cout << "-----|-----------------|-------------|-------------------|---------------|----------------|-----------"
                "|---------------"
                "|----------\n";

   size_t idx = 0;
   for (size_t n : {8, 16, 32, 64, 128, 256}) {
      const auto& keys = all_keys[idx++];

      // Measure glz::ordered_small_map
      int64_t before = g_allocated.load();
      auto* small_map = new glz::ordered_small_map<int>();
      for (size_t i = 0; i < n; ++i) {
         (*small_map)[keys[i]] = static_cast<int>(i);
      }
      // Force index build for maps > threshold
      small_map->find("__nonexistent__");
      int64_t small_map_bytes = g_allocated.load() - before;
      delete small_map;

      // Measure glz::ordered_map
      before = g_allocated.load();
      auto* ordered_map = new glz::ordered_map<std::string, int>();
      for (size_t i = 0; i < n; ++i) {
         (*ordered_map)[keys[i]] = static_cast<int>(i);
      }
      int64_t ordered_map_bytes = g_allocated.load() - before;
      delete ordered_map;

      // Measure std::map
      before = g_allocated.load();
      auto* std_map = new std::map<std::string, int>();
      for (size_t i = 0; i < n; ++i) {
         (*std_map)[keys[i]] = static_cast<int>(i);
      }
      int64_t std_map_bytes = g_allocated.load() - before;
      delete std_map;

      double small_per = static_cast<double>(small_map_bytes) / n;
      double ordered_per = static_cast<double>(ordered_map_bytes) / n;
      double std_per = static_cast<double>(std_map_bytes) / n;
      double ordered_ratio = static_cast<double>(ordered_map_bytes) / small_map_bytes;
      double std_ratio = static_cast<double>(std_map_bytes) / small_map_bytes;

      printf("%4zu | %15lld | %11.1f | %17lld | %13.1f | %14lld | %9.1f | %13.2fx | %8.2fx\n", n, small_map_bytes,
             small_per, ordered_map_bytes, ordered_per, std_map_bytes, std_per, ordered_ratio, std_ratio);
   }

   return 0;
}
