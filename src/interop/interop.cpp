#include "glaze/interop/interop.hpp"
#include <unordered_map>
#include <functional>
#include <cstring>
#include <cstdio>
#include <array>
#include <type_traits>
#include <utility>


#include <chrono>

namespace glz {

// Helper function to map type index to actual type using if constexpr
template<size_t I>
constexpr auto type_for_index() {
    if constexpr (I == 1) return bool{};
    else if constexpr (I == 2) return int8_t{};
    else if constexpr (I == 3) return int16_t{};
    else if constexpr (I == 4) return int32_t{};
    else if constexpr (I == 5) return int64_t{};
    else if constexpr (I == 6) return uint8_t{};
    else if constexpr (I == 7) return uint16_t{};
    else if constexpr (I == 8) return uint32_t{};
    else if constexpr (I == 9) return uint64_t{};
    else if constexpr (I == 10) return float{};
    else if constexpr (I == 11) return double{};
    else return; // void for invalid indices
}

// Type alias using decltype for faster compilation
template<size_t I>
using type_from_index = decltype(type_for_index<I>());

inline std::unordered_map<std::string_view, std::function<void*()>> constructors;
inline std::unordered_map<std::string_view, std::function<void(void*)>> destructors;

struct TypeInfoCacheEntry {
    glz_type_info info;
    std::vector<glz_member_info> members;
    // No need to store member names - they have static lifetime from glz::reflect
};

inline std::unordered_map<std::string_view, TypeInfoCacheEntry> type_info_cache;

// Type descriptor pool implementation
::glz_type_descriptor* TypeDescriptorPool::allocate_primitive(uint8_t kind) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_PRIMITIVE;
    desc->data.primitive.kind = kind;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_string(bool is_view) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_STRING;
    desc->data.string.is_view = is_view ? 1 : 0;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_vector(::glz_type_descriptor* element) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_VECTOR;
    desc->data.vector.element_type = element;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_map(::glz_type_descriptor* key, ::glz_type_descriptor* value) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_MAP;
    desc->data.map.key_type = key;
    desc->data.map.value_type = value;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_complex(uint8_t kind) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_COMPLEX;
    desc->data.complex.kind = kind;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_struct(const char* type_name, const ::glz_type_info* info, size_t type_hash) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_STRUCT;
    desc->data.struct_type.type_name = type_name;
    desc->data.struct_type.info = info;
    desc->data.struct_type.type_hash = type_hash;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_optional(::glz_type_descriptor* element) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_OPTIONAL;
    desc->data.optional.element_type = element;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_function(::glz_type_descriptor* return_type, uint8_t param_count, 
                                                           ::glz_type_descriptor** param_types, bool is_const, void* function_ptr) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_FUNCTION;
    desc->data.function.return_type = return_type;
    desc->data.function.param_count = param_count;
    
    // Create a persistent copy of the param_types array if it exists
    if (param_count > 0 && param_types != nullptr) {
        // Use unique_ptr to manage the array lifetime
        auto param_array = std::make_unique<::glz_type_descriptor*[]>(param_count);
        for (uint8_t i = 0; i < param_count; ++i) {
            param_array[i] = param_types[i];
        }
        desc->data.function.param_types = param_array.get();
        // Store the unique_ptr to ensure proper cleanup
        param_arrays.push_back(std::move(param_array));
    } else {
        desc->data.function.param_types = nullptr;
    }
    
    desc->data.function.is_const = is_const ? 1 : 0;
    desc->data.function.function_ptr = function_ptr;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}

::glz_type_descriptor* TypeDescriptorPool::allocate_shared_future(::glz_type_descriptor* value_type) {
    auto desc = std::make_unique<::glz_type_descriptor>();
    desc->index = GLZ_TYPE_SHARED_FUTURE;
    desc->data.shared_future.value_type = value_type;
    
    auto* ptr = desc.get();
    descriptors.push_back(std::move(desc));
    return ptr;
}


void register_constructor(std::string_view type_name, std::function<void*()> ctor, std::function<void(void*)> dtor) {
    constructors[type_name] = ctor;
    destructors[type_name] = dtor;
}

}

// Helper struct to store optional operations for different types
struct OptionalOperations {
    bool (*has_value)(void* opt_ptr);
    void* (*get_value)(void* opt_ptr);
    void (*set_value)(void* opt_ptr, const void* value);
    void (*reset)(void* opt_ptr);
};

