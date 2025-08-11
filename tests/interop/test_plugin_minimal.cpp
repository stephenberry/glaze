// Ultra-minimal plugin that doesn't depend on any Glaze headers
// This is purely for testing dynamic library loading

struct MinimalPlugin {
    int value = 42;
    
    int multiply(int x) {
        return value * x;
    }
};

// Simple C interface functions for testing dynamic loading
extern "C" {
    // Create an instance
    void* create_minimal_plugin() {
        return new MinimalPlugin();
    }
    
    // Delete an instance
    void delete_minimal_plugin(void* ptr) {
        delete static_cast<MinimalPlugin*>(ptr);
    }
    
    // Get value
    int get_value(void* ptr) {
        return static_cast<MinimalPlugin*>(ptr)->value;
    }
    
    // Set value
    void set_value(void* ptr, int val) {
        static_cast<MinimalPlugin*>(ptr)->value = val;
    }
    
    // Call multiply
    int call_multiply(void* ptr, int x) {
        return static_cast<MinimalPlugin*>(ptr)->multiply(x);
    }
    
    // Plugin info
    const char* plugin_name() {
        return "MinimalPlugin";
    }
    
    const char* plugin_version() {
        return "1.0.0";
    }
}