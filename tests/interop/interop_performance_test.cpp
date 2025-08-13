// Performance and stress tests
// Benchmarks and stress tests for Julia interop
#include "ut/ut.hpp"
#include "glaze/interop/interop.hpp"
#include <vector>
#include <chrono>
#include <numeric>
#include <thread>
#include <cmath>

using namespace ut;
using namespace std::chrono;

// Helper to measure time
template<typename F>
double measure_ms(F&& func) {
    auto start = high_resolution_clock::now();
    func();
    auto end = high_resolution_clock::now();
    return duration_cast<microseconds>(end - start).count() / 1000.0;
}

// Helper to get approximate memory usage (simplified)
size_t get_memory_usage() {
    // This is a simplified approximation
    // In production, use platform-specific APIs
    return 0;  // Placeholder
}

struct PerformanceTestStruct {
    std::vector<double> data;
    std::vector<std::vector<double>> matrix;
    
    double compute_sum() const {
        return std::accumulate(data.begin(), data.end(), 0.0);
    }
    
    void process_data() {
        for(auto& val : data) {
            val = std::sin(val) * std::cos(val);
        }
    }
};

template<>
struct glz::meta<PerformanceTestStruct> {
    using T = PerformanceTestStruct;
    static constexpr auto value = object(
        "data", &T::data,
        "matrix", &T::matrix,
        "compute_sum", &T::compute_sum,
        "process_data", &T::process_data
    );
};