// Template to generate operations for a specific optional type
template<typename T>
OptionalOperations make_optional_operations() {
    return OptionalOperations{
        // has_value
        [](void* opt_ptr) -> bool {
            auto* opt = static_cast<std::optional<T>*>(opt_ptr);
            return opt->has_value();
        },
        // get_value
        [](void* opt_ptr) -> void* {
            auto* opt = static_cast<std::optional<T>*>(opt_ptr);
            return opt->has_value() ? &(opt->value()) : nullptr;
        },
        // set_value
        [](void* opt_ptr, const void* value) {
            auto* opt = static_cast<std::optional<T>*>(opt_ptr);
            if (value) {
                *opt = *static_cast<const T*>(value);
            }
        },
        // reset
        [](void* opt_ptr) {
            auto* opt = static_cast<std::optional<T>*>(opt_ptr);
            opt->reset();
        }
    };
}

// Registry for optional operations by primitive type kind
namespace {
    // Use primitive type kind as key for simplicity since we know the types
    std::unordered_map<uint8_t, OptionalOperations> primitive_optional_ops;
    
    // Special registry for string optionals
    OptionalOperations string_optional_ops;
    
    // Initialize operations for common types
    void init_optional_operations() {
        static bool initialized = false;
        if (initialized) return;
        
        // Register operations by primitive kind
        primitive_optional_ops[1] = make_optional_operations<bool>();      // Bool
        primitive_optional_ops[4] = make_optional_operations<int32_t>();   // I32
        primitive_optional_ops[10] = make_optional_operations<float>();    // F32
        primitive_optional_ops[11] = make_optional_operations<double>();   // F64
        
        // String operations
        string_optional_ops = make_optional_operations<std::string>();
        
        initialized = true;
    }
    
    // Helper to get operations based on type descriptor
    OptionalOperations* get_optional_operations(const glz_type_descriptor* element_type) {
        if (!element_type) return nullptr;
        
        switch (element_type->index) {
            case GLZ_TYPE_PRIMITIVE: {
                uint8_t prim_kind = element_type->data.primitive.kind;
                auto it = primitive_optional_ops.find(prim_kind);
                return it != primitive_optional_ops.end() ? &it->second : nullptr;
            }
            case GLZ_TYPE_STRING:
                return &string_optional_ops;
            default:
                return nullptr;
        }
    }
}


// Generic member function invoker using templates and type erasure
template<typename R, typename C, typename... Args>
void* invoke_member_function_impl(void* obj_ptr, void* func_ptr, void** args, void* result_buffer) {
    auto* obj = static_cast<C*>(obj_ptr);
    auto func = reinterpret_cast<R(C::*)(Args...)>(func_ptr);
    
    if constexpr (std::is_void_v<R>) {
        // Void return type
        std::apply([&](auto&&... unpacked_args) {
            (obj->*func)(unpacked_args...);
        }, std::tuple<Args...>{*static_cast<std::remove_cvref_t<Args>*>(args[0])...});
        return nullptr;
    } else if constexpr (sizeof...(Args) == 0) {
        // No arguments
        R result = (obj->*func)();
        if (result_buffer) {
            *static_cast<R*>(result_buffer) = result;
            return result_buffer;
        }
        return nullptr;
    } else if constexpr (sizeof...(Args) == 1) {
        // One argument
        using Arg0 = std::tuple_element_t<0, std::tuple<Args...>>;
        R result = (obj->*func)(*static_cast<std::remove_cvref_t<Arg0>*>(args[0]));
        if (result_buffer) {
            *static_cast<R*>(result_buffer) = result;
            return result_buffer;
        }
        return nullptr;
    } else if constexpr (sizeof...(Args) == 2) {
        // Two arguments
        using Arg0 = std::tuple_element_t<0, std::tuple<Args...>>;
        using Arg1 = std::tuple_element_t<1, std::tuple<Args...>>;
        R result = (obj->*func)(
            *static_cast<std::remove_cvref_t<Arg0>*>(args[0]),
            *static_cast<std::remove_cvref_t<Arg1>*>(args[1])
        );
        if (result_buffer) {
            *static_cast<R*>(result_buffer) = result;
            return result_buffer;
        }
        return nullptr;
    } else if constexpr (sizeof...(Args) == 3) {
        // Three arguments
        using Arg0 = std::tuple_element_t<0, std::tuple<Args...>>;
        using Arg1 = std::tuple_element_t<1, std::tuple<Args...>>;
        using Arg2 = std::tuple_element_t<2, std::tuple<Args...>>;
        R result = (obj->*func)(
            *static_cast<std::remove_cvref_t<Arg0>*>(args[0]),
            *static_cast<std::remove_cvref_t<Arg1>*>(args[1]),
            *static_cast<std::remove_cvref_t<Arg2>*>(args[2])
        );
        if (result_buffer) {
            *static_cast<R*>(result_buffer) = result;
            return result_buffer;
        }
        return nullptr;
    } else {
        // More than 3 arguments not supported yet
        return nullptr;
    }
}

