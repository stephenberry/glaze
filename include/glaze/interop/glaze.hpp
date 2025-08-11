#pragma once

// High-level C++ interface that mirrors Julia's Glaze.jl module
// Provides a clean, intuitive API for working with Glaze types dynamically

#include "glaze/interop/interop.hpp"
#include "glaze/interop/client.hpp"
#include <any>
#include <typeindex>
#include <variant>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <complex>
#include <future>

namespace glz {

// Forward declarations
class itype;
class iinstance;
class ifield;
class imethod;
class ivalue;

// Dynamic value type that can hold references to any Glaze-supported type
class ivalue {
public:
    // Store pointers for direct memory access, with type information
    using value_type = std::variant<
        std::monostate,  // null/nothing
        bool*,
        int8_t*, int16_t*, int32_t*, int64_t*,
        uint8_t*, uint16_t*, uint32_t*, uint64_t*,
        float*, double*,
        std::string*,
        std::vector<ivalue>*,  // Arrays of values
        std::unordered_map<std::string, ivalue>*,  // Objects
        std::complex<float>*, std::complex<double>*,
        std::shared_ptr<iinstance>,  // Reference to an instance
        std::shared_future<ivalue>*,  // Async results
        // For owning temporary values (e.g., computed results)
        std::shared_ptr<std::any>  // Owned value with type erasure
    >;
    
private:
    value_type value_;
    bool owned_ = false;  // Whether this value owns the data
    
    // Helper to create owned values
    template<typename T>
    void make_owned(T&& val) {
        auto owned_ptr = std::make_shared<std::any>(std::forward<T>(val));
        value_ = owned_ptr;
        owned_ = true;
    }
    
public:
    ivalue() : value_(std::monostate{}) {}
    
    // Constructor for references (non-owning)
    template<typename T>
    explicit ivalue(T* ptr) : owned_(false) {
        if constexpr (std::is_same_v<T, bool>) value_ = static_cast<bool*>(ptr);
        else if constexpr (std::is_same_v<T, int8_t>) value_ = static_cast<int8_t*>(ptr);
        else if constexpr (std::is_same_v<T, int16_t>) value_ = static_cast<int16_t*>(ptr);
        else if constexpr (std::is_same_v<T, int32_t>) value_ = static_cast<int32_t*>(ptr);
        else if constexpr (std::is_same_v<T, int64_t>) value_ = static_cast<int64_t*>(ptr);
        else if constexpr (std::is_same_v<T, uint8_t>) value_ = static_cast<uint8_t*>(ptr);
        else if constexpr (std::is_same_v<T, uint16_t>) value_ = static_cast<uint16_t*>(ptr);
        else if constexpr (std::is_same_v<T, uint32_t>) value_ = static_cast<uint32_t*>(ptr);
        else if constexpr (std::is_same_v<T, uint64_t>) value_ = static_cast<uint64_t*>(ptr);
        else if constexpr (std::is_same_v<T, float>) value_ = static_cast<float*>(ptr);
        else if constexpr (std::is_same_v<T, double>) value_ = static_cast<double*>(ptr);
        else if constexpr (std::is_same_v<T, std::string>) value_ = static_cast<std::string*>(ptr);
        else if constexpr (std::is_same_v<T, std::vector<ivalue>>) value_ = static_cast<std::vector<ivalue>*>(ptr);
        else if constexpr (std::is_same_v<T, std::unordered_map<std::string, ivalue>>) 
            value_ = static_cast<std::unordered_map<std::string, ivalue>*>(ptr);
        else if constexpr (std::is_same_v<T, std::complex<float>>) value_ = static_cast<std::complex<float>*>(ptr);
        else if constexpr (std::is_same_v<T, std::complex<double>>) value_ = static_cast<std::complex<double>*>(ptr);
        else {
            // For other types, store as owned std::any
            make_owned(std::move(*ptr));
        }
    }
    
    // Constructor for owned values (makes a copy)
    template<typename T>
    static ivalue make_owned_value(T&& val) {
        ivalue v;
        v.make_owned(std::forward<T>(val));
        return v;
    }
    
    // Reference wrapper constructor for safer reference semantics
    template<typename T>
    explicit ivalue(std::reference_wrapper<T> ref) : ivalue(&ref.get()) {}
    
    // Instance wrapper constructor
    explicit ivalue(std::shared_ptr<iinstance> instance) : value_(instance) {}
    
