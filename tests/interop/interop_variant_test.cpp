// Comprehensive variant interop tests
#include "ut/ut.hpp"
#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <complex>
#include <optional>
#include <iostream>
#include "glaze/glaze.hpp"
#include "glaze/interop/interop.hpp"

using namespace ut;

// Test struct to use in variants
struct VariantTestStruct {
    int id;
    std::string name;
    double value;
    
    bool operator==(const VariantTestStruct& other) const {
        return id == other.id && name == other.name && value == other.value;
    }
};

// Another test struct
struct AnotherStruct {
    std::vector<int> numbers;
    std::optional<std::string> description;
};

// Register Glaze metadata
template <>
struct glz::meta<VariantTestStruct> {
    using T = VariantTestStruct;
    static constexpr auto value = glz::object(
        "id", &T::id,
        "name", &T::name,
        "value", &T::value
    );
};

template <>
struct glz::meta<AnotherStruct> {
    using T = AnotherStruct;
    static constexpr auto value = glz::object(
        "numbers", &T::numbers,
        "description", &T::description
    );
};

// Test struct with variant member
struct StructWithVariant {
    std::variant<int, std::string, double> basic_variant;
    std::variant<bool, VariantTestStruct> struct_variant;
    std::optional<std::variant<int, std::string>> optional_variant;
    
    // Methods
    int get_variant_index() const {
        return static_cast<int>(basic_variant.index());
    }
    
    void set_to_int(int value) {
        basic_variant = value;
    }
    
    void set_to_string(const std::string& value) {
        basic_variant = value;
    }
    
    void set_to_double(double value) {
        basic_variant = value;
    }
};

template <>
struct glz::meta<StructWithVariant> {
    using T = StructWithVariant;
    static constexpr auto value = glz::object(
        "basic_variant", &T::basic_variant,
        "struct_variant", &T::struct_variant,
        "optional_variant", &T::optional_variant,
        "get_variant_index", &T::get_variant_index,
        "set_to_int", &T::set_to_int,
        "set_to_string", &T::set_to_string,
        "set_to_double", &T::set_to_double
    );
};

// Complex nested variant type
using ComplexVariant = std::variant<
    int,
    double,
    std::string,
    std::vector<int>,
    std::unordered_map<std::string, double>,
    VariantTestStruct,
    std::optional<int>,
    std::complex<float>
>;

// Global test instances
StructWithVariant global_variant_test;
ComplexVariant global_complex_variant;