extern "C" {

GLZ_API glz_type_info* glz_get_type_info(const char* type_name) {
    auto it = glz::type_info_cache.find(type_name);  // Transparent lookup with string_view
    if (it != glz::type_info_cache.end()) {
        return &it->second.info;
    }
    
    for (const auto& type : glz::interop_registry.get_types()) {
        if (type->name == type_name) {  // Compare as C strings
            glz::TypeInfoCacheEntry entry;
            entry.info.name = type->name.data();  // Null-terminated from string literal
            entry.info.size = type->size;
            entry.info.member_count = type->members.size();
            
            // Convert MemberInfo to glz_member_info
            entry.members.reserve(type->members.size());
            
            for (const auto& member : type->members) {
                glz_member_info c_member;
                c_member.name = member.name;  // Direct pointer - has static lifetime
                c_member.type = member.type;  // Direct pointer to type descriptor
                c_member.getter = member.getter;
                c_member.setter = member.setter;
                c_member.kind = static_cast<uint8_t>(member.kind);
                c_member.function_ptr = member.function_ptr;
                entry.members.push_back(c_member);
            }
            
            entry.info.members = entry.members.data();
            
            // Use string_view as key (type_name has static lifetime from string literal)
            std::string_view type_name_sv(type_name);
            glz::type_info_cache[type_name_sv] = std::move(entry);
            
            // Fix up the pointers after moving
            auto& cached_entry = glz::type_info_cache[type_name_sv];
            cached_entry.info.members = cached_entry.members.data();
            
            return &cached_entry.info;
        }
    }
    
    return nullptr;
}

GLZ_API glz_type_info* glz_get_type_info_by_hash(size_t type_hash) {
    auto hash_it = glz::type_hash_to_name.find(type_hash);
    if (hash_it != glz::type_hash_to_name.end()) {
        return glz_get_type_info(hash_it->second.data());
    }
    return nullptr;
}

GLZ_API void* glz_create_instance(const char* type_name) {
    auto it = glz::constructors.find(type_name);  // Transparent lookup with string_view
    if (it != glz::constructors.end()) {
        return it->second();
    }
    return nullptr;
}

GLZ_API void glz_destroy_instance(const char* type_name, void* instance) {
    auto it = glz::destructors.find(type_name);  // Transparent lookup with string_view
    if (it != glz::destructors.end()) {
        it->second(instance);
    }
}

GLZ_API void* glz_get_member_ptr(void* instance, const glz_member_info* member) {
    return member->getter(instance);
}

// Helper to extract primitive type from descriptor
static uint8_t get_primitive_type(const glz_type_descriptor* desc) {
    if (desc && desc->index == GLZ_TYPE_PRIMITIVE) {
        return desc->data.primitive.kind;
    }
    return 0;
}

// Generic vector implementations now taking type descriptor
GLZ_API glz_vector glz_vector_view(void* vec_ptr, const glz_type_descriptor* type_desc) {
    glz_vector view;
    view.data = nullptr;
    view.size = 0;
    view.capacity = 0;
    
    if (!type_desc || type_desc->index != GLZ_TYPE_VECTOR) {
        return view;
    }
    
    auto elem_type = get_primitive_type(type_desc->data.vector.element_type);
    
    if (elem_type >= 2 && elem_type <= 11) {
        // Use visit pattern for primitive types
        glz::visit<12>([vec_ptr, &view]<size_t index>() {
            if constexpr (index >= 2 && index <= 11) {
                using T = glz::type_from_index<index>;
                auto* vec = static_cast<std::vector<T>*>(vec_ptr);
                view.data = vec->data();
                view.size = vec->size();
                view.capacity = vec->capacity();
            }
        }, elem_type);
    } else {
        // Check for complex types
        if (type_desc->data.vector.element_type && 
            type_desc->data.vector.element_type->index == GLZ_TYPE_COMPLEX) {
            if (type_desc->data.vector.element_type->data.complex.kind == 0) { // float
                auto* vec = static_cast<std::vector<std::complex<float>>*>(vec_ptr);
                view.data = vec->data();
                view.size = vec->size();
                view.capacity = vec->capacity();
            } else { // double
                auto* vec = static_cast<std::vector<std::complex<double>>*>(vec_ptr);
                view.data = vec->data();
                view.size = vec->size();
                view.capacity = vec->capacity();
            }
        }
    }
    
    return view;
}

GLZ_API void glz_vector_resize(void* vec_ptr, const glz_type_descriptor* type_desc, size_t new_size) {
    if (!type_desc || type_desc->index != GLZ_TYPE_VECTOR) {
        return;
    }
    
    auto elem_type = get_primitive_type(type_desc->data.vector.element_type);
    
    if (elem_type >= 2 && elem_type <= 11) {
        // Use visit pattern for primitive types
        glz::visit<12>([vec_ptr, new_size]<size_t index>() {
            if constexpr (index >= 2 && index <= 11) {
                using T = glz::type_from_index<index>;
                static_cast<std::vector<T>*>(vec_ptr)->resize(new_size);
            }
        }, elem_type);
    } else {
        // Check for complex types
        if (type_desc->data.vector.element_type && 
            type_desc->data.vector.element_type->index == GLZ_TYPE_COMPLEX) {
            if (type_desc->data.vector.element_type->data.complex.kind == 0) { // float
                static_cast<std::vector<std::complex<float>>*>(vec_ptr)->resize(new_size);
            } else { // double
                static_cast<std::vector<std::complex<double>>*>(vec_ptr)->resize(new_size);
            }
        }
    }
}

GLZ_API void glz_vector_push_back(void* vec_ptr, const glz_type_descriptor* type_desc, const void* value) {
    if (!type_desc || type_desc->index != GLZ_TYPE_VECTOR) {
        return;
    }
    
    auto elem_type = get_primitive_type(type_desc->data.vector.element_type);
    
    if (elem_type >= 2 && elem_type <= 11) {
        // Use visit pattern for primitive types
        glz::visit<12>([vec_ptr, value]<size_t index>() {
            if constexpr (index >= 2 && index <= 11) {
                using T = glz::type_from_index<index>;
                static_cast<std::vector<T>*>(vec_ptr)->push_back(*static_cast<const T*>(value));
            }
        }, elem_type);
    } else {
        // Check for complex types
        if (type_desc->data.vector.element_type && 
            type_desc->data.vector.element_type->index == GLZ_TYPE_COMPLEX) {
            if (type_desc->data.vector.element_type->data.complex.kind == 0) { // float
                static_cast<std::vector<std::complex<float>>*>(vec_ptr)->push_back(*static_cast<const std::complex<float>*>(value));
            } else { // double
                static_cast<std::vector<std::complex<double>>*>(vec_ptr)->push_back(*static_cast<const std::complex<double>*>(value));
            }
        }
    }
}

GLZ_API glz_string glz_string_view(std::string* str) {
    glz_string view;
    view.data = const_cast<char*>(str->data());
    view.size = str->size();
    view.capacity = str->capacity();
    return view;
}

GLZ_API void glz_string_set(std::string* str, const char* value, size_t len) {
    str->assign(value, len);
}

GLZ_API const char* glz_string_c_str(std::string* str) {
    return str->c_str();
}

GLZ_API size_t glz_string_size(std::string* str) {
    return str->size();
}

// Specialized vector functions that don't need type descriptors
// These are specifically for the Julia interface

GLZ_API glz_vector glz_vector_float32_view(void* vec_ptr) {
    glz_vector view;
    auto* vec = static_cast<std::vector<float>*>(vec_ptr);
    view.data = vec->data();
    view.size = vec->size();
    view.capacity = vec->capacity();
    return view;
}

GLZ_API void glz_vector_float32_resize(void* vec_ptr, size_t new_size) {
    auto* vec = static_cast<std::vector<float>*>(vec_ptr);
    vec->resize(new_size);
}

GLZ_API void glz_vector_float32_push_back(void* vec_ptr, float value) {
    auto* vec = static_cast<std::vector<float>*>(vec_ptr);
    vec->push_back(value);
}

GLZ_API glz_vector glz_vector_float64_view(void* vec_ptr) {
    glz_vector view;
    auto* vec = static_cast<std::vector<double>*>(vec_ptr);
    view.data = vec->data();
    view.size = vec->size();
    view.capacity = vec->capacity();
    return view;
}

GLZ_API void glz_vector_float64_resize(void* vec_ptr, size_t new_size) {
    auto* vec = static_cast<std::vector<double>*>(vec_ptr);
    vec->resize(new_size);
}

GLZ_API void glz_vector_float64_push_back(void* vec_ptr, double value) {
    auto* vec = static_cast<std::vector<double>*>(vec_ptr);
    vec->push_back(value);
}

GLZ_API glz_vector glz_vector_int32_view(void* vec_ptr) {
    glz_vector view;
    auto* vec = static_cast<std::vector<int32_t>*>(vec_ptr);
    view.data = vec->data();
    view.size = vec->size();
    view.capacity = vec->capacity();
    return view;
}

GLZ_API void glz_vector_int32_resize(void* vec_ptr, size_t new_size) {
    auto* vec = static_cast<std::vector<int32_t>*>(vec_ptr);
    vec->resize(new_size);
}

GLZ_API void glz_vector_int32_push_back(void* vec_ptr, int32_t value) {
    auto* vec = static_cast<std::vector<int32_t>*>(vec_ptr);
    vec->push_back(value);
}

GLZ_API glz_vector glz_vector_complexf32_view(void* vec_ptr) {
    glz_vector view;
    auto* vec = static_cast<std::vector<std::complex<float>>*>(vec_ptr);
    view.data = vec->data();
    view.size = vec->size();
    view.capacity = vec->capacity();
    return view;
}

GLZ_API void glz_vector_complexf32_resize(void* vec_ptr, size_t new_size) {
    auto* vec = static_cast<std::vector<std::complex<float>>*>(vec_ptr);
    vec->resize(new_size);
}

GLZ_API void glz_vector_complexf32_push_back(void* vec_ptr, float real, float imag) {
    auto* vec = static_cast<std::vector<std::complex<float>>*>(vec_ptr);
    vec->push_back(std::complex<float>(real, imag));
}

GLZ_API glz_vector glz_vector_complexf64_view(void* vec_ptr) {
    glz_vector view;
    auto* vec = static_cast<std::vector<std::complex<double>>*>(vec_ptr);
    view.data = vec->data();
    view.size = vec->size();
    view.capacity = vec->capacity();
    return view;
}

GLZ_API void glz_vector_complexf64_resize(void* vec_ptr, size_t new_size) {
    auto* vec = static_cast<std::vector<std::complex<double>>*>(vec_ptr);
    vec->resize(new_size);
}

GLZ_API void glz_vector_complexf64_push_back(void* vec_ptr, double real, double imag) {
    auto* vec = static_cast<std::vector<std::complex<double>>*>(vec_ptr);
    vec->push_back(std::complex<double>(real, imag));
}

GLZ_API void* glz_get_instance(const char* instance_name) {
    return glz::interop_registry.get_instance(std::string_view(instance_name));
}

GLZ_API const char* glz_get_instance_type(const char* instance_name) {
    return glz::interop_registry.get_instance_type(std::string_view(instance_name));
}

// Optional API implementations using the registry approach
// Note: glz_optional_has_value is primarily for internal Julia use
// Julia users should use the idiomatic isnothing() function instead
GLZ_API bool glz_optional_has_value(void* opt_ptr, const glz_type_descriptor* element_type) {
    if (!opt_ptr || !element_type) return false;
    
    init_optional_operations();
    
    auto* ops = get_optional_operations(element_type);
    return ops ? ops->has_value(opt_ptr) : false;
}

GLZ_API void* glz_optional_get_value(void* opt_ptr, const glz_type_descriptor* element_type) {
    if (!opt_ptr || !element_type) return nullptr;
    
    init_optional_operations();
    
    auto* ops = get_optional_operations(element_type);
    return ops ? ops->get_value(opt_ptr) : nullptr;
}

GLZ_API void glz_optional_set_value(void* opt_ptr, const void* value, const glz_type_descriptor* element_type) {
    if (!opt_ptr || !value || !element_type) return;
    
    init_optional_operations();
    
    auto* ops = get_optional_operations(element_type);
    if (ops) {
        ops->set_value(opt_ptr, value);
    }
}

// Helper function specifically for setting string values in optionals
GLZ_API void glz_optional_set_string_value(void* opt_ptr, const char* value, size_t len) {
    if (!opt_ptr || !value) return;
    
    auto* opt = static_cast<std::optional<std::string>*>(opt_ptr);
    *opt = std::string(value, len);
}

GLZ_API void glz_optional_reset(void* opt_ptr, const glz_type_descriptor* element_type) {
    if (!opt_ptr || !element_type) return;
    
    init_optional_operations();
    
    auto* ops = get_optional_operations(element_type);
    if (ops) {
        ops->reset(opt_ptr);
    }
}


GLZ_API void* glz_call_member_function_with_type(void* obj_ptr, const char* type_name, const glz_member_info* member, void** args, void* result_buffer) {
    if (!obj_ptr || !type_name || !member || member->kind != 1) { // Must be a member function
        return nullptr;
    }
    
    if (!member->type || member->type->index != GLZ_TYPE_FUNCTION) {
        return nullptr;
    }
    
    // Check if we have a direct function pointer (from the new template-based system)
    if (member->function_ptr) {
        // Cast back to the invoker function type
        using InvokerFunc = void*(*)(void*, void**, void*);
        auto invoker = reinterpret_cast<InvokerFunc>(member->function_ptr);
        return invoker(obj_ptr, args, result_buffer);
    }
    
    // Function not found
    return nullptr;
}

// Vector creation/destruction implementations
GLZ_API void* glz_create_vector(const glz_type_descriptor* type_desc) {
    if (!type_desc || type_desc->index != GLZ_TYPE_VECTOR) {
        return nullptr;
    }
    
    auto& vec_desc = type_desc->data.vector;
    auto* element_type = vec_desc.element_type;
    
    if (!element_type) return nullptr;
    
    // Create vector based on element type
    if (element_type->index == GLZ_TYPE_PRIMITIVE) {
        auto& prim_desc = element_type->data.primitive;
        switch (prim_desc.kind) {
            case 4:  // I32
                return new std::vector<int32_t>();
            case 10: // F32
                return new std::vector<float>();
            case 11: // F64
                return new std::vector<double>();
            default:
                return nullptr;
        }
    } else if (element_type->index == GLZ_TYPE_STRING) {
        return new std::vector<std::string>();
    } else if (element_type->index == GLZ_TYPE_COMPLEX) {
        auto& complex_desc = element_type->data.complex;
        if (complex_desc.kind == 0) { // float complex
            return new std::vector<std::complex<float>>();
        } else { // double complex
            return new std::vector<std::complex<double>>();
        }
    }
    
    return nullptr;
}

GLZ_API void glz_destroy_vector(void* vec_ptr, const glz_type_descriptor* type_desc) {
    if (!vec_ptr || !type_desc || type_desc->index != GLZ_TYPE_VECTOR) {
        return;
    }
    
    auto& vec_desc = type_desc->data.vector;
    auto* element_type = vec_desc.element_type;
    
    if (!element_type) return;
    
    // Delete vector based on element type
    if (element_type->index == GLZ_TYPE_PRIMITIVE) {
        auto& prim_desc = element_type->data.primitive;
        switch (prim_desc.kind) {
            case 4:  // I32
                delete static_cast<std::vector<int32_t>*>(vec_ptr);
                break;
            case 10: // F32
                delete static_cast<std::vector<float>*>(vec_ptr);
                break;
            case 11: // F64
                delete static_cast<std::vector<double>*>(vec_ptr);
                break;
        }
    } else if (element_type->index == GLZ_TYPE_STRING) {
        delete static_cast<std::vector<std::string>*>(vec_ptr);
    } else if (element_type->index == GLZ_TYPE_COMPLEX) {
        auto& complex_desc = element_type->data.complex;
        if (complex_desc.kind == 0) { // float complex
            delete static_cast<std::vector<std::complex<float>>*>(vec_ptr);
        } else { // double complex
            delete static_cast<std::vector<std::complex<double>>*>(vec_ptr);
        }
    }
}

GLZ_API void glz_vector_set_data(void* vec_ptr, const glz_type_descriptor* type_desc, const void* data, size_t size) {
    if (!vec_ptr || !type_desc || !data || type_desc->index != GLZ_TYPE_VECTOR) {
        return;
    }
    
    auto& vec_desc = type_desc->data.vector;
    auto* element_type = vec_desc.element_type;
    
    if (!element_type) return;
    
    // Set vector data based on element type
    if (element_type->index == GLZ_TYPE_PRIMITIVE) {
        auto& prim_desc = element_type->data.primitive;
        switch (prim_desc.kind) {
            case 4: { // I32
                auto* vec = static_cast<std::vector<int32_t>*>(vec_ptr);
                const auto* typed_data = static_cast<const int32_t*>(data);
                vec->assign(typed_data, typed_data + size);
                break;
            }
            case 10: { // F32
                auto* vec = static_cast<std::vector<float>*>(vec_ptr);
                const auto* typed_data = static_cast<const float*>(data);
                vec->assign(typed_data, typed_data + size);
                break;
            }
            case 11: { // F64
                auto* vec = static_cast<std::vector<double>*>(vec_ptr);
                const auto* typed_data = static_cast<const double*>(data);
                vec->assign(typed_data, typed_data + size);
                break;
            }
        }
    }
}

// Specialized vector functions for common types
GLZ_API void* glz_create_vector_int32() {
    return new std::vector<int32_t>();
}

GLZ_API void* glz_create_vector_float32() {
    return new std::vector<float>();
}

GLZ_API void* glz_create_vector_float64() {
    return new std::vector<double>();
}

GLZ_API void* glz_create_vector_string() {
    return new std::vector<std::string>();
}

GLZ_API void glz_vector_int32_set_data(void* vec_ptr, const int32_t* data, size_t size) {
    if (!vec_ptr || !data) return;
    auto* vec = static_cast<std::vector<int32_t>*>(vec_ptr);
    vec->assign(data, data + size);
}

GLZ_API void glz_vector_float32_set_data(void* vec_ptr, const float* data, size_t size) {
    if (!vec_ptr || !data) return;
    auto* vec = static_cast<std::vector<float>*>(vec_ptr);
    vec->assign(data, data + size);
}

GLZ_API void glz_vector_float64_set_data(void* vec_ptr, const double* data, size_t size) {
    if (!vec_ptr || !data) return;
    auto* vec = static_cast<std::vector<double>*>(vec_ptr);
    vec->assign(data, data + size);
}

GLZ_API void glz_vector_string_push_back(void* vec_ptr, const char* str, size_t len) {
    if (!vec_ptr || !str) return;
    auto* vec = static_cast<std::vector<std::string>*>(vec_ptr);
    vec->emplace_back(str, len);
}

// Special handling for vector-returning member functions
// This creates a new vector on the heap and returns a pointer to it
GLZ_API void* glz_create_result_vector_int32() {
    return new std::vector<int32_t>();
}

GLZ_API void* glz_create_result_vector_float32() {
    return new std::vector<float>();
}

GLZ_API void* glz_create_result_vector_float64() {
    return new std::vector<double>();
}

GLZ_API void glz_destroy_result_vector_int32(void* vec_ptr) {
    delete static_cast<std::vector<int32_t>*>(vec_ptr);
}

GLZ_API void glz_destroy_result_vector_float32(void* vec_ptr) {
    delete static_cast<std::vector<float>*>(vec_ptr);
}

GLZ_API void glz_destroy_result_vector_float64(void* vec_ptr) {
    delete static_cast<std::vector<double>*>(vec_ptr);
}

// Size query functions for safe allocation
GLZ_API size_t glz_sizeof_vector_int32() {
    return sizeof(std::vector<int32_t>);
}

GLZ_API size_t glz_sizeof_vector_float32() {
    return sizeof(std::vector<float>);
}

GLZ_API size_t glz_sizeof_vector_float64() {
    return sizeof(std::vector<double>);
}

GLZ_API size_t glz_sizeof_vector_string() {
    return sizeof(std::vector<std::string>);
}

GLZ_API size_t glz_sizeof_vector_complexf32() {
    return sizeof(std::vector<std::complex<float>>);
}

GLZ_API size_t glz_sizeof_vector_complexf64() {
    return sizeof(std::vector<std::complex<double>>);
}

// Alignment query functions
GLZ_API size_t glz_alignof_vector_int32() {
    return alignof(std::vector<int32_t>);
}

GLZ_API size_t glz_alignof_vector_float32() {
    return alignof(std::vector<float>);
}

GLZ_API size_t glz_alignof_vector_float64() {
    return alignof(std::vector<double>);
}

GLZ_API size_t glz_alignof_vector_string() {
    return alignof(std::vector<std::string>);
}

GLZ_API size_t glz_alignof_vector_complexf32() {
    return alignof(std::vector<std::complex<float>>);
}

GLZ_API size_t glz_alignof_vector_complexf64() {
    return alignof(std::vector<std::complex<double>>);
}

// Temporary string creation/destruction
GLZ_API void* glz_create_string(const char* str, size_t len) {
    return new std::string(str, len);
}

GLZ_API void glz_destroy_string(void* str_ptr) {
    delete static_cast<std::string*>(str_ptr);
}

} // extern "C"

