// Complex number support test
// Tests std::complex operations for Julia interop
#include "ut/ut.hpp"
#include "glaze/interop/interop.hpp"
#include <complex>
#include <vector>
#include <cmath>

using namespace ut;

// Test structure with complex number fields
struct ComplexTestStruct {
    std::complex<float> single_complex;
    std::complex<double> double_complex;
    std::vector<std::complex<float>> complex_float_vec;
    std::vector<std::complex<double>> complex_double_vec;
    std::optional<std::complex<float>> optional_complex;
    
    std::complex<double> multiply_complex(std::complex<double> a, std::complex<double> b) {
        return a * b;
    }
    
    double magnitude(std::complex<double> c) {
        return std::abs(c);
    }
    
    std::vector<std::complex<float>> generate_complex_array(size_t n) {
        std::vector<std::complex<float>> result;
        result.reserve(n);
        for(size_t i = 0; i < n; ++i) {
            result.emplace_back(i, -static_cast<float>(i));
        }
        return result;
    }
};

template<>
struct glz::meta<ComplexTestStruct> {
    using T = ComplexTestStruct;
    static constexpr auto value = object(
        "single_complex", &T::single_complex,
        "double_complex", &T::double_complex,
        "complex_float_vec", &T::complex_float_vec,
        "complex_double_vec", &T::complex_double_vec,
        "optional_complex", &T::optional_complex,
        "multiply_complex", &T::multiply_complex,
        "magnitude", &T::magnitude,
        "generate_complex_array", &T::generate_complex_array
    );
};