    // Type checking
    bool is_null() const { return std::holds_alternative<std::monostate>(value_); }
    bool is_bool() const { return std::holds_alternative<bool*>(value_); }
    bool is_int() const { 
        return std::holds_alternative<int8_t*>(value_) ||
               std::holds_alternative<int16_t*>(value_) ||
               std::holds_alternative<int32_t*>(value_) || 
               std::holds_alternative<int64_t*>(value_) ||
               std::holds_alternative<uint8_t*>(value_) ||
               std::holds_alternative<uint16_t*>(value_) ||
               std::holds_alternative<uint32_t*>(value_) ||
               std::holds_alternative<uint64_t*>(value_);
    }
    bool is_float() const { 
        return std::holds_alternative<float*>(value_) || 
               std::holds_alternative<double*>(value_);
    }
    bool is_string() const { return std::holds_alternative<std::string*>(value_); }
    bool is_array() const { return std::holds_alternative<std::vector<ivalue>*>(value_); }
    bool is_object() const { 
        return std::holds_alternative<std::unordered_map<std::string, ivalue>*>(value_);
    }
    bool is_instance() const { 
        return std::holds_alternative<std::shared_ptr<iinstance>>(value_);
    }
    bool is_future() const { 
        return std::holds_alternative<std::shared_future<ivalue>*>(value_);
    }
    bool is_owned() const { return owned_; }
    
    // value extraction - get pointer to the stored type
    template<typename T>
    T* get_ptr() {
        if constexpr (std::is_same_v<T, bool>) return std::get_if<bool*>(&value_) ? *std::get_if<bool*>(&value_) : nullptr;
        else if constexpr (std::is_same_v<T, int32_t>) return std::get_if<int32_t*>(&value_) ? *std::get_if<int32_t*>(&value_) : nullptr;
        else if constexpr (std::is_same_v<T, int64_t>) return std::get_if<int64_t*>(&value_) ? *std::get_if<int64_t*>(&value_) : nullptr;
        else if constexpr (std::is_same_v<T, float>) return std::get_if<float*>(&value_) ? *std::get_if<float*>(&value_) : nullptr;
        else if constexpr (std::is_same_v<T, double>) return std::get_if<double*>(&value_) ? *std::get_if<double*>(&value_) : nullptr;
        else if constexpr (std::is_same_v<T, std::string>) return std::get_if<std::string*>(&value_) ? *std::get_if<std::string*>(&value_) : nullptr;
        else return nullptr;
    }
    
    template<typename T>
    const T* get_ptr() const {
        return const_cast<ivalue*>(this)->get_ptr<T>();
    }
    
    // Get reference to the value (throws if wrong type or null pointer)
    template<typename T>
    T& get_ref() {
        T* ptr = get_ptr<T>();
        if (!ptr) throw std::runtime_error("value is not of requested type or is null");
        return *ptr;
    }
    
    template<typename T>
    const T& get_ref() const {
        const T* ptr = get_ptr<T>();
        if (!ptr) throw std::runtime_error("value is not of requested type or is null");
        return *ptr;
    }
    
    // Convenience accessors that dereference the pointers
    bool as_bool() const;
    int64_t as_int() const;
    double as_float() const;
    std::string as_string() const;
    std::vector<ivalue>& as_array();
    const std::vector<ivalue>& as_array() const;
    
    // Object field access (like Julia's dot notation)
    ivalue& operator[](const std::string& key);
    const ivalue& operator[](const std::string& key) const;
    
    // Array element access
    ivalue& operator[](size_t index);
    const ivalue& operator[](size_t index) const;
    
    // JSON serialization
    std::string to_json() const;
    static ivalue from_json(const std::string& json);
};

// Represents a field in a type
class ifield {
    friend class itype;
    friend class iinstance;
    
private:
    std::string name_;
    const glz_member_info* info_;
    std::shared_ptr<itype> type_;
    
public:
    ifield() : name_(), info_(nullptr), type_(nullptr) {}
    ifield(std::string_view name, const glz_member_info* info, std::shared_ptr<itype> type = nullptr)
        : name_(name), info_(info), type_(type) {}
    const std::string& name() const { return name_; }
    bool is_function() const { return info_ && info_->kind == 1; }
    std::shared_ptr<itype> get_type() const { return type_; }
};

// Represents a method in a type
class imethod {
    friend class itype;
    friend class iinstance;
    
private:
    std::string name_;
    const glz_member_info* info_;
    std::vector<std::shared_ptr<itype>> param_types_;
    std::shared_ptr<itype> return_type_;
    
public:
    imethod() : name_(), info_(nullptr) {}
    imethod(std::string_view name, const glz_member_info* info = nullptr)
        : name_(name), info_(info) {}
    const std::string& name() const { return name_; }
    size_t param_count() const { return param_types_.size(); }
    const std::vector<std::shared_ptr<itype>>& param_types() const { return param_types_; }
    std::shared_ptr<itype> return_type() const { return return_type_; }
};

// Represents a Glaze type
class itype : public std::enable_shared_from_this<itype> {
    friend class iinstance;
    friend class iglaze;
    
private:
    std::string name_;
    size_t size_;
    std::unordered_map<std::string, ifield> fields_;
    std::unordered_map<std::string, imethod> methods_;
    const glz_type_info* info_;
    
public:
    // Constructor - types are created through iglaze class
    itype(const glz_type_info* info);
    const std::string& name() const { return name_; }
    size_t size() const { return size_; }
    