// Type-erased interface for shared_future
struct SharedFutureBase {
    virtual ~SharedFutureBase() = default;
    virtual bool is_ready() const = 0;
    virtual void wait() = 0;
    virtual bool valid() const = 0;
    virtual void* get_value() = 0;
    virtual const glz_type_descriptor* get_type_descriptor() const = 0;
};

// Helper template to cast shared_future based on type descriptor
template<typename T>
bool shared_future_is_ready_impl(void* future_ptr) {
    auto* future = static_cast<std::shared_future<T>*>(future_ptr);
    return future->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template<typename T>
void shared_future_wait_impl(void* future_ptr) {
    auto* future = static_cast<std::shared_future<T>*>(future_ptr);
    future->wait();
}

template<typename T>
bool shared_future_valid_impl(void* future_ptr) {
    auto* future = static_cast<std::shared_future<T>*>(future_ptr);
    return future->valid();
}

template<typename T>
void* shared_future_get_impl(void* future_ptr) {
    auto* future = static_cast<std::shared_future<T>*>(future_ptr);
    if (future->valid()) {
        // For primitive types, we need to allocate and return
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            T* result = new T(future->get());
            return result;
        } else {
            // For complex types, return address of the result
            static thread_local T result;
            result = future->get();
            return &result;
        }
    }
    return nullptr;
}

