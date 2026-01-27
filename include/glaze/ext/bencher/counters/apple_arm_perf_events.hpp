#pragma once

#include "bencher/config.hpp"

#if defined(BENCH_MAC)

#include <dlfcn.h>
#include <mach/mach_time.h>
#include <sys/kdebug.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

// -----------------------------------------------------------------------------
/** Customize error handling **/
// -----------------------------------------------------------------------------
namespace bencher
{
   enum class mac_error_code {
      success = 0,
      dlopen_kperf_failed,
      dlopen_kperfdata_failed,
      symbol_load_failed,
      permission_denied,
      cannot_load_pmc_database,
      cannot_create_config,
      cannot_force_counters,
      event_not_found,
      cannot_add_event,
      cannot_get_kpc_classes,
      cannot_get_kpc_count,
      cannot_get_kpc_map,
      cannot_get_kpc_registers,
      cannot_force_all_ctrs,
      cannot_set_kpc_config,
      cannot_set_counting,
      cannot_set_thread_counting,
      unknown
   };
}

template <>
struct std::is_error_condition_enum<bencher::mac_error_code> : true_type
{};

namespace bencher
{
   // A concrete error_category for mac/kperf errors
   struct mac_error_category : std::error_category
   {
      const char* name() const noexcept override { return "bencher::mac_error"; }

      std::string message(int c) const override
      {
         switch (static_cast<mac_error_code>(c)) {
         case mac_error_code::success:
            return "Success";
         case mac_error_code::dlopen_kperf_failed:
            return "Failed to load kperf.framework";
         case mac_error_code::dlopen_kperfdata_failed:
            return "Failed to load kperfdata.framework";
         case mac_error_code::symbol_load_failed:
            return "Failed to load a required symbol from framework";
         case mac_error_code::permission_denied:
            return "Warning (reduced metrics):\nPermission denied [kperf.framework] (requires root privileges)";
         case mac_error_code::cannot_load_pmc_database:
            return "Cannot load PMC database";
         case mac_error_code::cannot_create_config:
            return "Cannot create kpep config";
         case mac_error_code::cannot_force_counters:
            return "Failed to force counters";
         case mac_error_code::event_not_found:
            return "Requested event not found in the PMU database";
         case mac_error_code::cannot_add_event:
            return "Failed to add event to config";
         case mac_error_code::cannot_get_kpc_classes:
            return "Failed to retrieve KPC classes";
         case mac_error_code::cannot_get_kpc_count:
            return "Failed to retrieve KPC count";
         case mac_error_code::cannot_get_kpc_map:
            return "Failed to retrieve KPC map";
         case mac_error_code::cannot_get_kpc_registers:
            return "Failed to retrieve KPC registers";
         case mac_error_code::cannot_force_all_ctrs:
            return "Failed to force all KPC counters";
         case mac_error_code::cannot_set_kpc_config:
            return "Failed to set KPC configuration";
         case mac_error_code::cannot_set_counting:
            return "Failed to set counting";
         case mac_error_code::cannot_set_thread_counting:
            return "Failed to enable thread counting";
         default:
            return "Unknown mac error code";
         }
      }
   };

   inline const std::error_category& get_mac_error_category()
   {
      static mac_error_category category_instance;
      return category_instance;
   }

   // Helper to transform a mac_error_code into a std::error_condition
   inline std::error_condition make_error_condition(mac_error_code e) noexcept
   {
      return std::error_condition(static_cast<int>(e), get_mac_error_category());
   }
} // namespace bencher

namespace bencher
{
   // -----------------------------------------------------------------------------
   // KPC constants
   // -----------------------------------------------------------------------------

   inline constexpr int KPC_CLASS_FIXED = 0;
   inline constexpr int KPC_CLASS_CONFIGURABLE = 1;
   inline constexpr int KPC_CLASS_POWER = 2;
   inline constexpr int KPC_CLASS_RAWPMU = 3;

