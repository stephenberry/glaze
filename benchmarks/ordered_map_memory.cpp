#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <vector>

#include <malloc/malloc.h> // macOS malloc_size

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

#include "glaze/containers/ordered_small_map.hpp"
#include "tsl/ordered_map.h"

std::vector<std::string> generate_keys(size_t n)
{
   std::vector<std::string> base = {"id",        "name",    "email",   "age",       "active",  "role",
                                     "created",   "updated", "type",    "status",    "title",   "body",
                                     "url",       "path",    "method",  "headers",   "params",  "query",
                                     "page",      "limit",   "offset",  "total",     "count",   "data",
                                     "error",     "message", "code",    "timestamp", "version", "format",
                                     "encoding",  "length",  "width",   "height",    "color",   "font",
                                     "size",      "weight",  "opacity", "visible",   "enabled", "locked",
                                     "readonly",  "required","optional","default",   "min",     "max",
                                     "pattern",   "prefix",  "suffix",  "separator", "locale",  "timezone",
                                     "currency",  "country", "region",  "city",      "street",  "zip"};

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
   std::cout << "sizeof(tsl::ordered_map<std::string, int>) = " << sizeof(tsl::ordered_map<std::string, int>) << "\n";
   std::cout << "\n";

   // Pre-generate keys outside measurement
   std::vector<std::vector<std::string>> all_keys;
   for (size_t n : {8, 16, 32, 64, 128, 256}) {
      all_keys.push_back(generate_keys(n));
   }

   std::cout << "  n  | glz bytes | glz/entry | tsl bytes | tsl/entry | ratio (tsl/glz)\n";
   std::cout << "-----|-----------|-----------|-----------|-----------|----------------\n";

   size_t idx = 0;
   for (size_t n : {8, 16, 32, 64, 128, 256}) {
      const auto& keys = all_keys[idx++];

      // Measure glz::ordered_small_map
      int64_t before = g_allocated.load();
      auto* glz_map = new glz::ordered_small_map<int>();
      for (size_t i = 0; i < n; ++i) {
         (*glz_map)[keys[i]] = static_cast<int>(i);
      }
      // Force index build for maps > threshold
      glz_map->find("__nonexistent__");
      int64_t glz_bytes = g_allocated.load() - before;
      delete glz_map;

      // Measure tsl::ordered_map
      before = g_allocated.load();
      auto* tsl_map = new tsl::ordered_map<std::string, int>();
      for (size_t i = 0; i < n; ++i) {
         (*tsl_map)[keys[i]] = static_cast<int>(i);
      }
      int64_t tsl_bytes = g_allocated.load() - before;
      delete tsl_map;

      double glz_per = static_cast<double>(glz_bytes) / n;
      double tsl_per = static_cast<double>(tsl_bytes) / n;
      double ratio = static_cast<double>(tsl_bytes) / glz_bytes;

      printf("%4zu | %9lld | %9.1f | %9lld | %9.1f | %14.2fx\n", n, glz_bytes, glz_per, tsl_bytes, tsl_per, ratio);
   }

   return 0;
}