// Dispatch based on type descriptor
using SharedFutureOp = void*(*)(void*);

template<typename Op>
void* dispatch_shared_future_op(void* future_ptr, const glz_type_descriptor* value_type, Op op) {
    if (!value_type) return nullptr;
    
    if (value_type->index == GLZ_TYPE_PRIMITIVE) {
        auto kind = value_type->data.primitive.kind;
        switch (kind) {
            case 1: return op.template operator()<bool>(future_ptr);
            case 2: return op.template operator()<int8_t>(future_ptr);
            case 3: return op.template operator()<int16_t>(future_ptr);
            case 4: return op.template operator()<int32_t>(future_ptr);
            case 5: return op.template operator()<int64_t>(future_ptr);
            case 6: return op.template operator()<uint8_t>(future_ptr);
            case 7: return op.template operator()<uint16_t>(future_ptr);
            case 8: return op.template operator()<uint32_t>(future_ptr);
            case 9: return op.template operator()<uint64_t>(future_ptr);
            case 10: return op.template operator()<float>(future_ptr);
            case 11: return op.template operator()<double>(future_ptr);
        }
    } else if (value_type->index == GLZ_TYPE_STRING) {
        return op.template operator()<std::string>(future_ptr);
    } else if (value_type->index == GLZ_TYPE_VECTOR) {
        // Would need to dispatch based on element type
        // For now, just support vector<int32_t> as example
        auto elem_type = value_type->data.vector.element_type;
        if (elem_type && elem_type->index == GLZ_TYPE_PRIMITIVE && elem_type->data.primitive.kind == 4) {
            return op.template operator()<std::vector<int32_t>>(future_ptr);
        }
    }
    
    return nullptr;
}