    // field access
    bool has_field(const std::string& name) const {
        return fields_.find(name) != fields_.end();
    }
    
    const ifield& get_field(const std::string& name) const {
        auto it = fields_.find(name);
        if (it == fields_.end()) {
            throw std::runtime_error("field not found: " + name);
        }
        return it->second;
    }
    
    const std::unordered_map<std::string, ifield>& fields() const { return fields_; }
    
    // method access
    bool has_method(const std::string& name) const {
        return methods_.find(name) != methods_.end();
    }
    
    const imethod& get_method(const std::string& name) const {
        auto it = methods_.find(name);
        if (it == methods_.end()) {
            throw std::runtime_error("method not found: " + name);
        }
        return it->second;
    }
    
    const std::unordered_map<std::string, imethod>& methods() const { return methods_; }
    
    // Create an instance of this type
    std::shared_ptr<iinstance> create_instance();
};

// Represents an instance of a Glaze type
class iinstance : public std::enable_shared_from_this<iinstance> {
    friend class itype;
    friend class iglaze;
    
private:
    void* ptr_;
    std::shared_ptr<itype> type_;
    bool owned_;
    
public:
    iinstance(void* ptr, std::shared_ptr<itype> type, bool owned = true)
        : ptr_(ptr), type_(type), owned_(owned) {}
    ~iinstance();
    
    std::shared_ptr<itype> get_type() const { return type_; }
    void* ptr() { return ptr_; }
    const void* ptr() const { return ptr_; }
    
    // field access - primary interface using operator[]
    // Usage: instance["field_name"] returns a value pointing to the field
    ivalue operator[](const std::string& field_name) {
        return getfield(field_name);
    }
    
    ivalue operator[](const std::string& field_name) const {
        return getfield(field_name);
    }
    
    // Helper class to enable arrow operator for shared_ptr<iinstance>
    class field_accessor {
        iinstance* instance_;
    public:
        explicit field_accessor(iinstance* inst) : instance_(inst) {}
        ivalue operator[](const std::string& field_name) {
            return instance_->getfield(field_name);
        }
    };
    
    // Enable direct field access on shared_ptr<iinstance>
    field_accessor fields() { return field_accessor(this); }
    
    // Lower-level field access (used by operator[])
    ivalue getfield(const std::string& field_name) const;
    void setfield(const std::string& field_name, const ivalue& val);
    
    // method invocation (like Julia's method calls)
    template<typename... Args>
    ivalue call(const std::string& method_name, Args&&... args);
    
    // Convert to JSON
    std::string to_json() const;
    
    // Create from JSON
    static std::shared_ptr<iinstance> from_json(const std::string& json, std::shared_ptr<itype> type);
};

// Main Glaze interop class (similar to Julia's Glaze module)
class iglaze {
private:
    // type registry
    static std::unordered_map<std::string, std::shared_ptr<itype>> type_registry_;
    
    // instance registry for global instances
    static std::unordered_map<std::string, std::shared_ptr<iinstance>> instance_registry_;
    
    // Library handles for dynamically loaded libraries
    static std::vector<std::unique_ptr<interop::InteropLibrary>> loaded_libraries_;
    
public:
    // Register a C++ type (like Julia's register_type)
    template<typename T>
    static std::shared_ptr<itype> register_type(const std::string& name) {
        glz::register_type<T>(name);
        return get_type(name);
    }
    
    // Get a registered type
    static std::shared_ptr<itype> get_type(const std::string& name);
    
    // Check if a type is registered
    static bool has_type(const std::string& name) {
        return type_registry_.find(name) != type_registry_.end();
    }
    
    // List all registered types
    static std::vector<std::string> list_types();
    
    // Create an instance of a type
    static std::shared_ptr<iinstance> create_instance(const std::string& type_name);
    
    // Register a global instance
    template<typename T>
    static void register_instance(const std::string& name, const std::string& type_name, T& inst) {
        glz::register_instance(name, type_name, inst);
        void* ptr = glz_get_instance(name.c_str());
        auto t = get_type(type_name);
        instance_registry_[name] = std::make_shared<iinstance>(ptr, t, false);
    }
    
