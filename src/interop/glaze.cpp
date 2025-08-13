#include "glaze/interop/glaze.hpp"
#include <sstream>
#include <iomanip>

namespace glz {

// Static member definitions
std::unordered_map<std::string, std::shared_ptr<itype>> iglaze::type_registry_;
std::unordered_map<std::string, std::shared_ptr<iinstance>> iglaze::instance_registry_;
std::vector<std::unique_ptr<interop::interop_library>> iglaze::loaded_libraries_;

// ivalue implementation
bool ivalue::as_bool() const {
    if (auto* ptr = std::get_if<bool*>(&value_)) return **ptr;
    throw std::runtime_error("ivalue is not a boolean");
}

int64_t ivalue::as_int() const {
    if (auto* val = std::get_if<int8_t*>(&value_)) return **val;
    if (auto* val = std::get_if<int16_t*>(&value_)) return **val;
    if (auto* val = std::get_if<int32_t*>(&value_)) return **val;
    if (auto* val = std::get_if<int64_t*>(&value_)) return **val;
    if (auto* val = std::get_if<uint8_t*>(&value_)) return **val;
    if (auto* val = std::get_if<uint16_t*>(&value_)) return **val;
    if (auto* val = std::get_if<uint32_t*>(&value_)) return **val;
    if (auto* val = std::get_if<uint64_t*>(&value_)) return **val;
    throw std::runtime_error("ivalue is not an integer type");
}

double ivalue::as_float() const {
    if (auto* val = std::get_if<float*>(&value_)) return **val;
    if (auto* val = std::get_if<double*>(&value_)) return **val;
    // Also convert integers to float
    if (is_int()) return static_cast<double>(as_int());
    throw std::runtime_error("ivalue is not a numeric type");
}

std::string ivalue::as_string() const {
    if (auto* val = std::get_if<std::string*>(&value_)) {
        return **val;
    }
    // Convert other types to string
    if (is_bool()) return as_bool() ? "true" : "false";
    if (is_int()) return std::to_string(as_int());
    if (is_float()) return std::to_string(as_float());
    if (is_null()) return "null";
    throw std::runtime_error("ivalue cannot be converted to string");
}

std::vector<ivalue>& ivalue::as_array() {
    if (auto* val = std::get_if<std::vector<ivalue>*>(&value_)) {
        return **val;
    }
    throw std::runtime_error("ivalue is not an array");
}

const std::vector<ivalue>& ivalue::as_array() const {
    if (auto* val = std::get_if<std::vector<ivalue>*>(&value_)) {
        return **val;
    }
    throw std::runtime_error("ivalue is not an array");
}

ivalue& ivalue::operator[](const std::string& key) {
    if (auto* obj_ptr = std::get_if<std::unordered_map<std::string, ivalue>*>(&value_)) {
        return (**obj_ptr)[key];
    }
    throw std::runtime_error("ivalue is not an object");
}

const ivalue& ivalue::operator[](const std::string& key) const {
    if (auto* obj_ptr = std::get_if<std::unordered_map<std::string, ivalue>*>(&value_)) {
        const auto& obj = **obj_ptr;
        auto it = obj.find(key);
        if (it == obj.end()) {
            static const ivalue null_value;
            return null_value;
        }
        return it->second;
    }
    throw std::runtime_error("ivalue is not an object");
}

ivalue& ivalue::operator[](size_t index) {
    if (auto* arr_ptr = std::get_if<std::vector<ivalue>*>(&value_)) {
        auto& arr = **arr_ptr;
        if (index >= arr.size()) {
            throw std::out_of_range("Array index out of bounds");
        }
        return arr[index];
    }
    throw std::runtime_error("ivalue is not an array");
}

const ivalue& ivalue::operator[](size_t index) const {
    if (auto* arr_ptr = std::get_if<std::vector<ivalue>*>(&value_)) {
        const auto& arr = **arr_ptr;
        if (index >= arr.size()) {
            throw std::out_of_range("Array index out of bounds");
        }
        return arr[index];
    }
    throw std::runtime_error("ivalue is not an array");
}

std::string ivalue::to_json() const {
    std::ostringstream oss;
    
    std::visit([&oss](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        
        if constexpr (std::is_same_v<T, std::monostate>) {
            oss << "null";
        } else if constexpr (std::is_same_v<T, bool*>) {
            oss << (*val ? "true" : "false");
        } else if constexpr (std::is_arithmetic_v<std::remove_pointer_t<T>>) {
            oss << *val;
        } else if constexpr (std::is_same_v<T, std::string*>) {
            oss << std::quoted(*val);
        } else if constexpr (std::is_same_v<T, std::vector<ivalue>*>) {
            oss << "[";
            const auto& vec = *val;
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i > 0) oss << ",";
                oss << vec[i].to_json();
            }
            oss << "]";
        } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, ivalue>*>) {
            oss << "{";
            bool first = true;
            for (const auto& [key, value] : *val) {
                if (!first) oss << ",";
                oss << std::quoted(key) << ":" << value.to_json();
                first = false;
            }
            oss << "}";
        } else if constexpr (std::is_same_v<T, std::complex<float>*> || 
                           std::is_same_v<T, std::complex<double>*>) {
            const auto& c = *val;
            oss << "{\"real\":" << c.real() << ",\"imag\":" << c.imag() << "}";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<iinstance>>) {
            if (val) {
                oss << val->to_json();
            } else {
                oss << "null";
            }
        } else {
            oss << "\"<unsupported>\"";
        }
    }, value_);
    
    return oss.str();
}