extern "C" {

GLZ_API bool glz_shared_future_is_ready(void* future_ptr) {
    auto* wrapper = static_cast<SharedFutureBase*>(future_ptr);
    return wrapper->is_ready();
}

GLZ_API void glz_shared_future_wait(void* future_ptr) {
    auto* wrapper = static_cast<SharedFutureBase*>(future_ptr);
    wrapper->wait();
}

GLZ_API bool glz_shared_future_valid(void* future_ptr) {
    auto* wrapper = static_cast<SharedFutureBase*>(future_ptr);
    return wrapper->valid();
}

} // extern "C"

struct GetOp {
    template<typename T>
    void* operator()(void* ptr) const {
        return shared_future_get_impl<T>(ptr);
    }
};

extern "C" {

GLZ_API void* glz_shared_future_get(void* future_ptr, const glz_type_descriptor* value_type) {
    if (!future_ptr) return nullptr;
    
    auto* wrapper = static_cast<SharedFutureBase*>(future_ptr);
    return wrapper->get_value();
}

GLZ_API void glz_shared_future_destroy(void* future_ptr, const glz_type_descriptor* value_type) {
    delete static_cast<SharedFutureBase*>(future_ptr);
}


GLZ_API const glz_type_descriptor* glz_shared_future_get_value_type(void* future_ptr) {
    if (!future_ptr) return nullptr;
    
    auto* wrapper = static_cast<SharedFutureBase*>(future_ptr);
    return wrapper->get_type_descriptor();
}

// Pure C FFI functions for dynamic type registration
GLZ_API bool glz_register_type_dynamic(
    const char* name,
    size_t size,
    size_t alignment,
    void* (*constructor)(void),
    void (*destructor)(void*)
) {
    if (!name || !constructor || !destructor) return false;
    
    // Register constructor and destructor with existing system
    glz::register_constructor(name, constructor, destructor);
    
    // Create a minimal type entry in the existing registry
    auto type_info = std::make_unique<glz::TypeInfo>();
    type_info->name = name;
    type_info->size = size;
    // Leave members empty - can be populated later
    
    glz::interop_registry.add_type(std::move(type_info));
    return true;
}

GLZ_API bool glz_register_member_data(
    const char* type_name,
    const char* member_name,
    void* (*getter)(void*),
    void (*setter)(void*, void*)
) {
    if (!type_name || !member_name || !getter) return false;
    
    // Find the existing type in the registry
    for (auto& type : glz::interop_registry.types) {
        if (type->name == type_name) {
            glz::MemberInfo member;
            member.name = member_name;  // Assumes static lifetime
            member.type = nullptr;      // Type descriptor not required for basic FFI
            member.getter = reinterpret_cast<void*(*)(void*)>(getter);
            member.setter = reinterpret_cast<void(*)(void*, void*)>(setter);
            member.kind = glz::MemberKind::DATA_MEMBER;
            member.function_ptr = nullptr;
            
            type->members.push_back(member);
            type->member_names.emplace_back(member_name);
            
            // Fix member name pointer after adding
            type->members.back().name = type->member_names.back().data();
            
            return true;
        }
    }
    return false;  // Type not found
}

} // extern "C"