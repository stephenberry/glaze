// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>
#include <vector>
#include <complex>
#include <unordered_map>
#include <optional>
#include <utility>
#include <future>
#include <cstdio>
#include <iostream>
#include <chrono>
#include "glaze/glaze.hpp"

#ifdef _WIN32
    #ifdef GLZ_EXPORTS
        #define GLZ_API __declspec(dllexport)
    #else
        #define GLZ_API __declspec(dllimport)
    #endif
#else
    #define GLZ_API __attribute__((visibility("default")))
#endif

// C-compatible type descriptor structures (outside namespace)

// Ensure consistent struct packing between C++ and Julia
#pragma pack(push, 1)

// Type descriptor index - acts like variant::index()
enum glz_type_kind : uint8_t {
    GLZ_TYPE_PRIMITIVE = 0,
    GLZ_TYPE_STRING,
    GLZ_TYPE_VECTOR,
    GLZ_TYPE_MAP,
    GLZ_TYPE_COMPLEX,
    GLZ_TYPE_STRUCT,
    GLZ_TYPE_OPTIONAL,
    GLZ_TYPE_FUNCTION,
    GLZ_TYPE_SHARED_FUTURE
};

// Forward declarations
struct glz_type_descriptor;
struct glz_type_info;

// Individual type descriptors as C structs
struct glz_primitive_desc {
    uint8_t kind;  // Maps to existing ValueType enum values
};

struct glz_string_desc {
    uint8_t is_view;  // 0=string, 1=string_view
};

struct glz_vector_desc {
    glz_type_descriptor* element_type;  // Owned pointer
};

struct glz_map_desc {
    glz_type_descriptor* key_type;      // Owned pointer
    glz_type_descriptor* value_type;    // Owned pointer
};

struct glz_complex_desc {
    uint8_t kind;  // 0=float, 1=double
};

struct glz_struct_desc {
    const char* type_name;         // Static string
    const glz_type_info* info;     // Pointer to registered type
    size_t type_hash;              // Hash of the type for runtime identification
};

struct glz_optional_desc {
    glz_type_descriptor* element_type;  // Owned pointer to element type
};

struct glz_function_desc {
    glz_type_descriptor* return_type;   // Return type descriptor
    uint8_t param_count;                // Number of parameters
    glz_type_descriptor** param_types;  // Array of parameter type descriptors
    uint8_t is_const;                   // 1 if const member function, 0 otherwise
    void* function_ptr;                 // Type-erased function pointer
};

struct glz_shared_future_desc {
    glz_type_descriptor* value_type;    // Type of the future's value
};

#pragma pack(pop)

// The variant-like type descriptor - needs explicit padding for alignment
#pragma pack(push, 8)  // 8-byte alignment for the union
struct glz_type_descriptor {
    uint8_t index;  // Which union member is active (glz_type_kind)
    uint8_t padding[7];  // Explicit padding to align union to 8 bytes
    
    union {
        struct glz_primitive_desc primitive;
        struct glz_string_desc string;
        struct glz_vector_desc vector;
        struct glz_map_desc map;
        struct glz_complex_desc complex;
        struct glz_struct_desc struct_type;
        struct glz_optional_desc optional;
        struct glz_function_desc function;
        struct glz_shared_future_desc shared_future;
    } data;
};
#pragma pack(pop)