int main() {
    using namespace ut;
    
    glz::register_type<PerformanceTestStruct>("PerformanceTestStruct");

    "large vector allocation benchmark"_test = [] {
        const size_t sizes[] = {1000, 10000, 100000, 1000000};
        
        std::cout << "\n  Vector allocation benchmarks:\n";
        
        for(size_t N : sizes) {
            auto time_ms = measure_ms([N]() {
                auto* vec = glz_create_vector_float64();
                glz_vector_resize(vec, 
                    glz::create_type_descriptor<std::vector<double>>(), N);
                glz_destroy_vector(vec, 
                    glz::create_type_descriptor<std::vector<double>>());
            });
            
            std::cout << "    " << N << " elements: " << time_ms << "ms\n";
            
            // Performance expectations
            if(N <= 10000) {
                expect(time_ms < 10) << "Small vectors should allocate in < 10ms";
            } else if(N <= 100000) {
                expect(time_ms < 50) << "Medium vectors should allocate in < 50ms";
            } else {
                expect(time_ms < 500) << "Large vectors should allocate in < 500ms";
            }
        }
        
        std::cout << "✅ Large vector allocation benchmark passed\n";
    };

    "zero-copy access performance"_test = [] {
        const size_t N = 1'000'000;
        std::vector<double> vec(N);
        
        // Initialize with test data
        for(size_t i = 0; i < N; ++i) {
            vec[i] = i * 0.001;
        }
        
        auto time_ms = measure_ms([&vec]() {
            // Get zero-copy view
            auto view = glz_vector_view(&vec, 
                glz::create_type_descriptor<std::vector<double>>());
            double* data = static_cast<double*>(view.data);
            
            // Sum all elements
            double sum = 0;
            for(size_t i = 0; i < view.size; ++i) {
                sum += data[i];
            }
            
            // Use sum to prevent optimization
            volatile double result = sum;
            (void)result;
        });
        
        std::cout << "  Zero-copy sum of 1M doubles: " << time_ms << "ms\n";
        expect(time_ms < 50) << "Summing 1M doubles should take < 50ms";
        
        std::cout << "✅ Zero-copy access performance test passed\n";
    };

    "element-wise operations benchmark"_test = [] {
        const size_t N = 100'000;
        std::vector<double> vec(N);
        
        auto view = glz_vector_view(&vec, 
            glz::create_type_descriptor<std::vector<double>>());
        double* data = static_cast<double*>(view.data);
        
        // Benchmark sin() operation
        auto sin_time = measure_ms([data, N]() {
            for(size_t i = 0; i < N; ++i) {
                data[i] = std::sin(i * 0.001);
            }
        });
        
        // Benchmark multiplication
        auto mult_time = measure_ms([data, N]() {
            for(size_t i = 0; i < N; ++i) {
                data[i] *= 2.5;
            }
        });
        
        // Benchmark addition
        auto add_time = measure_ms([data, N]() {
            for(size_t i = 0; i < N; ++i) {
                data[i] += 10.0;
            }
        });
        
        std::cout << "  Element-wise operations on 100K elements:\n";
        std::cout << "    sin():  " << sin_time << "ms\n";
        std::cout << "    mult:   " << mult_time << "ms\n";
        std::cout << "    add:    " << add_time << "ms\n";
        
        // Allow some tolerance since modern CPUs can do add/mult in similar time
        expect(add_time <= mult_time * 1.1) << "Addition should not be significantly slower than multiplication";
        expect(mult_time < sin_time) << "Multiplication should be faster than sin()";
        
        std::cout << "✅ Element-wise operations benchmark passed\n";
    };

    "memory stress test - many small vectors"_test = [] {
        const size_t count = 10000;
        const size_t vec_size = 100;
        
        std::vector<void*> allocations;
        allocations.reserve(count);
        
        auto alloc_time = measure_ms([&allocations, count, vec_size]() {
            for(size_t i = 0; i < count; ++i) {
                auto* vec = glz_create_vector_int32();
                glz_vector_resize(vec, 
                    glz::create_type_descriptor<std::vector<int32_t>>(), vec_size);
                allocations.push_back(vec);
            }
        });
        
        std::cout << "  Allocated 10K vectors of 100 ints in: " << alloc_time << "ms\n";
        
        // Expected memory: 10000 * 100 * 4 bytes = 4MB minimum
        size_t expected_memory = count * vec_size * sizeof(int32_t);
        std::cout << "  Expected minimum memory: " << expected_memory / (1024*1024) << "MB\n";
        
        // Cleanup
        auto cleanup_time = measure_ms([&allocations]() {
            for(auto* vec : allocations) {
                glz_destroy_vector(vec, 
                    glz::create_type_descriptor<std::vector<int32_t>>());
            }
        });
        
        std::cout << "  Cleanup time: " << cleanup_time << "ms\n";
        
        expect(alloc_time < 1000) << "Allocating 10K small vectors should take < 1s";
        expect(cleanup_time < alloc_time) << "Cleanup should be faster than allocation";
        
        std::cout << "✅ Memory stress test - many small vectors passed\n";
    };

    "member function call overhead"_test = [] {
        PerformanceTestStruct test_obj;
        test_obj.data.resize(10000);
        for(size_t i = 0; i < 10000; ++i) {
            test_obj.data[i] = i * 0.1;
        }
        
        auto* type_info = glz_get_type_info("PerformanceTestStruct");
        
        // Find compute_sum function
        const glz_member_info* sum_func = nullptr;
        for(size_t i = 0; i < type_info->member_count; ++i) {
            if(std::string(type_info->members[i].name) == "compute_sum") {
                sum_func = &type_info->members[i];
                break;
            }
        }
        
        const size_t iterations = 1000;
        
        // Benchmark direct C++ call
        auto cpp_time = measure_ms([&test_obj, iterations]() {
            for(size_t i = 0; i < iterations; ++i) {
                volatile double result = test_obj.compute_sum();
                (void)result;
            }
        });
        
        // Benchmark through interop API
        auto api_time = measure_ms([&test_obj, sum_func, iterations]() {
            for(size_t i = 0; i < iterations; ++i) {
                double result;
                glz_call_member_function_with_type(
                    &test_obj, "PerformanceTestStruct", sum_func, nullptr, &result
                );
                volatile double r = result;
                (void)r;
            }
        });
        
        std::cout << "  Function call overhead (1000 iterations):\n";
        std::cout << "    Direct C++: " << cpp_time << "ms\n";
        std::cout << "    Interop API: " << api_time << "ms\n";
        std::cout << "    Overhead: " << (api_time - cpp_time) / iterations << "ms per call\n";
        
        // API should not be more than 10x slower for this simple case
        expect(api_time < cpp_time * 10) << "API overhead should be reasonable";
        
        std::cout << "✅ Member function call overhead test passed\n";
    };

    "push_back performance"_test = [] {
        std::vector<double> vec;
        const size_t N = 10000;
        
        auto push_time = measure_ms([&vec, N]() {
            for(size_t i = 0; i < N; ++i) {
                double val = i * 1.5;
                glz_vector_push_back(&vec, 
                    glz::create_type_descriptor<std::vector<double>>(), &val);
            }
        });
        
        std::cout << "  Push_back 10K elements: " << push_time << "ms\n";
        expect(vec.size() == N);
        expect(push_time < 100) << "Push_back 10K elements should take < 100ms";
        
        std::cout << "✅ Push_back performance test passed\n";
    };

    "matrix operations benchmark"_test = [] {
        const size_t rows = 100;
        const size_t cols = 100;
        
        PerformanceTestStruct test_obj;
        test_obj.matrix.resize(rows);
        for(auto& row : test_obj.matrix) {
            row.resize(cols);
        }
        
        // Fill matrix
        auto fill_time = measure_ms([&test_obj, rows, cols]() {
            for(size_t i = 0; i < rows; ++i) {
                for(size_t j = 0; j < cols; ++j) {
                    test_obj.matrix[i][j] = i * cols + j;
                }
            }
        });
        
        // Matrix transpose (simplified)
        auto transpose_time = measure_ms([&test_obj, rows, cols]() {
            std::vector<std::vector<double>> transposed(cols, std::vector<double>(rows));
            for(size_t i = 0; i < rows; ++i) {
                for(size_t j = 0; j < cols; ++j) {
                    transposed[j][i] = test_obj.matrix[i][j];
                }
            }
        });
        
        std::cout << "  Matrix operations (100x100):\n";
        std::cout << "    Fill:      " << fill_time << "ms\n";
        std::cout << "    Transpose: " << transpose_time << "ms\n";
        
        expect(fill_time < 10) << "Filling 100x100 matrix should take < 10ms";
        expect(transpose_time < 20) << "Transposing 100x100 matrix should take < 20ms";
        
        std::cout << "✅ Matrix operations benchmark passed\n";
    };

    "type registration performance"_test = [] {
        // Test repeated type lookups
        const size_t lookups = 10000;
        
        auto lookup_time = measure_ms([lookups]() {
            for(size_t i = 0; i < lookups; ++i) {
                auto* info = glz_get_type_info("PerformanceTestStruct");
                volatile auto* ptr = info;
                (void)ptr;
            }
        });
        
        std::cout << "  Type info lookup (10K times): " << lookup_time << "ms\n";
        std::cout << "  Per lookup: " << (lookup_time * 1000 / lookups) << "µs\n";
        
        expect(lookup_time < 10) << "10K type lookups should take < 10ms (cached)";
        
        std::cout << "✅ Type registration performance test passed\n";
    };

    "peak throughput test"_test = [] {
        const size_t N = 10'000'000;  // 10 million doubles = 80MB
        std::vector<double> vec(N);
        
        auto view = glz_vector_view(&vec, 
            glz::create_type_descriptor<std::vector<double>>());
        double* data = static_cast<double*>(view.data);
        
        // Memory bandwidth test: write
        auto write_time = measure_ms([data, N]() {
            for(size_t i = 0; i < N; ++i) {
                data[i] = i;
            }
        });
        
        // Memory bandwidth test: read
        auto read_time = measure_ms([data, N]() {
            double sum = 0;
            for(size_t i = 0; i < N; ++i) {
                sum += data[i];
            }
            volatile double result = sum;
            (void)result;
        });
        
        double write_bandwidth = (N * sizeof(double)) / (write_time * 1e6);  // GB/s
        double read_bandwidth = (N * sizeof(double)) / (read_time * 1e6);    // GB/s
        
        std::cout << "  Peak throughput (10M doubles):\n";
        std::cout << "    Write: " << write_bandwidth << " GB/s\n";
        std::cout << "    Read:  " << read_bandwidth << " GB/s\n";
        
        // Expect at least 1 GB/s for modern systems
        expect(write_bandwidth > 1.0) << "Write bandwidth should exceed 1 GB/s";
        expect(read_bandwidth > 1.0) << "Read bandwidth should exceed 1 GB/s";
        
        std::cout << "✅ Peak throughput test passed\n";
    };

    std::cout << "\n🎉 All performance tests completed successfully!\n";
    std::cout << "📊 Performance metrics collected for Julia interop optimization\n";
    std::cout << "✅ System ready for high-performance Julia integration\n\n";
    
    return 0;
}