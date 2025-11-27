// Glaze Library
// For the license information refer to glaze.hpp

// Pure C plugin interface for ABI stability
// Plugins export these symbols with C linkage
// Request/response data uses REPE binary format

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Current plugin interface version - increment when ABI changes
#define REPE_PLUGIN_INTERFACE_VERSION 1

// ABI-stable buffer for request/response data
typedef struct {
    const char* data;
    uint64_t size;
} repe_buffer;

// Result codes for plugin operations
typedef enum {
    REPE_OK = 0,
    REPE_ERROR_INIT_FAILED = 1,
    REPE_ERROR_INVALID_CONFIG = 2,
    REPE_ERROR_ALREADY_INITIALIZED = 3
} repe_result;

// ---------------------------------------------------------------------------
// Plugin Interface Version (required)
// ---------------------------------------------------------------------------

// Returns the plugin interface version this plugin was built against.
// Hosts should check: repe_plugin_interface_version() == REPE_PLUGIN_INTERFACE_VERSION
// If versions don't match, the host should refuse to load the plugin.
uint32_t repe_plugin_interface_version(void);

// ---------------------------------------------------------------------------
// Plugin Identification (required)
// ---------------------------------------------------------------------------

// Returns a human-readable plugin name (e.g., "calculator")
const char* repe_plugin_name(void);

// Returns the plugin version string (e.g., "1.0.0")
const char* repe_plugin_version(void);

// Returns the root path for request routing (e.g., "/calculator")
// All methods in this plugin should be under this path
const char* repe_plugin_root_path(void);

// ---------------------------------------------------------------------------
// Plugin Lifecycle (optional - may be NULL)
// ---------------------------------------------------------------------------

// Initialize the plugin with optional configuration.
// Called once by the host before any calls to repe_plugin_call.
// config/config_size: Optional JSON or REPE-formatted configuration (may be NULL/0)
// Returns: REPE_OK on success, error code on failure
// If not exported, the host assumes initialization is handled lazily.
repe_result repe_plugin_init(const char* config, uint64_t config_size);

// Shutdown the plugin and release all resources.
// Called once by the host before unloading the plugin.
// After this call, no further calls will be made to the plugin.
// If not exported, the host assumes no cleanup is needed.
void repe_plugin_shutdown(void);

// ---------------------------------------------------------------------------
// Request Processing (required)
// ---------------------------------------------------------------------------

// Process a REPE request and return a REPE response.
//
// Thread Safety: This function may be called concurrently from multiple threads.
// Each thread maintains its own response buffer.
//
// Buffer Lifetime: The returned buffer is valid only until the next call to
// repe_plugin_call on the SAME thread. Callers must copy the data if they
// need to retain it beyond the next call.
//
// WARNING: Do not store the returned buffer pointer for later use.
// The memory will be overwritten by subsequent calls.
repe_buffer repe_plugin_call(const char* request, uint64_t request_size);

#ifdef __cplusplus
}
#endif