namespace glz {

// Compile-time FNV-1a hash function
template<size_t N>
constexpr size_t fnv1a_hash(const char (&str)[N]) {
    size_t hash = 14695981039346656037ULL; // FNV offset basis
    for (size_t i = 0; i < N - 1; ++i) { // N-1 to skip null terminator
        hash ^= static_cast<size_t>(str[i]);
        hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
}

// Helper to hash string_view at compile time
template<typename T>
constexpr size_t type_hash() {
    constexpr auto name = glz::name_v<T>;
    size_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < name.size(); ++i) {
        hash ^= static_cast<size_t>(name[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

GLZ_API void register_constructor(std::string_view type_name, std::function<void*()> ctor, std::function<void(void*)> dtor);

// Map to store type hash to name mapping for runtime type resolution
inline std::unordered_map<size_t, std::string_view> type_hash_to_name;

enum class MemberKind : uint8_t {
    DATA_MEMBER = 0,
    MEMBER_FUNCTION = 1
};

struct MemberInfo {
    const char* name;  // Keep as const char* for C compatibility
    ::glz_type_descriptor* type;  // Replaces the three enum fields
    void* (*getter)(void* obj);
    void (*setter)(void* obj, void* value);
    MemberKind kind = MemberKind::DATA_MEMBER;
    void* function_ptr = nullptr;  // For storing member function pointers
};

struct TypeInfo {
    std::string_view name;  // Type names are string literals with static lifetime
    size_t size;
    std::vector<MemberInfo> members;
    std::vector<std::string_view> member_names;  // Store member name views (static constexpr lifetime)
};

// Type descriptor pool for managing descriptor lifetime
struct TypeDescriptorPool {
    std::vector<std::unique_ptr<::glz_type_descriptor>> descriptors;
    // Track allocated parameter arrays to prevent memory leaks
    std::vector<std::unique_ptr<::glz_type_descriptor*[]>> param_arrays;
    
    ::glz_type_descriptor* allocate_primitive(uint8_t kind);
    ::glz_type_descriptor* allocate_string(bool is_view);
    ::glz_type_descriptor* allocate_vector(::glz_type_descriptor* element);
    ::glz_type_descriptor* allocate_map(::glz_type_descriptor* key, ::glz_type_descriptor* value);
    ::glz_type_descriptor* allocate_complex(uint8_t kind);
    ::glz_type_descriptor* allocate_struct(const char* type_name, const ::glz_type_info* info, size_t type_hash = 0);
    ::glz_type_descriptor* allocate_optional(::glz_type_descriptor* element);
    ::glz_type_descriptor* allocate_function(::glz_type_descriptor* return_type, uint8_t param_count, 
                                           ::glz_type_descriptor** param_types, bool is_const, void* function_ptr);
    ::glz_type_descriptor* allocate_shared_future(::glz_type_descriptor* value_type);
    
    void clear() { 
        descriptors.clear(); 
        param_arrays.clear();
    }
};

inline TypeDescriptorPool type_descriptor_pool;

// Type to index mapping for primitive types
template<typename T>
struct primitive_type_index {
    static constexpr uint8_t value = 0; // Default: not a primitive
};

// Specializations for each primitive type
template<> struct primitive_type_index<bool> { static constexpr uint8_t value = 1; };
template<> struct primitive_type_index<int8_t> { static constexpr uint8_t value = 2; };
template<> struct primitive_type_index<int16_t> { static constexpr uint8_t value = 3; };
template<> struct primitive_type_index<int32_t> { static constexpr uint8_t value = 4; };
template<> struct primitive_type_index<int64_t> { static constexpr uint8_t value = 5; };
template<> struct primitive_type_index<uint8_t> { static constexpr uint8_t value = 6; };
template<> struct primitive_type_index<uint16_t> { static constexpr uint8_t value = 7; };
template<> struct primitive_type_index<uint32_t> { static constexpr uint8_t value = 8; };
template<> struct primitive_type_index<uint64_t> { static constexpr uint8_t value = 9; };
template<> struct primitive_type_index<float> { static constexpr uint8_t value = 10; };
template<> struct primitive_type_index<double> { static constexpr uint8_t value = 11; };

// Helper to normalize types - maps platform-specific types to fixed-width types
template<typename T>
struct normalize_type {
    using type = T;
};

// Map long/unsigned long to their fixed-width equivalents based on size
template<>
struct normalize_type<long> {
    using type = std::conditional_t<sizeof(long) == 8, int64_t, int32_t>;
};

template<>
struct normalize_type<unsigned long> {
    using type = std::conditional_t<sizeof(unsigned long) == 8, uint64_t, uint32_t>;
};

template<>
struct normalize_type<long long> {
    using type = int64_t;
};

template<>
struct normalize_type<unsigned long long> {
    using type = uint64_t;
};

template<typename T>
using normalize_type_t = typename normalize_type<T>::type;

// Helper variable template - now uses normalized types
template<typename T>
inline constexpr uint8_t primitive_type_index_v = primitive_type_index<normalize_type_t<T>>::value;

// Concept to check if a type is a primitive
template<typename T>
concept is_primitive_type = (primitive_type_index_v<T> != 0);

// Forward declarations
template<typename T>
::glz_type_descriptor* create_type_descriptor();

template<typename T>
void register_type(std::string_view name);

// Forward declaration for shared_future wrapper creation
template<typename T>
void* create_shared_future_wrapper(std::shared_future<T> future);

// Type-erased interface for shared_future (moved to header for template visibility)
struct SharedFutureBase {
    virtual ~SharedFutureBase() = default;
    virtual bool is_ready() const = 0;
    virtual void wait() = 0;
    virtual bool valid() const = 0;
    virtual void* get_value() = 0;
    virtual const ::glz_type_descriptor* get_type_descriptor() const = 0;
};

template<typename T>
struct SharedFutureWrapper : SharedFutureBase {
    std::shared_future<T> future;
    ::glz_type_descriptor* type_desc;
    
    explicit SharedFutureWrapper(std::shared_future<T> f) : future(std::move(f)) {
        type_desc = create_type_descriptor<T>();
        if (!type_desc) {
            // For struct types, create a descriptor with type hash
            constexpr size_t compile_time_hash = type_hash<T>();
            type_desc = type_descriptor_pool.allocate_struct(nullptr, nullptr, compile_time_hash);
        }
    }
    
    bool is_ready() const override {
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    
    void wait() override {
        future.wait();
    }
    
    bool valid() const override {
        return future.valid();
    }
    
    void* get_value() override {
        if (!future.valid()) return nullptr;
        
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
            // For primitive types, allocate and return
            T value = future.get();
            T* result = new T(value);
            return result;
        } else {
            // For all other types (including structs), allocate on heap
            T value = future.get();
            
            
            T* result = new T(std::move(value));
            
            
            return result;
        }
    }
    
    const ::glz_type_descriptor* get_type_descriptor() const override {
        return type_desc;
    }
};

// Helper to create a heap-allocated shared_future wrapper
template<typename T>
void* create_shared_future_wrapper(std::shared_future<T> future) {
    return new SharedFutureWrapper<T>(std::move(future));
}


// String types
template<> inline ::glz_type_descriptor* create_type_descriptor<::std::string>() {
    return type_descriptor_pool.allocate_string(false);
}
template<> inline ::glz_type_descriptor* create_type_descriptor<::std::string_view>() {
    return type_descriptor_pool.allocate_string(true);
}

// Complex types
template<> inline ::glz_type_descriptor* create_type_descriptor<::std::complex<float>>() {
    return type_descriptor_pool.allocate_complex(0); // float
}
template<> inline ::glz_type_descriptor* create_type_descriptor<::std::complex<double>>() {
    return type_descriptor_pool.allocate_complex(1); // double
}

// Optional specialization
template<typename T>
::glz_type_descriptor* create_type_descriptor_optional() {
    auto* element = create_type_descriptor<T>();
    return type_descriptor_pool.allocate_optional(element);
}

// Function specialization
template<typename F>
::glz_type_descriptor* create_type_descriptor_function(F function_ptr) {
    using traits = function_traits<F>;
    
    // Create return type descriptor
    auto* return_desc = create_type_descriptor<typename traits::return_type>();
    
    // Create parameter type descriptors
    constexpr size_t param_count = traits::arity;
    ::glz_type_descriptor** param_descs = nullptr;
    
    if constexpr (param_count > 0) {
        // Note: This array's ownership will be transferred to TypeDescriptorPool::allocate_function
        // which will manage its lifetime using unique_ptr
        param_descs = new ::glz_type_descriptor*[param_count];
        [&]<size_t... I>(::std::index_sequence<I...>) {
            // Strip const and reference qualifiers before creating descriptor
            ((param_descs[I] = create_type_descriptor<std::remove_cvref_t<typename traits::template arg<I>>>()), ...);
        }(::std::make_index_sequence<param_count>{});
        
    }
    
    // Store the function pointer (type-erased) 
    // Note: We need to store it as a union or use a different approach for member function pointers
    // For now, store nullptr and handle the actual function pointer differently
    void* func_ptr = nullptr;
    
    return type_descriptor_pool.allocate_function(
        return_desc, 
        static_cast<uint8_t>(param_count), 
        param_descs, 
        traits::is_const, 
        func_ptr
    );
}

template<typename T>
struct is_optional : ::std::false_type {};

template<typename T>
struct is_optional<::std::optional<T>> : ::std::true_type {};

// Member function signature detection
template<typename T>
struct function_traits;

template<typename R, typename Class, typename... Args>
struct function_traits<R(Class::*)(Args...)> {
    using return_type = R;
    using class_type = Class;
    using args_tuple = ::std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool is_const = false;
    
    template<size_t I>
    using arg = ::std::tuple_element_t<I, args_tuple>;
};

template<typename R, typename Class, typename... Args>
struct function_traits<R(Class::*)(Args...) const> {
    using return_type = R;
    using class_type = Class;
    using args_tuple = ::std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool is_const = true;
    
    template<size_t I>
    using arg = ::std::tuple_element_t<I, args_tuple>;
};

// Vector specialization
template<typename T>
::glz_type_descriptor* create_type_descriptor_vector() {
    auto* element = create_type_descriptor<T>();
    return type_descriptor_pool.allocate_vector(element);
}

template<typename T>
struct is_vector : ::std::false_type {};

template<typename T>
struct is_vector<::std::vector<T>> : ::std::true_type {};

// Map specialization
template<typename K, typename V>
::glz_type_descriptor* create_type_descriptor_map() {
    auto* key = create_type_descriptor<K>();
    auto* value = create_type_descriptor<V>();
    return type_descriptor_pool.allocate_map(key, value);
}

template<typename T>
struct is_unordered_map : ::std::false_type {};

template<typename K, typename V>
struct is_unordered_map<::std::unordered_map<K, V>> : ::std::true_type {};

// Pair trait - we'll still use this to detect pairs for auto-registration
template<typename T>
struct is_pair : ::std::false_type {};

template<typename T1, typename T2>
struct is_pair<::std::pair<T1, T2>> : ::std::true_type {};

// Trait to detect std::shared_future
template<typename T>
struct is_shared_future : ::std::false_type {};

template<typename T>
struct is_shared_future<::std::shared_future<T>> : ::std::true_type {
    using value_type = T;
};

// General template that handles vectors, maps, and optionals
template<typename T>
::glz_type_descriptor* create_type_descriptor() {
    if constexpr (is_primitive_type<T>) {
        return type_descriptor_pool.allocate_primitive(primitive_type_index_v<T>);
    }
    else if constexpr (is_vector<T>::value) {
        using element_type = typename T::value_type;
        return create_type_descriptor_vector<element_type>();
    }
    else if constexpr (is_unordered_map<T>::value) {
        using key_type = typename T::key_type;
        using mapped_type = typename T::mapped_type;
        return create_type_descriptor_map<key_type, mapped_type>();
    }
    else if constexpr (is_optional<T>::value) {
        using element_type = typename T::value_type;
        return create_type_descriptor_optional<element_type>();
    }
    else if constexpr (is_pair<T>::value) {
        // Register pair as a struct type
        // Using static string to ensure proper null termination and persistence
        static const std::string type_name(glz::name_v<T>);
        
        // Register the pair type
        register_type<T>(type_name);
        
        // The type is now registered, but we'll let Julia look it up by name
        // when needed rather than trying to get the type info pointer here
        return type_descriptor_pool.allocate_struct(type_name.c_str(), nullptr, type_hash<T>());
    }
    else if constexpr (is_shared_future<T>::value) {
        using value_type = typename is_shared_future<T>::value_type;
        auto* value_desc = create_type_descriptor<value_type>();
        return type_descriptor_pool.allocate_shared_future(value_desc);
    }
    else {
        // For struct types, this will be handled by the registry
        return nullptr;
    }
}

// Generic member function accessor that generates type-erased wrappers
template<typename Class, typename FuncPtr>
struct MemberFunctionAccessor;

// Specialization for non-const member functions
template<typename Class, typename R, typename... Args>
struct MemberFunctionAccessor<Class, R(Class::*)(Args...)> {
    using FuncType = R(Class::*)(Args...);
    static constexpr size_t arg_count = sizeof...(Args);
    
    // Type-erased invoker function
    template<FuncType func>
    static void* invoke(void* obj_ptr, void** args, void* result_buffer) {
        auto* obj = static_cast<Class*>(obj_ptr);
        
        // Helper to unpack arguments from void** array
        auto unpack_and_call = [&]<size_t... Is>(std::index_sequence<Is...>) {
            if constexpr (std::is_void_v<R>) {
                (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                return result_buffer; // Return something for consistency
            } else if constexpr (is_shared_future<R>::value) {
                // For shared_future returns, always allocate on heap
                // Debug: print argument values
                R future_result = (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                using value_type = typename is_shared_future<R>::value_type;
                return create_shared_future_wrapper(std::move(future_result));
            } else {
                if (result_buffer) {
                    // For non-trivial types like std::vector, we need placement new
                    if constexpr (!std::is_trivially_copyable_v<R>) {
                        // Call function and construct result in place
                        new(result_buffer) R((obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...));
                    } else {
                        // For trivial types, call function and assign
                        R result = (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                        *static_cast<R*>(result_buffer) = std::move(result);
                    }
                    return result_buffer;
                } else {
                    // No result buffer provided
                    (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                    return static_cast<void*>(nullptr);
                }
            }
        };
        
        return unpack_and_call(std::index_sequence_for<Args...>{});
    }
    
    // Get a function pointer to the invoker
    template<FuncType func>
    static constexpr auto get_invoker() {
        return &invoke<func>;
    }
};

// Specialization for const member functions
template<typename Class, typename R, typename... Args>
struct MemberFunctionAccessor<Class, R(Class::*)(Args...) const> {
    using FuncType = R(Class::*)(Args...) const;
    static constexpr size_t arg_count = sizeof...(Args);
    
    // Type-erased invoker function
    template<FuncType func>
    static void* invoke(void* obj_ptr, void** args, void* result_buffer) {
        auto* obj = static_cast<const Class*>(obj_ptr);
        
        // Helper to unpack arguments from void** array
        auto unpack_and_call = [&]<size_t... Is>(std::index_sequence<Is...>) {
            if constexpr (std::is_void_v<R>) {
                (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                return result_buffer; // Return something for consistency
            } else if constexpr (is_shared_future<R>::value) {
                // For shared_future returns, always allocate on heap
                // Debug: print argument values
                R future_result = (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                using value_type = typename is_shared_future<R>::value_type;
                return create_shared_future_wrapper(std::move(future_result));
            } else {
                if (result_buffer) {
                    // For non-trivial types like std::vector, we need placement new
                    if constexpr (!std::is_trivially_copyable_v<R>) {
                        // Call function and construct result in place
                        new(result_buffer) R((obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...));
                    } else {
                        // For trivial types, call function and assign
                        R result = (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                        *static_cast<R*>(result_buffer) = std::move(result);
                    }
                    return result_buffer;
                } else {
                    // No result buffer provided
                    (obj->*func)(*static_cast<std::decay_t<Args>*>(args[Is])...);
                    return static_cast<void*>(nullptr);
                }
            }
        };
        
        return unpack_and_call(std::index_sequence_for<Args...>{});
    }
    
    // Get a function pointer to the invoker
    template<FuncType func>
    static constexpr auto get_invoker() {
        return &invoke<func>;
    }
};

// Helper to create a member function info at compile time
template<typename Class, auto MemberFunc>
struct MemberFunctionInfo {
    using FuncPtr = decltype(MemberFunc);
    using Accessor = MemberFunctionAccessor<Class, FuncPtr>;
    
    static void* (*get_invoker())(void*, void**, void*) {
        return Accessor::template get_invoker<MemberFunc>();
    }
    
    static ::glz_type_descriptor* create_descriptor() {
        return create_type_descriptor_function(MemberFunc);
    }
};


// Generic member accessor using member pointers
template <typename Class, class Member, size_t I>
struct MemberAccessor {
    static decltype(auto) target(void* obj)
    {
        auto& typed_obj = *static_cast<Class*>(obj);
        if constexpr (glz::reflectable<Class>) {
            return glz::get_member(typed_obj, glz::get<I>(glz::to_tie(typed_obj)));
        }
        else {
            (void)typed_obj;
            return glz::get_member(typed_obj, glz::get<I>(glz::reflect<Class>::values));
        }
    }

    static void* getter(void* obj) {
        return &target(obj);
    }
    
    static void setter(void* obj, void* value) {        
        // Check if the type needs special handling (non-trivially copyable)
        if constexpr (::std::is_same_v<Member, ::std::string> ||
                     ::std::is_same_v<Member, ::std::string_view>) {
            target(obj) = *static_cast<Member*>(value);
        }
        // Handle all vector types
        else if constexpr (requires { typename Member::value_type; typename Member::allocator_type; }) {
            // This captures all std::vector types
            target(obj) = *static_cast<Member*>(value);
        }
        // Handle all unordered_map types
        else if constexpr (requires { typename Member::key_type; typename Member::mapped_type; typename Member::hasher; }) {
            // This captures all std::unordered_map types
            target(obj) = *static_cast<Member*>(value);
        }
        // Handle all optional types
        else if constexpr (is_optional<Member>::value) {
            // This captures all std::optional types
            target(obj) = *static_cast<Member*>(value);
        }
        // For trivially copyable types, use memcpy
        else {
            auto& member = target(obj);
            std::memcpy(&member, value, sizeof(Member));
        }
    }
};

// Helper to extract member info from glz::meta using Glaze's approach
template<typename T>
class TypeRegistry {
    TypeInfo info;
    
public:
    explicit TypeRegistry(std::string_view name) {
        info.name = name;  // Direct assignment - name has static lifetime
        info.size = sizeof(T);
        
        // Use Glaze's reflection pattern
        using V = ::std::decay_t<T>;
        static constexpr auto N = glz::reflect<V>::size;
        
        
        glz::for_each<N>([&]<auto I>() {
            // Get the key/name
            static constexpr auto key = glz::reflect<V>::keys[I];
            
            MemberInfo member_info;
            // Store the member name view (safe because glz::reflect keys are static constexpr)
            info.member_names.emplace_back(key.data(), key.size());
            member_info.name = info.member_names.back().data();  // Null-terminated from string literal
            
            // Get the member pointer value and check its type
            if constexpr (glz::reflectable<V>) {
                // For reflectable types, we can't easily detect member function pointers at compile time
                // So for now, treat all as data members
                using MemberTypeRef = glz::refl_t<V, I>;
                using MemberType = ::std::remove_cvref_t<MemberTypeRef>;
                
                member_info.kind = MemberKind::DATA_MEMBER;
                member_info.type = create_type_descriptor<MemberType>();
                
                if (!member_info.type) {
                    constexpr size_t compile_time_hash = type_hash<MemberType>();
                    member_info.type = type_descriptor_pool.allocate_struct(
                        nullptr, nullptr, compile_time_hash
                    );
                }
                
                member_info.getter = &MemberAccessor<T, MemberType, I>::getter;
                member_info.setter = &MemberAccessor<T, MemberType, I>::setter;
                member_info.function_ptr = nullptr;
            } else {
                // For non-reflectable types with explicit glz::meta
                constexpr auto member_ptr = glz::get<I>(glz::reflect<V>::values);
                using MemberPtrType = std::remove_const_t<decltype(member_ptr)>;
                
                if constexpr (::std::is_member_function_pointer_v<MemberPtrType>) {
                    // This is a member function
                    member_info.kind = MemberKind::MEMBER_FUNCTION;
                    member_info.type = create_type_descriptor_function(member_ptr);
                    member_info.getter = nullptr;
                    member_info.setter = nullptr;
                    
                    // Get the invoker function directly from MemberFunctionAccessor
                    using Accessor = MemberFunctionAccessor<T, MemberPtrType>;
                    member_info.function_ptr = reinterpret_cast<void*>(Accessor::template get_invoker<member_ptr>());
                } else {
                    // This is a data member
                    using MemberTypeRef = glz::refl_t<V, I>;
                    using MemberType = ::std::remove_cvref_t<MemberTypeRef>;
                    
                    member_info.kind = MemberKind::DATA_MEMBER;
                    member_info.type = create_type_descriptor<MemberType>();
                    
                    if (!member_info.type) {
                        constexpr size_t compile_time_hash = type_hash<MemberType>();
                        member_info.type = type_descriptor_pool.allocate_struct(
                            nullptr, nullptr, compile_time_hash
                        );
                    }
                    
                    member_info.getter = &MemberAccessor<T, MemberType, I>::getter;
                    member_info.setter = &MemberAccessor<T, MemberType, I>::setter;
                    member_info.function_ptr = nullptr;
                }
            }
            
            info.members.push_back(member_info);
        });
    }
    
    const TypeInfo& get_info() const { return info; }
};

struct InstanceInfo {
    std::string name;       // Use string to own the data
    std::string type_name;  // Use string to own the data
    void* ptr;
};

class Registry {
public:
    std::vector<std::unique_ptr<TypeInfo>> types;
    std::vector<std::unique_ptr<InstanceInfo>> instances;
    
    Registry() = default;
    
    template<typename T>
    TypeRegistry<T> register_type(std::string_view name) {
        return TypeRegistry<T>(name);
    }
    
    void add_type(std::unique_ptr<TypeInfo> type) {
        types.push_back(std::move(type));
        
        // Fix member name pointers after moving
        auto& stored_type = *types.back();
        for (size_t i = 0; i < stored_type.members.size(); ++i) {
            stored_type.members[i].name = stored_type.member_names[i].data();
        }
    }
    
    const std::vector<std::unique_ptr<TypeInfo>>& get_types() const {
        return types;
    }
    
    void add_instance(std::string_view name, std::string_view type_name, void* ptr) {
        auto inst = std::make_unique<InstanceInfo>();
        inst->name = std::string(name);
        inst->type_name = std::string(type_name);
        inst->ptr = ptr;
        instances.push_back(std::move(inst));
    }
    
    const std::vector<std::unique_ptr<InstanceInfo>>& get_instances() const {
        return instances;
    }
    
    void* get_instance(std::string_view name) const {
        for (const auto& inst : instances) {
            if (inst->name == name) {
                return inst->ptr;
            }
        }
        return nullptr;
    }
    
    const char* get_instance_type(std::string_view name) const {
        for (const auto& inst : instances) {
            if (inst->name == name) {
                return inst->type_name.c_str();
            }
        }
        return nullptr;
    }
};

// Global inline instance of Registry for Julia-C++ interoperability
// Named 'interop_registry' to avoid confusion with Glaze's RPC registry template
inline Registry interop_registry;


// Function to register a type - uses glz::meta automatically
template<typename T>
void register_type(std::string_view name) {
    // Check if already registered by looking in the interop_registry
    for (const auto& existing : interop_registry.get_types()) {
        if (existing->name == name) {
            return; // Already registered
        }
    }
    
    auto reg = interop_registry.register_type<T>(name);
    interop_registry.add_type(std::make_unique<TypeInfo>(reg.get_info()));
    register_constructor(name,
        []() -> void* { return new T(); },
        [](void* ptr) { delete static_cast<T*>(ptr); }
    );
    
    // Store type hash mapping for runtime type resolution using compile-time hash
    type_hash_to_name[type_hash<T>()] = name;
}

// Function to register an instance - requires type to be registered first
template<typename T>
void register_instance(std::string_view instance_name, std::string_view type_name, T& instance) {
    interop_registry.add_instance(instance_name, type_name, static_cast<void*>(&instance));
}

// Meta specialization for std::pair - treat it as a struct with first/second members
template <typename T1, typename T2>
struct meta<std::pair<T1, T2>> {
    using T = std::pair<T1, T2>;
    static constexpr auto value = glz::object(
        "first", &T::first,
        "second", &T::second
    );
};

} // namespace glz

extern "C" {

#pragma pack(push, 8)  // Ensure 8-byte alignment for pointers

// C-compatible version of MemberInfo
struct glz_member_info {
    const char* name;
    glz_type_descriptor* type;  // Now uses the new type descriptor
    void* (*getter)(void* obj);
    void (*setter)(void* obj, void* value);
    uint8_t kind;  // 0 = data member, 1 = member function
    void* function_ptr;  // For member function pointers
};

struct glz_type_info {
    const char* name;
    size_t size;
    size_t member_count;
    glz_member_info* members;
};

// Generic vector view structure
struct glz_vector {
    void* data;
    size_t size;
    size_t capacity;
};

struct glz_string {
    char* data;
    size_t size;
    size_t capacity;
};

// Generic unordered_map view structure
struct glz_unordered_map {
    void* impl;  // Opaque pointer to the C++ unordered_map
    size_t size;
};

#pragma pack(pop)

GLZ_API glz_type_info* glz_get_type_info(const char* type_name);
GLZ_API glz_type_info* glz_get_type_info_by_hash(size_t type_hash);
GLZ_API void* glz_create_instance(const char* type_name);
GLZ_API void glz_destroy_instance(const char* type_name, void* instance);
GLZ_API void* glz_get_member_ptr(void* instance, const glz_member_info* member);

// Generic vector API functions - type is determined by type descriptor
GLZ_API glz_vector glz_vector_view(void* vec_ptr, const glz_type_descriptor* type_desc);
GLZ_API void glz_vector_resize(void* vec_ptr, const glz_type_descriptor* type_desc, size_t new_size);
GLZ_API void glz_vector_push_back(void* vec_ptr, const glz_type_descriptor* type_desc, const void* value);

GLZ_API glz_string glz_string_view(std::string* str);
GLZ_API void glz_string_set(std::string* str, const char* value, size_t len);
GLZ_API const char* glz_string_c_str(std::string* str);
GLZ_API size_t glz_string_size(std::string* str);

// Instance access functions
GLZ_API void* glz_get_instance(const char* instance_name);
GLZ_API const char* glz_get_instance_type(const char* instance_name);

// Optional API functions
// Note: glz_optional_has_value is for internal use - Julia users should use isnothing() instead
GLZ_API bool glz_optional_has_value(void* opt_ptr, const glz_type_descriptor* element_type);
GLZ_API void* glz_optional_get_value(void* opt_ptr, const glz_type_descriptor* element_type);
GLZ_API void glz_optional_set_value(void* opt_ptr, const void* value, const glz_type_descriptor* element_type);
GLZ_API void glz_optional_set_string_value(void* opt_ptr, const char* value, size_t len);
GLZ_API void glz_optional_reset(void* opt_ptr, const glz_type_descriptor* element_type);

// Member function invocation API
GLZ_API void* glz_call_member_function_with_type(void* obj_ptr, const char* type_name, const glz_member_info* member, void** args, void* result_buffer);

// Temporary vector creation/destruction for function calls
GLZ_API void* glz_create_vector(const glz_type_descriptor* type_desc);
GLZ_API void glz_destroy_vector(void* vec_ptr, const glz_type_descriptor* type_desc);
GLZ_API void glz_vector_set_data(void* vec_ptr, const glz_type_descriptor* type_desc, const void* data, size_t size);

// Specialized vector creation for common types (more efficient)
GLZ_API void* glz_create_vector_int32();
GLZ_API void* glz_create_vector_float32();
GLZ_API void* glz_create_vector_float64();
GLZ_API void* glz_create_vector_string();
GLZ_API void glz_vector_int32_set_data(void* vec_ptr, const int32_t* data, size_t size);
GLZ_API void glz_vector_float32_set_data(void* vec_ptr, const float* data, size_t size);
GLZ_API void glz_vector_float64_set_data(void* vec_ptr, const double* data, size_t size);
GLZ_API void glz_vector_string_push_back(void* vec_ptr, const char* str, size_t len);

// Query vector size and alignment for safe allocation
GLZ_API size_t glz_sizeof_vector_int32();
GLZ_API size_t glz_sizeof_vector_float32();
GLZ_API size_t glz_sizeof_vector_float64();
GLZ_API size_t glz_sizeof_vector_string();
GLZ_API size_t glz_sizeof_vector_complexf32();
GLZ_API size_t glz_sizeof_vector_complexf64();

GLZ_API size_t glz_alignof_vector_int32();
GLZ_API size_t glz_alignof_vector_float32();
GLZ_API size_t glz_alignof_vector_float64();
GLZ_API size_t glz_alignof_vector_string();
GLZ_API size_t glz_alignof_vector_complexf32();
GLZ_API size_t glz_alignof_vector_complexf64();

// Temporary string creation/destruction for function calls
GLZ_API void* glz_create_string(const char* str, size_t len);
GLZ_API void glz_destroy_string(void* str_ptr);

// Shared future API functions
GLZ_API bool glz_shared_future_is_ready(void* future_ptr);
GLZ_API void glz_shared_future_wait(void* future_ptr);
GLZ_API void* glz_shared_future_get(void* future_ptr, const glz_type_descriptor* value_type);
GLZ_API bool glz_shared_future_valid(void* future_ptr);
GLZ_API void glz_shared_future_destroy(void* future_ptr, const glz_type_descriptor* value_type);
GLZ_API const glz_type_descriptor* glz_shared_future_get_value_type(void* future_ptr);

}