ivalue ivalue::from_json(const std::string& json) {
    // This would need a proper JSON parser
    // For now, return a placeholder
    return ivalue();
}

// itype implementation
itype::itype(const glz_type_info* info) : info_(info) {
    if (info_) {
        name_ = info_->name;
        size_ = info_->size;
        
        // Parse members into fields and methods
        for (size_t i = 0; i < info_->member_count; ++i) {
            const auto& member = info_->members[i];
            std::string member_name(member.name);
            
            if (member.kind == 1) {  // method
                imethod m(member_name, &member);
                // Parse method signature from type descriptor
                // This would need to extract param and return types
                methods_[member_name] = m;
            } else {  // field
                ifield f(member_name, &member, nullptr);
                // Parse field type from type descriptor
                fields_[member_name] = f;
            }
        }
    }
}

std::shared_ptr<iinstance> itype::create_instance() {
    void* ptr = glz_create_instance(name_.c_str());
    if (!ptr) {
        throw std::runtime_error("Failed to create instance of type: " + name_);
    }
    return std::make_shared<iinstance>(ptr, shared_from_this(), true);
}

// iinstance implementation
iinstance::~iinstance() {
    if (owned_ && ptr_) {
        glz_destroy_instance(type_->name().c_str(), ptr_);
    }
}

ivalue iinstance::getfield(const std::string& field_name) const {
    const auto& f = type_->get_field(field_name);
    
    if (f.info_ && f.info_->getter) {
        void* field_ptr = f.info_->getter(const_cast<void*>(ptr_));
        
        // Return a ivalue that points directly to the field memory
        // This allows modification of the original data
        
        if (f.info_->type) {
            switch (f.info_->type->index) {
                case GLZ_TYPE_PRIMITIVE: {
                    // Handle primitive types based on kind
                    auto kind = f.info_->type->data.primitive.kind;
                    switch (kind) {
                        case 1: return ivalue(static_cast<bool*>(field_ptr));
                        case 4: return ivalue(static_cast<int32_t*>(field_ptr));
                        case 5: return ivalue(static_cast<int64_t*>(field_ptr));
                        case 10: return ivalue(static_cast<float*>(field_ptr));
                        case 11: return ivalue(static_cast<double*>(field_ptr));
                        default: break;
                    }
                    break;
                }
                case GLZ_TYPE_STRING: {
                    return ivalue(static_cast<std::string*>(field_ptr));
                }
                case GLZ_TYPE_VECTOR: {
                    // Return pointer to the actual vector
                    // Would need to determine element type for proper casting
                    return ivalue(static_cast<std::vector<ivalue>*>(field_ptr));
                }
                case GLZ_TYPE_STRUCT: {
                    // For nested structs, create a sub-instance that references the field
                    auto sub_instance = std::make_shared<iinstance>(
                        field_ptr, 
                        nullptr,  // Would need to get the actual type
                        false  // Not owned
                    );
                    return ivalue(sub_instance);
                }
                default:
                    break;
            }
        }
    }
    
    return ivalue();  // null value
}

