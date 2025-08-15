// Minimal plugin that doesn't use TypeRegistry to avoid linking issues
#include "glaze/interop/interop.hpp"

struct SimpleTestPlugin
{
   int value = 42;

   int multiply(int x) { return value * x; }
};

// Simple C interface functions for testing dynamic loading
extern "C" {
// Create an instance
GLZ_EXPORT void* create_simple_plugin() { return new SimpleTestPlugin(); }

// Delete an instance
GLZ_EXPORT void delete_simple_plugin(void* ptr) { delete static_cast<SimpleTestPlugin*>(ptr); }

// Get value
GLZ_EXPORT int get_value(void* ptr) { return static_cast<SimpleTestPlugin*>(ptr)->value; }

// Set value
GLZ_EXPORT void set_value(void* ptr, int val) { static_cast<SimpleTestPlugin*>(ptr)->value = val; }

// Call multiply
GLZ_EXPORT int call_multiply(void* ptr, int x) { return static_cast<SimpleTestPlugin*>(ptr)->multiply(x); }

// Plugin info
GLZ_EXPORT const char* plugin_name() { return "SimpleTestPlugin"; }

GLZ_EXPORT const char* plugin_version() { return "1.0.0"; }
}