    // Get a global instance
    static std::shared_ptr<iinstance> get_instance(const std::string& name);
    
    // List all global instances
    static std::vector<std::string> list_instances();
    
    // Load a shared library with Glaze types
    static void load_library(const std::string& path);
    
    // Unload all libraries
    static void unload_all_libraries();
    
    // JSON serialization helpers
    template<typename T>
    static std::string to_json(const T& obj) {
        return glz::write_json(obj).value_or("");
    }
    
    template<typename T>
    static T from_json(const std::string& json) {
        T obj;
        glz::read_json(obj, json);
        return obj;
    }
    
    // Reflection utilities
    template<typename T>
    static std::vector<std::string> field_names() {
        constexpr auto names = glz::reflect<T>::keys;
        std::vector<std::string> result;
        glz::for_each<names.size()>([&]<size_t I>() {
            result.emplace_back(names[I].data(), names[I].size());
        });
        return result;
    }
    
    // Get field value by name (compile-time known type)
    template<typename T, typename R>
    static R getfield(T& obj, const std::string& field_name);
    
    // Set field value by name (compile-time known type)
    template<typename T, typename V>
    static void setfield(T& obj, const std::string& field_name, V&& val);
    
    // Call method by name (compile-time known type)
    template<typename T, typename... Args>
    static auto call_method(T& obj, const std::string& method_name, Args&&... args);
};

// Implementation of template methods

template<typename... Args>
ivalue iinstance::call(const std::string& method_name, Args&&... args) {
    if (!type_->has_method(method_name)) {
        throw std::runtime_error("method not found: " + method_name);
    }
    
    const auto& m = type_->get_method(method_name);
    const auto* member_info = m.info_;
    
    // Convert arguments to void* array
    void* arg_array[sizeof...(Args)] = { const_cast<void*>(static_cast<const void*>(&args))... };
    void** arg_ptr = sizeof...(Args) > 0 ? arg_array : nullptr;
    
    // Determine return type and allocate result buffer
    // This is simplified - real implementation would handle all types
    void* result_buffer = nullptr;
    ivalue result;
    
    // Call the function through the C API
    void* raw_result = glz_call_member_function_with_type(
        ptr_, 
        type_->name().c_str(),
        member_info,
        arg_ptr,
        result_buffer
    );
    
    // Convert raw result to value
    // This would need proper type handling based on method.return_type()
    if (raw_result) {
        // Handle different return types
        // For now, simplified
        result = ivalue();  // Placeholder
    }
    
    return result;
}

template<typename T, typename R>
R iglaze::getfield(T& obj, const std::string& field_name) {
    // Use compile-time reflection to get field
    R result{};
    bool found = false;
    
    constexpr auto names = glz::reflect<T>::keys;
    glz::for_each<names.size()>([&]<size_t I>() {
        if (!found && std::string_view(names[I]) == field_name) {
            if constexpr (glz::reflectable<T>) {
                result = glz::get_member(obj, glz::get<I>(glz::to_tie(obj)));
            } else {
                result = glz::get_member(obj, glz::get<I>(glz::reflect<T>::values));
            }
            found = true;
        }
    });
    
    if (!found) {
        throw std::runtime_error("field not found: " + field_name);
    }
    
    return result;
}

template<typename T, typename V>
void iglaze::setfield(T& obj, const std::string& field_name, V&& val) {
    bool found = false;
    
    constexpr auto names = glz::reflect<T>::keys;
    glz::for_each<names.size()>([&]<size_t I>() {
        if (!found && std::string_view(names[I]) == field_name) {
            if constexpr (glz::reflectable<T>) {
                glz::get_member(obj, glz::get<I>(glz::to_tie(obj))) = std::forward<V>(val);
            } else {
                glz::get_member(obj, glz::get<I>(glz::reflect<T>::values)) = std::forward<V>(val);
            }
            found = true;
        }
    });
    
    if (!found) {
        throw std::runtime_error("field not found: " + field_name);
    }
}

template<typename T, typename... Args>
auto iglaze::call_method(T& obj, const std::string& method_name, Args&&... args) {
    // This would use compile-time reflection to find and call the method
    // Implementation would be similar to getfield/setfield but for methods
    // For now, this is a placeholder
    throw std::runtime_error("method calling not yet implemented for compile-time types");
}

} // namespace glz

// Convenience macros (similar to Julia's @register_type)
#define GLZ_REGISTER_TYPE(Type) glz::iglaze::register_type<Type>(#Type)
#define GLZ_REGISTER_INSTANCE(name, Type, instance) glz::iglaze::register_instance(name, #Type, instance)