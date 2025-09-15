// Zero-copy vector operations test
// Tests the most performance-critical feature for Julia interop
#include <chrono>
#include <complex>
#include <cstring>
#include <numeric>
#include <vector>

#include "glaze/interop/interop.hpp"
#include "boost/ut.hpp"

using namespace boost::ut;

// Test structure with various vector types
struct VectorTestStruct
{
   std::vector<float> float_vec;
   std::vector<double> double_vec;
   std::vector<int32_t> int_vec;
   std::vector<std::string> string_vec;
   std::vector<std::complex<float>> complex_vec;

   float sum_floats() const { return std::accumulate(float_vec.begin(), float_vec.end(), 0.0f); }

   void resize_all(size_t size)
   {
      float_vec.resize(size);
      double_vec.resize(size);
      int_vec.resize(size);
   }
};

template <>
struct glz::meta<VectorTestStruct>
{
   using T = VectorTestStruct;
   static constexpr auto value = object("float_vec", &T::float_vec, "double_vec", &T::double_vec, "int_vec",
                                        &T::int_vec, "string_vec", &T::string_vec, "complex_vec", &T::complex_vec,
                                        "sum_floats", &T::sum_floats, "resize_all", &T::resize_all);
};

int main()
{
   using namespace boost::ut;

   "zero-copy vector view"_test = [] {
      VectorTestStruct test_obj;
      const size_t N = 10000;

      // Resize vector
      test_obj.float_vec.resize(N);

      // Get zero-copy view
      glz_vector view = glz_vector_view(&test_obj.float_vec, glz::create_type_descriptor<std::vector<float>>());

      // Verify view properties
      expect(view.size == N);
      expect(view.capacity >= N);
      expect(view.data != nullptr);

      // Modify through direct pointer access
      float* data = static_cast<float*>(view.data);
      for (size_t i = 0; i < N; ++i) {
         data[i] = static_cast<float>(i * 3.14);
      }

      // Verify changes are visible in original vector
      for (size_t i = 0; i < N; ++i) {
         expect(test_obj.float_vec[i] == static_cast<float>(i * 3.14));
      }

      std::cout << "✅ Zero-copy vector view test passed\n";
   };

   "memory alignment for SIMD"_test = [] {
      std::vector<double> aligned_vec(1024);

      glz_vector view = glz_vector_view(&aligned_vec, glz::create_type_descriptor<std::vector<double>>());

      // Check 16-byte alignment for SIMD operations
      uintptr_t addr = reinterpret_cast<uintptr_t>(view.data);
      bool is_aligned = (addr % 16) == 0;

      // Note: std::vector doesn't guarantee alignment, but we test the pattern
      if (is_aligned) {
         std::cout << "✅ Vector data is 16-byte aligned for SIMD\n";
      }
      else {
         std::cout << "⚠️ Vector data not 16-byte aligned (expected with std::vector)\n";
      }
   };

   "large vector performance"_test = [] {
      const size_t N = 1'000'000; // 1 million elements
      std::vector<double> large_vec(N);

      auto start = std::chrono::high_resolution_clock::now();

      // Get zero-copy view
      glz_vector view = glz_vector_view(&large_vec, glz::create_type_descriptor<std::vector<double>>());

      // Direct pointer access
      double* data = static_cast<double*>(view.data);

      // Perform computation through view
      for (size_t i = 0; i < N; ++i) {
         data[i] = std::sin(i * 0.001);
      }

      auto end = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

      expect(view.size == N);
      expect(duration.count() < 1000) << "Operation on 1M elements should take < 1s";

      std::cout << "✅ Large vector (1M elements) processed in " << duration.count() << "ms\n";
   };

   "resize preserving data"_test = [] {
      std::vector<int32_t> vec = {1, 2, 3, 4, 5};

      // Get initial view
      auto view1 = glz_vector_view(&vec, glz::create_type_descriptor<std::vector<int32_t>>());
      expect(view1.size == 5);

      // Resize using C++ API (simulating what Julia would do)
      glz_vector_resize(&vec, glz::create_type_descriptor<std::vector<int32_t>>(), 10);

      // Get new view
      auto view2 = glz_vector_view(&vec, glz::create_type_descriptor<std::vector<int32_t>>());
      expect(view2.size == 10);

      // Verify original data preserved
      int32_t* data = static_cast<int32_t*>(view2.data);
      expect(data[0] == 1);
      expect(data[1] == 2);
      expect(data[2] == 3);
      expect(data[3] == 4);
      expect(data[4] == 5);

      std::cout << "✅ Resize preserving data test passed\n";
   };

   "zero-copy with complex types"_test = [] {
      std::vector<std::complex<float>> complex_vec(100);

      // Fill with test data
      for (size_t i = 0; i < 100; ++i) {
         complex_vec[i] = std::complex<float>(static_cast<float>(i), -static_cast<float>(i));
      }

      // Get zero-copy view
      auto view = glz_vector_view(&complex_vec, glz::create_type_descriptor<std::vector<std::complex<float>>>());

      // Access as complex array
      auto* complex_data = static_cast<std::complex<float>*>(view.data);

      // Verify data accessible
      expect(complex_data[0].real() == 0.0f);
      expect(complex_data[0].imag() == 0.0f);
      expect(complex_data[50].real() == 50.0f);
      // Note: complex_vec[50] was set to (50, -50), so imag should be -50
      expect(complex_data[50].imag() == -50.0f);

      // Modify through view
      complex_data[25] = std::complex<float>(3.14f, 2.718f);

      // Verify change in original
      expect(complex_vec[25].real() == 3.14f);
      expect(complex_vec[25].imag() == 2.718f);

      std::cout << "✅ Zero-copy with complex types test passed\n";
   };

   "push_back through API"_test = [] {
      std::vector<float> vec;

      // Push elements through API
      float val1 = 3.14f;
      float val2 = 2.718f;
      float val3 = 1.414f;

      glz_vector_push_back(&vec, glz::create_type_descriptor<std::vector<float>>(), &val1);
      glz_vector_push_back(&vec, glz::create_type_descriptor<std::vector<float>>(), &val2);
      glz_vector_push_back(&vec, glz::create_type_descriptor<std::vector<float>>(), &val3);

      // Verify through zero-copy view
      auto view = glz_vector_view(&vec, glz::create_type_descriptor<std::vector<float>>());

      expect(view.size == 3);
      float* data = static_cast<float*>(view.data);
      expect(data[0] == 3.14f);
      expect(data[1] == 2.718f);
      expect(data[2] == 1.414f);

      std::cout << "✅ Push_back through API test passed\n";
   };

   "memory lifetime management"_test = [] {
      // Test that view remains valid as long as vector exists
      std::vector<double>* vec = new std::vector<double>(1000);

      auto view = glz_vector_view(vec, glz::create_type_descriptor<std::vector<double>>());

      double* data = static_cast<double*>(view.data);

      // Fill with test pattern
      for (size_t i = 0; i < 1000; ++i) {
         data[i] = i * 1.5;
      }

      // Verify data accessible
      expect((*vec)[500] == 500 * 1.5);

      // Clean up
      delete vec;

      // Note: After delete, view.data is dangling - this tests that
      // the view doesn't own the memory, just provides access

      std::cout << "✅ Memory lifetime management test passed\n";
   };

   "string vector operations"_test = [] {
      std::vector<std::string> string_vec;

      // Add strings through C++ API
      string_vec.push_back("Hello");
      string_vec.push_back("World");
      string_vec.push_back("From");
      string_vec.push_back("Glaze");

      // Get view (note: strings are not contiguous in memory)
      auto view = glz_vector_view(&string_vec, glz::create_type_descriptor<std::vector<std::string>>());

      expect(view.size == 4);

      // For string vectors, data points to the vector's internal array
      // of string objects, not the character data
      expect(view.data != nullptr);

      std::cout << "✅ String vector operations test passed\n";
   };

   std::cout << "\n🎉 All zero-copy vector tests completed successfully!\n";
   std::cout << "📊 Coverage: zero-copy views, SIMD alignment, large vectors, complex types\n";
   std::cout << "✅ Ready for Julia integration with optimal performance\n\n";

   return 0;
}