void iinstance::setfield(const std::string& field_name, const ivalue& val) {
    const auto& f = type_->get_field(field_name);
    
    if (f.info_ && f.info_->setter) {
        // Convert ivalue to appropriate type and call setter
        // This would need proper type checking and conversion
        
        if (val.is_int()) {
            auto v = val.as_int();
            if (f.info_->type && f.info_->type->index == GLZ_TYPE_PRIMITIVE) {
                auto kind = f.info_->type->data.primitive.kind;
                if (kind == 4) {  // int32_t
                    int32_t i32_val = static_cast<int32_t>(v);
                    f.info_->setter(ptr_, &i32_val);
                } else if (kind == 5) {  // int64_t
                    f.info_->setter(ptr_, &v);
                }
            }
        } else if (val.is_string()) {
            auto str = val.as_string();
            f.info_->setter(ptr_, &str);
        } else if (val.is_float()) {
            auto v = val.as_float();
            if (f.info_->type && f.info_->type->index == GLZ_TYPE_PRIMITIVE) {
                auto kind = f.info_->type->data.primitive.kind;
                if (kind == 10) {  // float
                    float f_val = static_cast<float>(v);
                    f.info_->setter(ptr_, &f_val);
                } else if (kind == 11) {  // double
                    f.info_->setter(ptr_, &v);
                }
            }
        } else if (val.is_bool()) {
            bool v = val.as_bool();
            f.info_->setter(ptr_, &v);
        }
    }
}

std::string iinstance::to_json() const {
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& [name, f] : type_->fields()) {
        if (!first) oss << ",";
        oss << std::quoted(name) << ":";
        try {
            oss << getfield(name).to_json();
        } catch (...) {
            oss << "null";
        }
        first = false;
    }
    
    oss << "}";
    return oss.str();
}

std::shared_ptr<iinstance> iinstance::from_json(const std::string& json, std::shared_ptr<itype> t) {
    // This would need a proper JSON parser
    // For now, return a new instance
    return t->create_instance();
}

// glaze implementation
std::shared_ptr<itype> iglaze::get_type(const std::string& name) {
    // Check cache first
    auto it = type_registry_.find(name);
    if (it != type_registry_.end()) {
        return it->second;
    }
    
    // Get from C API
    auto* info = glz_get_type_info(name.c_str());
    if (!info) {
        throw std::runtime_error("type not found: " + name);
    }
    
    // Create and cache type wrapper
    auto t = std::make_shared<itype>(info);
    type_registry_[name] = t;
    return t;
}

std::vector<std::string> iglaze::list_types() {
    std::vector<std::string> result;
    for (const auto& [name, t] : type_registry_) {
        result.push_back(name);
    }
    return result;
}

std::shared_ptr<iinstance> iglaze::create_instance(const std::string& type_name) {
    auto t = get_type(type_name);
    return t->create_instance();
}

std::shared_ptr<iinstance> iglaze::get_instance(const std::string& name) {
    // Check cache first
    auto it = instance_registry_.find(name);
    if (it != instance_registry_.end()) {
        return it->second;
    }
    
    // Get from C API
    void* ptr = glz_get_instance(name.c_str());
    if (!ptr) {
        return nullptr;
    }
    
    const char* type_name = glz_get_instance_type(name.c_str());
    if (!type_name) {
        return nullptr;
    }
    
    auto t = get_type(type_name);
    auto inst = std::make_shared<iinstance>(ptr, t, false);
    instance_registry_[name] = inst;
    return inst;
}

std::vector<std::string> iglaze::list_instances() {
    std::vector<std::string> result;
    for (const auto& [name, inst] : instance_registry_) {
        result.push_back(name);
    }
    return result;
}

void iglaze::load_library(const std::string& path) {
    auto lib = std::make_unique<interop::interop_library>(path);
    loaded_libraries_.push_back(std::move(lib));
    
    // The library should self-register its types
    // Alternatively, we could scan for types here
}

void iglaze::unload_all_libraries() {
    loaded_libraries_.clear();
    // Clear registries that came from libraries
    // (keeping track of which types came from where would be needed)
}

} // namespace glz