   inline constexpr uint32_t KPC_CLASS_FIXED_MASK = 1u << KPC_CLASS_FIXED;
   inline constexpr uint32_t KPC_CLASS_CONFIGURABLE_MASK = 1u << KPC_CLASS_CONFIGURABLE;
   inline constexpr uint32_t KPC_CLASS_POWER_MASK = 1u << KPC_CLASS_POWER;
   inline constexpr uint32_t KPC_CLASS_RAWPMU_MASK = 1u << KPC_CLASS_RAWPMU;

   inline constexpr size_t KPC_MAX_COUNTERS = 32;
   using kpc_config_t = uint64_t;

   // -----------------------------------------------------------------------------
   // Used kperf function pointers
   // -----------------------------------------------------------------------------

   inline int32_t (*kpc_force_all_ctrs_get)(int32_t* val_out) = nullptr;
   inline int32_t (*kpc_force_all_ctrs_set)(int32_t val) = nullptr;
   inline int32_t (*kpc_set_config)(uint32_t classes, kpc_config_t* config) = nullptr;
   inline int32_t (*kpc_set_counting)(uint32_t classes) = nullptr;
   inline int32_t (*kpc_set_thread_counting)(uint32_t classes) = nullptr;
   inline int32_t (*kpc_get_thread_counters)(uint32_t tid, uint32_t buf_count, uint64_t* buf) = nullptr;

   // -----------------------------------------------------------------------------
   // Used kperfdata (kpep) function pointers and structures
   // -----------------------------------------------------------------------------

   // kpep structures (from kperfdata)
   struct kpep_event
   {
      const char* name;
      const char* description;
      const char* errata;
      const char* alias;
      const char* fallback;
      uint32_t mask;
      uint8_t number;
      uint8_t umask;
      uint8_t reserved;
      uint8_t is_fixed;
   };

   struct kpep_db
   {
      const char* name;
      const char* cpu_id;
      const char* marketing_name;
      void* plist_data;
      void* event_map;
      kpep_event* event_arr;
      kpep_event** fixed_event_arr;
      void* alias_map;
      size_t reserved_1;
      size_t reserved_2;
      size_t reserved_3;
      size_t event_count;
      size_t alias_count;
      size_t fixed_counter_count;
      size_t config_counter_count;
      size_t power_counter_count;
      uint32_t archtecture;
      uint32_t fixed_counter_bits;
      uint32_t config_counter_bits;
      uint32_t power_counter_bits;
   };

   struct kpep_config
   {
      kpep_db* db;
      kpep_event** ev_arr;
      size_t* ev_map;
      size_t* ev_idx;
      uint32_t* flags;
      uint64_t* kpc_periods;
      size_t event_count;
      size_t counter_count;
      uint32_t classes;
      uint32_t config_counter;
      uint32_t power_counter;
      uint32_t reserved;
   };

   enum class kpep_config_error_code : int32_t {
      NONE = 0,
      INVALID_ARGUMENT,
      OUT_OF_MEMORY,
      IO,
      BUFFER_TOO_SMALL,
      CURRENT_SYSTEM_UNKNOWN,
      DB_PATH_INVALID,
      DB_NOT_FOUND,
      DB_ARCH_UNSUPPORTED,
      DB_VERSION_UNSUPPORTED,
      DB_CORRUPT,
      EVENT_NOT_FOUND,
      CONFLICTING_EVENTS,
      COUNTERS_NOT_FORCED,
      EVENT_UNAVAILABLE,
      ERRNO,
      MAX
   };

   inline const char* kpep_config_error_desc(int32_t code)
   {
      static const char* names[] = {"none",
                                    "invalid argument",
                                    "out of memory",
                                    "I/O",
                                    "buffer too small",
                                    "current system unknown",
                                    "database path invalid",
                                    "database not found",
                                    "database architecture unsupported",
                                    "database version unsupported",
                                    "database corrupt",
                                    "event not found",
                                    "conflicting events",
                                    "all counters must be forced",
                                    "event unavailable",
                                    "check errno"};
      return (code >= 0 && code < static_cast<int32_t>(kpep_config_error_code::MAX)) ? names[code] : "unknown error";
   }

