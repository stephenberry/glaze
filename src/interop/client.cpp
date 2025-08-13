#include "glaze/interop/client.hpp"
#include <sstream>
#include <algorithm>

namespace glz::interop {

// MemberInfo implementation
bool member_info::is_function() const {
    return info_ && info_->kind == 1;
}

const glz_type_descriptor* member_info::type_descriptor() const {
    return info_ ? info_->type : nullptr;
}

// TypeInfo implementation
type_info::type_info(const glz_type_info* info, interop_library* lib) 
    : info_(info), library_(lib) {
    if (info_) {
        members_.reserve(info_->member_count);
        for (size_t i = 0; i < info_->member_count; ++i) {
            members_.emplace_back(&info_->members[i], info_->members[i].name);
        }
    }
}

std::string_view type_info::name() const {
    return info_ ? std::string_view(info_->name) : std::string_view{};
}

size_t type_info::size() const {
    return info_ ? info_->size : 0;
}

size_t type_info::member_count() const {
    return info_ ? info_->member_count : 0;
}

// Instance implementation
instance::~instance() {
    if (owned_ && ptr_ && library_ && library_->funcs_.destroy_instance) {
        library_->funcs_.destroy_instance(type_->name().data(), ptr_);
    }
}

void* instance::get_member_ptr(const member_info& member) {
    if (!library_->funcs_.get_member_ptr) {
        throw interop_exception("get_member_ptr function not available");
    }
    return library_->funcs_.get_member_ptr(ptr_, member.info_);
}

// InteropLibrary implementation
void interop_library::load(const std::string& library_path) {
    if (handle_) {
        close();
    }
    
    path_ = library_path;
    handle_ = LOAD_LIBRARY(library_path.c_str());
    
    if (!handle_) {
#ifdef _WIN32
        DWORD error = GetLastError();
        char buffer[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, 0, buffer, sizeof(buffer), nullptr);
        throw interop_exception("Failed to load library: " + library_path + " - " + buffer);
#else
        const char* error = dlerror();
        throw interop_exception("Failed to load library: " + library_path + " - " + (error ? error : "unknown error"));
#endif
    }
    
    // Load all the required functions
    try {
        // Type info functions
        funcs_.get_type_info = load_function<decltype(funcs_.get_type_info)>("glz_get_type_info");
        funcs_.get_type_info_by_hash = load_function<decltype(funcs_.get_type_info_by_hash)>("glz_get_type_info_by_hash");
        
        // Instance management
        funcs_.create_instance = load_function<decltype(funcs_.create_instance)>("glz_create_instance");
        funcs_.destroy_instance = load_function<decltype(funcs_.destroy_instance)>("glz_destroy_instance");
        funcs_.get_instance = load_function<decltype(funcs_.get_instance)>("glz_get_instance");
        funcs_.get_instance_type = load_function<decltype(funcs_.get_instance_type)>("glz_get_instance_type");
        
        // Member access
        funcs_.get_member_ptr = load_function<decltype(funcs_.get_member_ptr)>("glz_get_member_ptr");
        funcs_.call_member_function_with_type = load_function<decltype(funcs_.call_member_function_with_type)>("glz_call_member_function_with_type");
        
        // String operations (optional)
        try {
            funcs_.string_view = load_function<decltype(funcs_.string_view)>("glz_string_view");
            funcs_.string_set = load_function<decltype(funcs_.string_set)>("glz_string_set");
            funcs_.string_c_str = load_function<decltype(funcs_.string_c_str)>("glz_string_c_str");
            funcs_.string_size = load_function<decltype(funcs_.string_size)>("glz_string_size");
            funcs_.create_string = load_function<decltype(funcs_.create_string)>("glz_create_string");
            funcs_.destroy_string = load_function<decltype(funcs_.destroy_string)>("glz_destroy_string");
        } catch (...) {
            // String functions are optional
        }
        
        // Vector operations (optional)
        try {
            funcs_.vector_view = load_function<decltype(funcs_.vector_view)>("glz_vector_view");
            funcs_.vector_resize = load_function<decltype(funcs_.vector_resize)>("glz_vector_resize");
            funcs_.vector_push_back = load_function<decltype(funcs_.vector_push_back)>("glz_vector_push_back");
            funcs_.create_vector = load_function<decltype(funcs_.create_vector)>("glz_create_vector");
            funcs_.destroy_vector = load_function<decltype(funcs_.destroy_vector)>("glz_destroy_vector");
        } catch (...) {
            // Vector functions are optional
        }
        
        // Optional operations (optional)
        try {
            funcs_.optional_has_value = load_function<decltype(funcs_.optional_has_value)>("glz_optional_has_value");
            funcs_.optional_get_value = load_function<decltype(funcs_.optional_get_value)>("glz_optional_get_value");
            funcs_.optional_set_value = load_function<decltype(funcs_.optional_set_value)>("glz_optional_set_value");
            funcs_.optional_reset = load_function<decltype(funcs_.optional_reset)>("glz_optional_reset");
        } catch (...) {
            // Optional functions are optional
        }
        
    } catch (const std::exception& e) {
        close();
        throw;
    }
}

void interop_library::close() {
    if (handle_) {
        // Clear type cache first
        type_cache_.clear();
        
        CLOSE_LIBRARY(handle_);
        handle_ = nullptr;
        path_.clear();
        funcs_ = {};
    }
}

std::shared_ptr<type_info> interop_library::get_type(std::string_view type_name) {
    if (!funcs_.get_type_info) {
        throw interop_exception("Library not loaded or get_type_info not available");
    }
    
    // Check cache first
    auto it = type_cache_.find(std::string(type_name));
    if (it != type_cache_.end()) {
        return it->second;
    }
    
    // Get type info from library
    auto* info = funcs_.get_type_info(std::string(type_name).c_str());
    if (!info) {
        throw interop_exception("Type not found: " + std::string(type_name));
    }
    
    // Create and cache TypeInfo wrapper
    auto type_info_ptr = std::make_shared<type_info>(info, this);
    type_cache_[std::string(type_name)] = type_info_ptr;
    return type_info_ptr;
}

std::unique_ptr<instance> interop_library::create_instance(std::string_view type_name) {
    if (!funcs_.create_instance) {
        throw interop_exception("Library not loaded or create_instance not available");
    }
    
    // Get type info first
    auto type_info_obj = get_type(type_name);
    
    // Create instance
    void* ptr = funcs_.create_instance(std::string(type_name).c_str());
    if (!ptr) {
        throw interop_exception("Failed to create instance of type: " + std::string(type_name));
    }
    
    return std::make_unique<instance>(ptr, type_info_obj, this, true);
}

std::unique_ptr<instance> interop_library::get_instance(std::string_view instance_name) {
    if (!funcs_.get_instance || !funcs_.get_instance_type) {
        throw interop_exception("Library not loaded or instance functions not available");
    }
    
    // Get the instance pointer
    void* ptr = funcs_.get_instance(std::string(instance_name).c_str());
    if (!ptr) {
        throw interop_exception("Instance not found: " + std::string(instance_name));
    }
    
    // Get the type name
    const char* type_name = funcs_.get_instance_type(std::string(instance_name).c_str());
    if (!type_name) {
        throw interop_exception("Could not get type for instance: " + std::string(instance_name));
    }
    
    // Get type info
    auto type_info_obj = get_type(type_name);
    
    // Return non-owning instance wrapper
    return std::make_unique<instance>(ptr, type_info_obj, this, false);
}

std::vector<std::string> interop_library::list_types() const {
    // This would require an additional C API function to enumerate types
    // For now, return empty vector
    // TODO: Add glz_list_types() to C API
    return {};
}

std::vector<std::string> interop_library::list_instances() const {
    // This would require an additional C API function to enumerate instances
    // For now, return empty vector
    // TODO: Add glz_list_instances() to C API
    return {};
}

} // namespace glz::interop