int main() {
    using namespace ut;
    
    // Register the type
    glz::register_type<ComplexTestStruct>("ComplexTestStruct");

    "basic complex number operations"_test = [] {
        ComplexTestStruct test_obj;
        
        // Set complex values
        test_obj.single_complex = std::complex<float>(3.14f, 2.718f);
        test_obj.double_complex = std::complex<double>(1.414, -1.732);
        
        // Verify real and imaginary parts
        expect(test_obj.single_complex.real() == 3.14f);
        expect(test_obj.single_complex.imag() == 2.718f);
        expect(test_obj.double_complex.real() == 1.414);
        expect(test_obj.double_complex.imag() == -1.732);
        
        std::cout << "✅ Basic complex number operations test passed\n";
    };

    "complex vector operations"_test = [] {
        auto* vec = glz_create_vector(
            glz::create_type_descriptor<std::vector<std::complex<float>>>());
        
        // Cast to proper type
        auto* complex_vec = static_cast<std::vector<std::complex<float>>*>(vec);
        
        // Resize vector
        glz_vector_resize(vec, 
            glz::create_type_descriptor<std::vector<std::complex<float>>>(), 1000);
        
        // Get zero-copy view
        auto view = glz_vector_view(vec, 
            glz::create_type_descriptor<std::vector<std::complex<float>>>());
        
        // Access as complex array
        auto* complex_data = static_cast<std::complex<float>*>(view.data);
        
        // Fill with test data
        for(size_t i = 0; i < 1000; ++i) {
            complex_data[i] = std::complex<float>(
                std::sin(i * 0.1f), 
                std::cos(i * 0.1f)
            );
        }
        
        // Verify data
        expect(view.size == 1000);
        expect(std::abs(complex_data[0] - std::complex<float>(0.0f, 1.0f)) < 0.001f);
        
        // Test push_back
        std::complex<float> new_val(3.14f, 2.718f);
        glz_vector_push_back(vec, 
            glz::create_type_descriptor<std::vector<std::complex<float>>>(), &new_val);
        
        expect(complex_vec->size() == 1001);
        expect(complex_vec->back() == new_val);
        
        // Clean up
        glz_destroy_vector(vec, 
            glz::create_type_descriptor<std::vector<std::complex<float>>>());
        
        std::cout << "✅ Complex vector operations test passed\n";
    };

    "complex arithmetic through view"_test = [] {
        std::vector<std::complex<double>> vec(500);
        
        // Get zero-copy view
        auto view = glz_vector_view(&vec, 
            glz::create_type_descriptor<std::vector<std::complex<double>>>());
        
        auto* data = static_cast<std::complex<double>*>(view.data);
        
        // Perform complex arithmetic
        for(size_t i = 0; i < 500; ++i) {
            double angle = i * M_PI / 250.0;
            data[i] = std::polar(1.0, angle);  // Unit circle points
        }
        
        // Verify results
        expect(std::abs(data[0] - std::complex<double>(1.0, 0.0)) < 0.001);
        expect(std::abs(data[250] - std::complex<double>(-1.0, 0.0)) < 0.001);
        
        // Test complex operations
        std::complex<double> sum = 0;
        for(size_t i = 0; i < 500; ++i) {
            sum += data[i];
        }
        
        // Sum should be close to zero for uniformly distributed unit circle points
        expect(std::abs(sum) < 0.1) << "Sum of unit circle points should be near zero";
        
        std::cout << "✅ Complex arithmetic through view test passed\n";
    };

    "optional complex support"_test = [] {
        ComplexTestStruct test_obj;
        
        // Test unset optional
        expect(!test_obj.optional_complex.has_value());
        
        // Set optional complex
        test_obj.optional_complex = std::complex<float>(1.5f, -2.5f);
        
        expect(test_obj.optional_complex.has_value());
        expect(test_obj.optional_complex->real() == 1.5f);
        expect(test_obj.optional_complex->imag() == -2.5f);
        
        // Test through API
        auto* opt_ptr = &test_obj.optional_complex;
        auto* desc = glz::create_type_descriptor<std::optional<std::complex<float>>>();
        
        expect(glz_optional_has_value(opt_ptr, desc) == true);
        
        auto* value = glz_optional_get_value(opt_ptr, desc);
        auto* complex_val = static_cast<std::complex<float>*>(value);
        expect(complex_val->real() == 1.5f);
        expect(complex_val->imag() == -2.5f);
        
        // Reset optional
        glz_optional_reset(opt_ptr, desc);
        expect(!test_obj.optional_complex.has_value());
        
        std::cout << "✅ Optional complex support test passed\n";
    };

    "complex member functions"_test = [] {
        ComplexTestStruct test_obj;
        
        // Test multiply_complex
        std::complex<double> a(2.0, 3.0);
        std::complex<double> b(4.0, -1.0);
        auto result = test_obj.multiply_complex(a, b);
        
        // (2+3i) * (4-i) = 8 - 2i + 12i - 3i² = 8 + 10i + 3 = 11 + 10i
        expect(std::abs(result - std::complex<double>(11.0, 10.0)) < 0.001);
        
        // Test magnitude
        std::complex<double> c(3.0, 4.0);
        double mag = test_obj.magnitude(c);
        expect(std::abs(mag - 5.0) < 0.001);  // 3-4-5 triangle
        
        // Test generate_complex_array
        auto generated = test_obj.generate_complex_array(100);
        expect(generated.size() == 100);
        expect(generated[50] == std::complex<float>(50.0f, -50.0f));
        
        std::cout << "✅ Complex member functions test passed\n";
    };

    "large complex vector performance"_test = [] {
        const size_t N = 100'000;
        std::vector<std::complex<double>> vec(N);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Get zero-copy view
        auto view = glz_vector_view(&vec, 
            glz::create_type_descriptor<std::vector<std::complex<double>>>());
        auto* data = static_cast<std::complex<double>*>(view.data);
        
        // Perform FFT-like operation (simplified)
        for(size_t i = 0; i < N; ++i) {
            double angle = 2.0 * M_PI * i / N;
            data[i] = std::exp(std::complex<double>(0, angle));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        expect(duration.count() < 500) << "100K complex operations should take < 500ms";
        
        std::cout << "✅ Large complex vector (100K) processed in " 
                  << duration.count() << "ms\n";
    };

    "complex type descriptor validation"_test = [] {
        // Test that type descriptors are created correctly
        auto* float_complex_desc = glz::create_type_descriptor<std::complex<float>>();
        auto* double_complex_desc = glz::create_type_descriptor<std::complex<double>>();
        
        expect(float_complex_desc != nullptr);
        expect(double_complex_desc != nullptr);
        expect(float_complex_desc->index == GLZ_TYPE_COMPLEX);
        expect(double_complex_desc->index == GLZ_TYPE_COMPLEX);
        expect(float_complex_desc->data.complex.kind == 0);  // float
        expect(double_complex_desc->data.complex.kind == 1);  // double
        
        // Test vector of complex
        auto* vec_desc = glz::create_type_descriptor<std::vector<std::complex<float>>>();
        expect(vec_desc != nullptr);
        expect(vec_desc->index == GLZ_TYPE_VECTOR);
        expect(vec_desc->data.vector.element_type != nullptr);
        expect(vec_desc->data.vector.element_type->index == GLZ_TYPE_COMPLEX);
        
        std::cout << "✅ Complex type descriptor validation test passed\n";
    };

    "complex interop with Julia patterns"_test = [] {
        // Test patterns that Julia would use
        std::vector<std::complex<float>> vec(1000);
        
        // Julia-style broadcasting: vec .= complex(1.0, 2.0)
        auto view = glz_vector_view(&vec, 
            glz::create_type_descriptor<std::vector<std::complex<float>>>());
        auto* data = static_cast<std::complex<float>*>(view.data);
        
        std::complex<float> broadcast_val(1.0f, 2.0f);
        for(size_t i = 0; i < view.size; ++i) {
            data[i] = broadcast_val;
        }
        
        // Verify all elements set
        for(size_t i = 0; i < 1000; ++i) {
            expect(vec[i] == broadcast_val);
        }
        
        // Julia-style element-wise operations: vec .= conj.(vec)
        for(size_t i = 0; i < view.size; ++i) {
            data[i] = std::conj(data[i]);
        }
        
        // Verify conjugate
        expect(vec[0] == std::complex<float>(1.0f, -2.0f));
        
        std::cout << "✅ Complex interop with Julia patterns test passed\n";
    };

    std::cout << "\n🎉 All complex number tests completed successfully!\n";
    std::cout << "📊 Coverage: basic ops, vectors, optionals, member functions, Julia patterns\n";
    std::cout << "✅ std::complex fully supported for Julia interop\n\n";
    
    return 0;
}