   // kpep function pointers
   inline int32_t (*kpep_config_create)(kpep_db* db, kpep_config** cfg_ptr) = nullptr;
   inline int32_t (*kpep_config_force_counters)(kpep_config* cfg) = nullptr;
   inline int32_t (*kpep_config_add_event)(kpep_config* cfg, kpep_event** ev_ptr, uint32_t flag,
                                           uint32_t* err) = nullptr;
   inline int32_t (*kpep_config_kpc)(kpep_config* cfg, kpc_config_t* buf, size_t buf_size) = nullptr;
   inline int32_t (*kpep_config_kpc_count)(kpep_config* cfg, size_t* count_ptr) = nullptr;
   inline int32_t (*kpep_config_kpc_classes)(kpep_config* cfg, uint32_t* classes_ptr) = nullptr;
   inline int32_t (*kpep_config_kpc_map)(kpep_config* cfg, size_t* buf, size_t buf_size) = nullptr;
   inline int32_t (*kpep_db_create)(const char* name, kpep_db** db_ptr) = nullptr;
   inline int32_t (*kpep_db_event)(kpep_db* db, const char* name, kpep_event** ev_ptr) = nullptr;

   // -----------------------------------------------------------------------------
   // Dynamic library loading helper
   // -----------------------------------------------------------------------------

   struct LibSymbol
   {
      const char* name;
      void** impl;
   };

   template <size_t N>
   bool loadSymbols(void* handle, const std::array<LibSymbol, N>& symbols, std::string& errMsg)
   {
      for (const auto& sym : symbols) {
         *sym.impl = dlsym(handle, sym.name);
         if (!*sym.impl) {
            errMsg = "Failed to load function: ";
            errMsg += sym.name;
            return false;
         }
      }
      return true;
   }

   inline const char* lib_path_kperf = "/System/Library/PrivateFrameworks/kperf.framework/kperf";
   inline const char* lib_path_kperfdata = "/System/Library/PrivateFrameworks/kperfdata.framework/kperfdata";

   inline void* lib_handle_kperf = nullptr;
   inline void* lib_handle_kperfdata = nullptr;

   static inline std::array<LibSymbol, 6> kperf_symbols = {{
      {"kpc_force_all_ctrs_get", reinterpret_cast<void**>(&kpc_force_all_ctrs_get)},
      {"kpc_force_all_ctrs_set", reinterpret_cast<void**>(&kpc_force_all_ctrs_set)},
      {"kpc_set_config", reinterpret_cast<void**>(&kpc_set_config)},
      {"kpc_set_counting", reinterpret_cast<void**>(&kpc_set_counting)},
      {"kpc_set_thread_counting", reinterpret_cast<void**>(&kpc_set_thread_counting)},
      {"kpc_get_thread_counters", reinterpret_cast<void**>(&kpc_get_thread_counters)},
   }};

   static inline std::array<LibSymbol, 9> kperfdata_symbols = {{
      {"kpep_config_create", reinterpret_cast<void**>(&kpep_config_create)},
      {"kpep_config_force_counters", reinterpret_cast<void**>(&kpep_config_force_counters)},
      {"kpep_config_add_event", reinterpret_cast<void**>(&kpep_config_add_event)},
      {"kpep_config_kpc", reinterpret_cast<void**>(&kpep_config_kpc)},
      {"kpep_config_kpc_count", reinterpret_cast<void**>(&kpep_config_kpc_count)},
      {"kpep_config_kpc_classes", reinterpret_cast<void**>(&kpep_config_kpc_classes)},
      {"kpep_config_kpc_map", reinterpret_cast<void**>(&kpep_config_kpc_map)},
      {"kpep_db_create", reinterpret_cast<void**>(&kpep_db_create)},
      {"kpep_db_event", reinterpret_cast<void**>(&kpep_db_event)},
   }};