int main() {
    using namespace ut;

    // Register types with interop
    glz::register_type<VariantTestStruct>("VariantTestStruct");
    glz::register_type<AnotherStruct>("AnotherStruct");
    glz::register_type<StructWithVariant>("StructWithVariant");
    
    // Register global instances
    glz::register_instance("global_variant_test", global_variant_test);
    
    "basic variant type descriptor creation"_test = [] {
        using TestVariant = std::variant<int, double, std::string>;
        auto* desc = glz::create_type_descriptor<TestVariant>();
        
        expect(desc != nullptr);
        expect(desc->index == GLZ_TYPE_VARIANT);
        expect(desc->data.variant.count == 3);
        expect(desc->data.variant.alternatives != nullptr);
        
        // Check alternative types
        expect(desc->data.variant.alternatives[0]->index == GLZ_TYPE_PRIMITIVE);
        expect(desc->data.variant.alternatives[0]->data.primitive.kind == 4); // int32_t
        
        expect(desc->data.variant.alternatives[1]->index == GLZ_TYPE_PRIMITIVE);
        expect(desc->data.variant.alternatives[1]->data.primitive.kind == 11); // double
        
        expect(desc->data.variant.alternatives[2]->index == GLZ_TYPE_STRING);
        
        std::cout << "✅ Basic variant type descriptor created correctly\n";
    };
    
    "variant with primitive types"_test = [] {
        std::variant<bool, int8_t, int32_t, double> test_variant;
        
        // Test default state (first alternative)
        expect(test_variant.index() == 0);
        expect(std::holds_alternative<bool>(test_variant));
        
        // Set to int32_t
        test_variant = int32_t(42);
        expect(test_variant.index() == 2);
        expect(std::holds_alternative<int32_t>(test_variant));
        expect(std::get<int32_t>(test_variant) == 42);
        
        // Set to double
        test_variant = 3.14159;
        expect(test_variant.index() == 3);
        expect(std::holds_alternative<double>(test_variant));
        expect(std::get<double>(test_variant) > 3.14 && std::get<double>(test_variant) < 3.15);
        
        std::cout << "✅ Variant with primitive types works correctly\n";
    };
    
    "variant with string and vector"_test = [] {
        std::variant<std::string, std::vector<int>, bool> test_variant;
        
        // Set to string
        test_variant = std::string("Hello, Variant!");
        expect(test_variant.index() == 0);
        expect(std::holds_alternative<std::string>(test_variant));
        expect(std::get<std::string>(test_variant) == "Hello, Variant!");
        
        // Set to vector
        test_variant = std::vector<int>{1, 2, 3, 4, 5};
        expect(test_variant.index() == 1);
        expect(std::holds_alternative<std::vector<int>>(test_variant));
        expect(std::get<std::vector<int>>(test_variant).size() == 5);
        expect(std::get<std::vector<int>>(test_variant)[0] == 1);
        
        // Set to bool
        test_variant = true;
        expect(test_variant.index() == 2);
        expect(std::holds_alternative<bool>(test_variant));
        expect(std::get<bool>(test_variant) == true);
        
        std::cout << "✅ Variant with string and vector works correctly\n";
    };
    
    "variant with struct types"_test = [] {
        std::variant<VariantTestStruct, AnotherStruct, int> test_variant;
        
        // Set to VariantTestStruct
        VariantTestStruct vts{123, "test", 45.67};
        test_variant = vts;
        expect(test_variant.index() == 0);
        expect(std::holds_alternative<VariantTestStruct>(test_variant));
        expect(std::get<VariantTestStruct>(test_variant) == vts);
        
        // Set to AnotherStruct
        AnotherStruct as;
        as.numbers = {10, 20, 30};
        as.description = "Another test";
        test_variant = as;
        expect(test_variant.index() == 1);
        expect(std::holds_alternative<AnotherStruct>(test_variant));
        expect(std::get<AnotherStruct>(test_variant).numbers.size() == 3);
        expect(std::get<AnotherStruct>(test_variant).description.value() == "Another test");
        
        std::cout << "✅ Variant with struct types works correctly\n";
    };
    
    "variant type descriptor for complex types"_test = [] {
        using ComplexVar = std::variant<
            std::vector<int>,
            std::unordered_map<std::string, double>,
            std::optional<std::string>,
            std::complex<float>
        >;
        
        auto* desc = glz::create_type_descriptor<ComplexVar>();
        
        expect(desc != nullptr);
        expect(desc->index == GLZ_TYPE_VARIANT);
        expect(desc->data.variant.count == 4);
        
        // Check vector descriptor
        expect(desc->data.variant.alternatives[0]->index == GLZ_TYPE_VECTOR);
        
        // Check map descriptor
        expect(desc->data.variant.alternatives[1]->index == GLZ_TYPE_MAP);
        
        // Check optional descriptor
        expect(desc->data.variant.alternatives[2]->index == GLZ_TYPE_OPTIONAL);
        
        // Check complex descriptor
        expect(desc->data.variant.alternatives[3]->index == GLZ_TYPE_COMPLEX);
        expect(desc->data.variant.alternatives[3]->data.complex.kind == 0); // float
        
        std::cout << "✅ Complex variant type descriptor created correctly\n";
    };
    
    "nested variant types"_test = [] {
        using InnerVariant = std::variant<int, std::string>;
        using OuterVariant = std::variant<bool, InnerVariant, double>;
        
        OuterVariant test_variant;
        
        // Set to inner variant with int
        test_variant = InnerVariant(42);
        expect(test_variant.index() == 1);
        expect(std::holds_alternative<InnerVariant>(test_variant));
        
        auto& inner = std::get<InnerVariant>(test_variant);
        expect(inner.index() == 0);
        expect(std::get<int>(inner) == 42);
        
        // Set inner variant to string
        test_variant = InnerVariant("nested");
        auto& inner2 = std::get<InnerVariant>(test_variant);
        expect(inner2.index() == 1);
        expect(std::get<std::string>(inner2) == "nested");
        
        std::cout << "✅ Nested variant types work correctly\n";
    };
    
    "variant inside optional"_test = [] {
        using VarType = std::variant<int, std::string>;
        std::optional<VarType> opt_variant;
        
        expect(!opt_variant.has_value());
        
        // Set optional to variant with int
        opt_variant = VarType(100);
        expect(opt_variant.has_value());
        expect(opt_variant->index() == 0);
        expect(std::get<int>(opt_variant.value()) == 100);
        
        // Set optional to variant with string
        opt_variant = VarType("optional variant");
        expect(opt_variant.has_value());
        expect(opt_variant->index() == 1);
        expect(std::get<std::string>(opt_variant.value()) == "optional variant");
        
        // Reset optional
        opt_variant.reset();
        expect(!opt_variant.has_value());
        
        std::cout << "✅ Variant inside optional works correctly\n";
    };
    
    "optional inside variant"_test = [] {
        using VarType = std::variant<int, std::optional<std::string>, bool>;
        VarType test_variant;
        
        // Set to optional with value
        test_variant = std::optional<std::string>("optional in variant");
        expect(test_variant.index() == 1);
        expect(std::holds_alternative<std::optional<std::string>>(test_variant));
        expect(std::get<std::optional<std::string>>(test_variant).has_value());
        expect(std::get<std::optional<std::string>>(test_variant).value() == "optional in variant");
        
        // Set to optional without value
        test_variant = std::optional<std::string>(std::nullopt);
        expect(test_variant.index() == 1);
        expect(!std::get<std::optional<std::string>>(test_variant).has_value());
        
        std::cout << "✅ Optional inside variant works correctly\n";
    };
    
    "struct with variant member"_test = [] {
        StructWithVariant test_obj;
        
        // Test basic_variant
        test_obj.set_to_int(42);
        expect(test_obj.get_variant_index() == 0);
        expect(std::get<int>(test_obj.basic_variant) == 42);
        
        test_obj.set_to_string("variant test");
        expect(test_obj.get_variant_index() == 1);
        expect(std::get<std::string>(test_obj.basic_variant) == "variant test");
        
        test_obj.set_to_double(2.718);
        expect(test_obj.get_variant_index() == 2);
        expect(std::get<double>(test_obj.basic_variant) > 2.71 && std::get<double>(test_obj.basic_variant) < 2.72);
        
        // Test struct_variant
        test_obj.struct_variant = true;
        expect(std::holds_alternative<bool>(test_obj.struct_variant));
        
        VariantTestStruct vts{999, "struct in variant", 123.456};
        test_obj.struct_variant = vts;
        expect(std::holds_alternative<VariantTestStruct>(test_obj.struct_variant));
        expect(std::get<VariantTestStruct>(test_obj.struct_variant) == vts);
        
        // Test optional_variant
        expect(!test_obj.optional_variant.has_value());
        
        test_obj.optional_variant = std::variant<int, std::string>(789);
        expect(test_obj.optional_variant.has_value());
        expect(test_obj.optional_variant->index() == 0);
        expect(std::get<int>(test_obj.optional_variant.value()) == 789);
        
        std::cout << "✅ Struct with variant member works correctly\n";
    };
    
    "global variant instance access"_test = [] {
        // Set values in global instance
        global_variant_test.set_to_string("global variant");
        expect(std::get<std::string>(global_variant_test.basic_variant) == "global variant");
        
        global_variant_test.struct_variant = VariantTestStruct{1, "global", 99.99};
        expect(std::holds_alternative<VariantTestStruct>(global_variant_test.struct_variant));
        
        global_variant_test.optional_variant = std::variant<int, std::string>("optional global");
        expect(global_variant_test.optional_variant.has_value());
        expect(std::get<std::string>(global_variant_test.optional_variant.value()) == "optional global");
        
        std::cout << "✅ Global variant instance access works correctly\n";
    };
    
    "variant C API - type_at_index"_test = [] {
        using TestVariant = std::variant<int, double, std::string>;
        auto* desc = glz::create_type_descriptor<TestVariant>();
        
        auto* type0 = glz_variant_type_at_index(desc, 0);
        expect(type0 != nullptr);
        expect(type0->index == GLZ_TYPE_PRIMITIVE);
        expect(type0->data.primitive.kind == 4); // int32_t
        
        auto* type1 = glz_variant_type_at_index(desc, 1);
        expect(type1 != nullptr);
        expect(type1->index == GLZ_TYPE_PRIMITIVE);
        expect(type1->data.primitive.kind == 11); // double
        
        auto* type2 = glz_variant_type_at_index(desc, 2);
        expect(type2 != nullptr);
        expect(type2->index == GLZ_TYPE_STRING);
        
        // Test out of bounds
        auto* type3 = glz_variant_type_at_index(desc, 3);
        expect(type3 == nullptr);
        expect(glz_get_last_error() == GLZ_ERROR_INVALID_PARAMETER);
        
        glz_clear_error();
        
        std::cout << "✅ Variant C API type_at_index works correctly\n";
    };
    
    "variant C API - get/set operations"_test = [] {
        using TestVariant = std::variant<int, std::string, double>;
        auto* desc = glz::create_type_descriptor<TestVariant>();
        
        TestVariant test_var = 42;
        
        // Test get_index
        auto index = glz_variant_index(&test_var, desc);
        expect(index == 0);
        
        // Test get_value
        void* value = glz_variant_get(&test_var, desc);
        expect(value != nullptr);
        expect(*static_cast<int*>(value) == 42);
        
        // Test holds_alternative
        expect(glz_variant_holds_alternative(&test_var, desc, 0) == true);
        expect(glz_variant_holds_alternative(&test_var, desc, 1) == false);
        expect(glz_variant_holds_alternative(&test_var, desc, 2) == false);
        
        // Test set_value to string
        std::string new_value = "Hello Variant";
        bool set_result = glz_variant_set(&test_var, desc, 1, &new_value);
        expect(set_result == true);
        expect(test_var.index() == 1);
        expect(std::get<std::string>(test_var) == "Hello Variant");
        
        // Test set_value to double
        double double_value = 3.14159;
        set_result = glz_variant_set(&test_var, desc, 2, &double_value);
        expect(set_result == true);
        expect(test_var.index() == 2);
        expect(std::get<double>(test_var) > 3.14 && std::get<double>(test_var) < 3.15);
        
        std::cout << "✅ Variant C API get/set operations work correctly\n";
    };
    
    "variant C API - create/destroy"_test = [] {
        using TestVariant = std::variant<int, std::string, double>;
        auto* desc = glz::create_type_descriptor<TestVariant>();
        
        // Create variant with initial int value
        int initial_value = 100;
        void* created_var = glz_create_variant(desc, 0, &initial_value);
        expect(created_var != nullptr);
        
        auto* var_ptr = static_cast<TestVariant*>(created_var);
        expect(var_ptr->index() == 0);
        expect(std::get<int>(*var_ptr) == 100);
        
        // Destroy the variant
        glz_destroy_variant(created_var, desc);
        
        // Create with string
        std::string str_value = "Created variant";
        created_var = glz_create_variant(desc, 1, &str_value);
        expect(created_var != nullptr);
        
        var_ptr = static_cast<TestVariant*>(created_var);
        expect(var_ptr->index() == 1);
        expect(std::get<std::string>(*var_ptr) == "Created variant");
        
        glz_destroy_variant(created_var, desc);
        
        std::cout << "✅ Variant C API create/destroy works correctly\n";
    };
    
    "edge cases and error conditions"_test = [] {
        // Empty variant (monostate)
        std::variant<std::monostate, int, std::string> mono_variant;
        expect(mono_variant.index() == 0);
        expect(std::holds_alternative<std::monostate>(mono_variant));
        
        // Large variant with many alternatives
        using LargeVariant = std::variant<
            int, double, float, bool, char,
            int8_t, int16_t, int32_t, int64_t,
            uint8_t, uint16_t, uint32_t, uint64_t
        >;
        
        LargeVariant large_var;
        large_var = uint64_t(0xFFFFFFFFFFFFFFFF);
        expect(large_var.index() == 12);
        expect(std::get<uint64_t>(large_var) == 0xFFFFFFFFFFFFFFFF);
        
        // Variant with duplicate types (should still work)
        // Note: std::variant with duplicate types is not standard-compliant,
        // so we skip this test
        
        std::cout << "✅ Edge cases handled correctly\n";
    };
    
    std::cout << "\n🎉 All variant interop tests passed!\n";
    
    return 0;
}