   inline std::error_condition lib_init()
   {
      static std::error_condition result = []() -> std::error_condition {
         lib_handle_kperf = dlopen(lib_path_kperf, RTLD_LAZY);
         if (!lib_handle_kperf) {
            return mac_error_code::dlopen_kperf_failed;
         }
         lib_handle_kperfdata = dlopen(lib_path_kperfdata, RTLD_LAZY);
         if (!lib_handle_kperfdata) {
            dlclose(lib_handle_kperf);
            lib_handle_kperf = nullptr;
            return mac_error_code::dlopen_kperfdata_failed;
         }

         std::string err_msg;
         if (!loadSymbols(lib_handle_kperf, kperf_symbols, err_msg)) {
            dlclose(lib_handle_kperf);
            dlclose(lib_handle_kperfdata);
            lib_handle_kperf = nullptr;
            lib_handle_kperfdata = nullptr;
            return mac_error_code::symbol_load_failed;
         }
         if (!loadSymbols(lib_handle_kperfdata, kperfdata_symbols, err_msg)) {
            dlclose(lib_handle_kperf);
            dlclose(lib_handle_kperfdata);
            lib_handle_kperf = nullptr;
            lib_handle_kperfdata = nullptr;
            return mac_error_code::symbol_load_failed;
         }
         return {}; // success
      }();
      return result;
   }

   // -----------------------------------------------------------------------------
   // Event aliases and performance counter setup
   // -----------------------------------------------------------------------------

   struct event_alias
   {
      const char* alias;
      std::array<const char*, 8> names{};
   };

   inline constexpr std::array<event_alias, 4> profile_events{
      {{"cycles", {"FIXED_CYCLES", "CPU_CLK_UNHALTED.THREAD", "CPU_CLK_UNHALTED.CORE", nullptr}},
       {"instructions", {"FIXED_INSTRUCTIONS", "INST_RETIRED.ANY", nullptr}},
       {"branches", {"INST_BRANCH", "BR_INST_RETIRED.ALL_BRANCHES", "INST_RETIRED.ANY", nullptr}},
       {"branch-misses",
        {"BRANCH_MISPRED_NONSPEC", "BRANCH_MISPREDICT", "BR_MISP_RETIRED.ALL_BRANCHES", "BR_INST_RETIRED.MISPRED",
         nullptr}}}};

   inline kpep_event* get_event(kpep_db* db, const event_alias& alias)
   {
      for (const auto name : alias.names) {
         if (!name) break;
         kpep_event* ev = nullptr;
         if (kpep_db_event(db, name, &ev) == 0) {
            return ev;
         }
      }
      return nullptr;
   }

   inline std::array<kpc_config_t, KPC_MAX_COUNTERS> regs{};
   inline std::array<size_t, KPC_MAX_COUNTERS> counter_map{};
   inline std::array<uint64_t, KPC_MAX_COUNTERS> counters_0{};
   inline std::array<uint64_t, KPC_MAX_COUNTERS> counters_1{};
   inline constexpr size_t ev_count = profile_events.size();

   inline std::error_condition setup_performance_counters()
   {
      static std::error_condition result = []() -> std::error_condition {
         // 1) Initialize library loading
         if (auto ec = lib_init()) {
            return ec;
         }

         // 2) Check permission (kpc requires special privileges)
         int32_t force_ctrs = 0;
         if (kpc_force_all_ctrs_get(&force_ctrs) != 0) {
            return mac_error_code::permission_denied;
         }

         // 3) Create database
         // Note: db and cfg are intentionally not freed. This is a one-time setup,
         // the kperfdata API is undocumented (no known free functions), and the
         // small allocation is reclaimed on process exit.
         kpep_db* db = nullptr;
         int32_t ret = kpep_db_create(nullptr, &db);
         if (ret != 0) {
            return mac_error_code::cannot_load_pmc_database;
         }

         // 4) Create config
         kpep_config* cfg = nullptr;
         ret = kpep_config_create(db, &cfg);
         if (ret != 0) {
            return mac_error_code::cannot_create_config;
         }

         // 5) Force counters
         ret = kpep_config_force_counters(cfg);
         if (ret != 0) {
            return mac_error_code::cannot_force_counters;
         }

         // 6) Try to map our known events
         std::array<kpep_event*, ev_count> ev_arr{};
         for (size_t i = 0; i < ev_count; i++) {
            ev_arr[i] = get_event(db, profile_events[i]);
            if (!ev_arr[i]) {
               return mac_error_code::event_not_found;
            }
         }

         for (size_t i = 0; i < ev_count; i++) {
            ret = kpep_config_add_event(cfg, &ev_arr[i], 0, nullptr);
            if (ret != 0) {
               return mac_error_code::cannot_add_event;
            }
         }

         // 7) Gather config details
         uint32_t classes = 0;
         size_t reg_count = 0;
         ret = kpep_config_kpc_classes(cfg, &classes);
         if (ret != 0) {
            return mac_error_code::cannot_get_kpc_classes;
         }

         ret = kpep_config_kpc_count(cfg, &reg_count);
         if (ret != 0) {
            return mac_error_code::cannot_get_kpc_count;
         }

         ret = kpep_config_kpc_map(cfg, counter_map.data(), sizeof(counter_map));
         if (ret != 0) {
            return mac_error_code::cannot_get_kpc_map;
         }

         ret = kpep_config_kpc(cfg, regs.data(), sizeof(regs));
         if (ret != 0) {
            return mac_error_code::cannot_get_kpc_registers;
         }

         // 8) Force all counters
         if (kpc_force_all_ctrs_set(1) != 0) {
            return mac_error_code::cannot_force_all_ctrs;
         }

         // 9) Set config if we have configurables
         if ((classes & KPC_CLASS_CONFIGURABLE_MASK) && reg_count) {
            if (kpc_set_config(classes, regs.data()) != 0) {
               return mac_error_code::cannot_set_kpc_config;
            }
         }

         // 10) Start counting
         if (kpc_set_counting(classes) != 0) {
            return mac_error_code::cannot_set_counting;
         }
         if (kpc_set_thread_counting(classes) != 0) {
            return mac_error_code::cannot_set_thread_counting;
         }

         return {}; // success
      }();
      return result;
   }

   // -----------------------------------------------------------------------------
   // Performance counters retrieval
   // -----------------------------------------------------------------------------

   struct performance_counters
   {
      double cycles{};
      double branches{};
      double missed_branches{};
      double instructions{};
   };

   inline constexpr performance_counters operator-(const performance_counters& a,
                                                   const performance_counters& b) noexcept
   {
      return performance_counters(a.cycles - b.cycles, a.branches - b.branches, a.missed_branches - b.missed_branches,
                                  a.instructions - b.instructions);
   }

   inline performance_counters get_counters()
   {
      // Attempt retrieving counters. If it fails, we return something placeholder.
      // (User can detect if data is valid or not.)
      if (kpc_get_thread_counters(0, KPC_MAX_COUNTERS, counters_0.data()) != 0) {
         // Return dummy counters
         return {0, 0, 0, 0};
      }
      // Indices:
      //   0 -> cycles,
      //   1 -> instructions,
      //   2 -> branches,
      //   3 -> branch misses,
      // as per how we added them in setup.
      return {static_cast<double>(counters_0[counter_map[0]]), static_cast<double>(counters_0[counter_map[2]]),
              static_cast<double>(counters_0[counter_map[3]]), static_cast<double>(counters_0[counter_map[1]])};
   }

   // Example collector for benchmarking
   template <class EventCount>
   struct event_collector_type
   {
      performance_counters diff{};

      event_collector_type() { setup_performance_counters(); }

      [[nodiscard]] std::error_condition error()
      {
         return setup_performance_counters();
      }

      template <class Function, class... Args>
      [[nodiscard]] std::error_condition start(EventCount& count, Function&& function, Args&&... args)
      {
         using namespace std::chrono;
         if (not error()) {
            diff = get_counters();
         }
         auto start_clock = steady_clock::now();
         if constexpr (std::is_void_v<std::invoke_result_t<Function, Args...>>) {
            std::forward<Function>(function)(std::forward<Args>(args)...);
            count.bytes_processed = 0;
         }
         else {
            count.bytes_processed = std::forward<Function>(function)(std::forward<Args>(args)...);
         }
         auto end_clock = steady_clock::now();

         if (not error()) {
            performance_counters end = get_counters();
            diff = end - diff;
            count.cycles.emplace(diff.cycles);
            count.instructions.emplace(diff.instructions);
            count.branches.emplace(diff.branches);
            count.missed_branches.emplace(diff.missed_branches);
         }
         count.elapsed = end_clock - start_clock;
         return error();
      }
   };
}

#endif // BENCH_MAC
