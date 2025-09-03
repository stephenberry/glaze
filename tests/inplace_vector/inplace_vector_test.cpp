#include "glaze/containers/inplace_vector.hpp"

#include <compare>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

using glz::inplace_vector;

template <typename T, size_t N>
using freestanding_iv = glz::freestanding::inplace_vector<T, N>;

// used for initializing freestading inplace_vector without il constructor
template <typename T>
auto vec_of(std::initializer_list<typename T::value_type> il)
{
   T vec;
   for (const auto& item : il) {
      vec.try_emplace_back(item);
   }
   return vec;
}

struct my_struct_entry
{
   int a;
   int b;
   int c;
   auto operator<=>(const my_struct_entry&) const = default;
};

template <typename Container>
struct my_struct
{
   Container vec;
   auto operator<=>(const my_struct&) const
   {
      return std::lexicographical_compare_three_way(vec.begin(), vec.end(), vec.begin(), vec.end());
   }
};

template <typename IV>
constexpr auto test_int_vec = [] {
   const std::string_view json{R"([1,2,3,4,5])"};
   IV vec;
   std::string buffer{};

   expect(!glz::read<glz::opts{}>(vec, json));
   expect(vec.size() == 5);
   expect(vec == vec_of<IV>({1, 2, 3, 4, 5}));

   expect(!glz::write<glz::opts{}>(vec, buffer));
   expect(buffer == json);
};

template <typename IV>
constexpr auto test_int_vec_overflow = [] {
   IV vec;
   expect(glz::read<glz::opts{}>(vec, "[1,2,3,4,5,6,7,8,9,10]") == glz::error_code::none);
   expect(vec.size() == 10);

   expect(glz::read<glz::opts{}>(vec, "[1,2,3,4,5,6,7,8,9,10,11]") == glz::error_code::exceeded_static_array_size);
   expect(vec.size() == 10);

   expect(glz::read<glz::opts{}>(vec, "[1]") == glz::error_code::none);
   expect(vec.size() == 1);

   expect(glz::read<glz::opts{}>(vec, "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]") ==
          glz::error_code::exceeded_static_array_size);
   expect(vec.size() == 10);

   expect(glz::read<glz::opts{}>(vec, "[]") == glz::error_code::none);
   expect(vec.size() == 0) << "result: " << vec.size() << " expected: 0";
};

template <typename IV>
constexpr auto test_struct_vec = [] {
   const std::string_view json{R"({"vec":[{"a":1,"b":2,"c":3},{"a":4,"b":5,"c":6},{"a":7,"b":8,"c":9}]})"};
   std::string buffer{};
   my_struct<IV> s;

   expect(!glz::read<glz::opts{}>(s, json));
   expect(s.vec.size() == 3);
   expect(s.vec == vec_of<IV>({{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}));

   expect(!glz::write<glz::opts{}>(s, buffer));
   expect(buffer == json);
};

template <typename IV>
constexpr auto test_pair_vec = [] {
   IV vec;
   std::string buffer{};

   expect(!glz::read_json(vec, R"({"1":2,"3":4})"));
   expect(vec.size() == 2);
   expect(!glz::write_json(vec, buffer));
   expect(buffer == R"({"1":2,"3":4})");

   expect(glz::read_json(vec, R"({"1":2,"3":4,"5":6})") == glz::error_code::exceeded_static_array_size);
   expect(!glz::read_json(vec, R"({})"));
   expect(vec.size() == 0);

   expect(!glz::write_json(vec, buffer));
   expect(buffer == R"({})") << buffer;
};

suite json_test = [] {
   "int_vec"_test = [] {
      test_int_vec<inplace_vector<int, 10>>();
      test_int_vec<freestanding_iv<int, 10>>();
   };

   "int_vec_overflow"_test = [] {
      test_int_vec_overflow<inplace_vector<int, 10>>();
      test_int_vec_overflow<freestanding_iv<int, 10>>();
   };

   "struct_vec"_test = [] {
      test_struct_vec<inplace_vector<my_struct_entry, 3>>();
      test_struct_vec<freestanding_iv<my_struct_entry, 3>>();
   };

   "pair_vec"_test = [] {
      test_pair_vec<inplace_vector<std::pair<int, int>, 2>>();
      test_pair_vec<freestanding_iv<std::pair<int, int>, 2>>();
   };
};

suite construction_tests = [] {
   "default_constructor"_test = [] {
      inplace_vector<int, 10> v;
      expect(v.empty()) << "Default constructed vector should be empty\n";
      expect(v.size() == 0) << "Default constructed vector should have 0 size\n";
      expect(v.capacity() == 10) << "Capacity should match template parameter\n";
   };

   "size_constructor"_test = [] {
      inplace_vector<int, 10> v(5);
      expect(v.size() == 5) << "Size constructor should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == 0) << "Elements should be value-initialized\n";
      }
   };

   "size_value_constructor"_test = [] {
      inplace_vector<int, 10> v(5, 42);
      expect(v.size() == 5) << "Size-value constructor should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == 42) << "All elements should be initialized to the given value\n";
      }
   };

   "range_constructor"_test = [] {
      int arr[] = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v(std::begin(arr), std::end(arr));

      expect(v.size() == 5) << "Range constructor should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == arr[i]) << "Elements should match source range\n";
      }
   };

#ifdef __cpp_lib_containers_ranges
   "from_range_constructor"_test = [] {
      std::array<int, 5> arr = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v(std::from_range, arr);

      expect(v.size() == 5) << "From-range constructor should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == arr[i]) << "Elements should match source range\n";
      }
   };
#endif

   "copy_constructor"_test = [] {
      inplace_vector<int, 10> v1(5, 42);
      inplace_vector<int, 10> v2(v1);

      expect(v2.size() == v1.size()) << "Copy constructor should preserve size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v2[i] == v1[i]) << "Elements should be copied\n";
      }
   };

   "move_constructor"_test = [] {
      inplace_vector<int, 10> v1(5, 42);
      inplace_vector<int, 10> v2(std::move(v1));

      expect(v2.size() == 5) << "Move constructor should preserve size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v2[i] == 42) << "Elements should be moved\n";
      }

      // For trivially-copyable types, the moved-from vector still has its elements
      // according to the proposal document
      expect(v1.size() == 5) << "Moved-from vector should retain size for trivial types\n";
   };

   "initializer_list_constructor"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      expect(v.size() == 5) << "Initializer list constructor should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == i + 1) << "Elements should match initializer list\n";
      }
   };

   "copy_assignment"_test = [] {
      inplace_vector<int, 10> v1 = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v2;

      v2 = v1;

      expect(v2.size() == v1.size()) << "Copy assignment should preserve size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v2[i] == v1[i]) << "Elements should be copied\n";
      }
   };

   "move_assignment"_test = [] {
      inplace_vector<int, 10> v1 = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v2;

      v2 = std::move(v1);

      expect(v2.size() == 5) << "Move assignment should preserve size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v2[i] == i + 1) << "Elements should be moved\n";
      }

      // For trivially-copyable types, the moved-from vector still has its elements
      expect(v1.size() == 5) << "Moved-from vector should retain size for trivial types\n";
   };

   "initializer_list_assignment"_test = [] {
      inplace_vector<int, 10> v;

      v = {1, 2, 3, 4, 5};

      expect(v.size() == 5) << "Initializer list assignment should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == i + 1) << "Elements should match initializer list\n";
      }
   };

   "assign_range"_test = [] {
      int arr[] = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v;

      v.assign(std::begin(arr), std::end(arr));

      expect(v.size() == 5) << "Range assign should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == arr[i]) << "Elements should match source range\n";
      }
   };

   "assign_range_from_container"_test = [] {
      std::array<int, 5> arr = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v;

      v.assign_range(arr);

      expect(v.size() == 5) << "Range assign should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == arr[i]) << "Elements should match source range\n";
      }
   };

   "assign_size_value"_test = [] {
      inplace_vector<int, 10> v;

      v.assign(5, 42);

      expect(v.size() == 5) << "Size-value assign should set proper size\n";

      for (int i = 0; i < 5; ++i) {
         expect(v[i] == 42) << "All elements should be assigned to the given value\n";
      }
   };
};

suite capacity_tests = [] {
   "empty"_test = [] {
      inplace_vector<int, 10> v;
      expect(v.empty()) << "Default constructed vector should be empty\n";

      v.push_back(42);
      expect(not v.empty()) << "Vector with elements should not be empty\n";

      v.clear();
      expect(v.empty()) << "Cleared vector should be empty\n";
   };

   "size"_test = [] {
      inplace_vector<int, 10> v;
      expect(v.size() == 0) << "Default constructed vector should have size 0\n";

      v.push_back(1);
      expect(v.size() == 1) << "Size should increment after push_back\n";

      v.push_back(2);
      expect(v.size() == 2) << "Size should increment after push_back\n";

      v.pop_back();
      expect(v.size() == 1) << "Size should decrement after pop_back\n";

      v.clear();
      expect(v.size() == 0) << "Size should be 0 after clear\n";
   };

   "max_size_and_capacity"_test = [] {
      inplace_vector<int, 10> v;
      expect(v.max_size() == 10) << "max_size should match template parameter\n";
      expect(v.capacity() == 10) << "capacity should match template parameter\n";

      inplace_vector<int, 20> v2;
      expect(v2.max_size() == 20) << "max_size should match template parameter\n";
      expect(v2.capacity() == 20) << "capacity should match template parameter\n";
   };

   "resize_smaller"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      v.resize(3);

      expect(v.size() == 3) << "resize should reduce size\n";
      expect(v[0] == 1) << "resize should preserve remaining elements\n";
      expect(v[1] == 2) << "resize should preserve remaining elements\n";
      expect(v[2] == 3) << "resize should preserve remaining elements\n";
   };

   "resize_larger"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3};

      v.resize(5);

      expect(v.size() == 5) << "resize should increase size\n";
      expect(v[0] == 1) << "resize should preserve existing elements\n";
      expect(v[1] == 2) << "resize should preserve existing elements\n";
      expect(v[2] == 3) << "resize should preserve existing elements\n";
      expect(v[3] == 0) << "new elements should be value-initialized\n";
      expect(v[4] == 0) << "new elements should be value-initialized\n";
   };

   "resize_value"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3};

      v.resize(5, 42);

      expect(v.size() == 5) << "resize should increase size\n";
      expect(v[0] == 1) << "resize should preserve existing elements\n";
      expect(v[1] == 2) << "resize should preserve existing elements\n";
      expect(v[2] == 3) << "resize should preserve existing elements\n";
      expect(v[3] == 42) << "new elements should be initialized to the given value\n";
      expect(v[4] == 42) << "new elements should be initialized to the given value\n";
   };

   "reserve_and_shrink_to_fit"_test = [] {
      inplace_vector<int, 10> v;

      // These are no-ops for inplace_vector but should not throw if n <= capacity
      v.reserve(5);
      expect(v.capacity() == 10) << "reserve should not change capacity\n";

      v.shrink_to_fit();
      expect(v.capacity() == 10) << "shrink_to_fit should not change capacity\n";

#if __cpp_exceptions >= 199711L
      // Should throw if n > capacity
      expect(throws([&v] { v.reserve(20); })) << "reserve should throw if n > capacity\n";
#endif
   };

   "capacity_exceeded"_test = [] {
      inplace_vector<int, 3> v = {1, 2, 3};

#if __cpp_exceptions >= 199711L
      expect(throws([&v] { v.push_back(4); })) << "push_back should throw if capacity would be exceeded\n";

      expect(throws([&v] { v.resize(4); })) << "resize should throw if capacity would be exceeded\n";

      expect(throws([&v] { v.insert(v.begin(), 0); })) << "insert should throw if capacity would be exceeded\n";
#endif

      expect(v.try_emplace_back(4) == nullptr);
      expect(v.size() == 3) << "try_emplace_back should not modify size if capacity exceeded\n";

      expect(v.try_push_back(4) == nullptr);
      expect(v.size() == 3) << "try_push_back should not modify size if capacity exceeded\n";
   };
};

suite element_access_tests = [] {
   "subscript_operator"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      expect(v[0] == 1) << "operator[] should provide access to elements\n";
      expect(v[4] == 5) << "operator[] should provide access to elements\n";

      // Test modifying elements
      v[2] = 42;
      expect(v[2] == 42) << "operator[] should allow modifying elements\n";
   };

   "at"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      expect(v.at(0) == 1) << "at() should provide access to elements\n";
      expect(v.at(4) == 5) << "at() should provide access to elements\n";

      // Test modifying elements
      v.at(2) = 42;
      expect(v.at(2) == 42) << "at() should allow modifying elements\n";
#if __cpp_exceptions >= 199711L
      // Test out-of-bounds access
      expect(throws([&v] { v.at(10); })) << "at() should throw for out-of-bounds access\n";
#endif
   };

   "front_and_back"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      expect(v.front() == 1) << "front() should return the first element\n";
      expect(v.back() == 5) << "back() should return the last element\n";

      // Test modifying elements
      v.front() = 10;
      v.back() = 50;

      expect(v.front() == 10) << "front() should allow modifying the first element\n";
      expect(v.back() == 50) << "back() should allow modifying the last element\n";
   };

   "data"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      const int* data_ptr = v.data();
      expect(data_ptr != nullptr) << "data() should return a valid pointer\n";
      expect(*data_ptr == 1) << "data() pointer should point to the first element\n";
      expect(*(data_ptr + 2) == 3) << "data() pointer should allow pointer arithmetic\n";

      // Test modifying elements through data pointer
      int* mutable_data = v.data();
      *mutable_data = 10;
      expect(v[0] == 10) << "data() pointer should allow modifying elements\n";
   };
};

suite modifiers_tests = [] {
   "push_back"_test = [] {
      inplace_vector<int, 5> v;

      v.push_back(1);
      expect(v.size() == 1) << "push_back should increment size\n";
      expect(v[0] == 1) << "push_back should add element at the end\n";

      v.push_back(2);
      expect(v.size() == 2) << "push_back should increment size\n";
      expect(v[1] == 2) << "push_back should add element at the end\n";

      // Test push_back return value
      auto& ref = v.push_back(3);
      expect(ref == 3) << "push_back should return reference to the added element\n";
      expect(&ref == &v.back()) << "push_back should return reference to the actual element\n";
   };

   "emplace_back"_test = [] {
      inplace_vector<std::pair<int, int>, 5> v;

      v.emplace_back(1, 2);
      expect(v.size() == 1) << "emplace_back should increment size\n";
      expect(v[0].first == 1) << "emplace_back should construct element in-place\n";
      expect(v[0].second == 2) << "emplace_back should construct element in-place\n";

      // Test emplace_back return value
      auto& ref = v.emplace_back(3, 4);
      expect(ref.first == 3) << "emplace_back should return reference to the added element\n";
      expect(ref.second == 4) << "emplace_back should return reference to the added element\n";
      expect(&ref == &v.back()) << "emplace_back should return reference to the actual element\n";
   };

   "pop_back"_test = [] {
      inplace_vector<int, 5> v = {1, 2, 3, 4};

      v.pop_back();
      expect(v.size() == 3) << "pop_back should decrement size\n";
      expect(v.back() == 3) << "pop_back should remove the last element\n";

      v.pop_back();
      expect(v.size() == 2) << "pop_back should decrement size\n";
      expect(v.back() == 2) << "pop_back should remove the last element\n";
   };

   "clear"_test = [] {
      inplace_vector<int, 5> v = {1, 2, 3, 4};

      v.clear();
      expect(v.empty()) << "clear should make the vector empty\n";
      expect(v.size() == 0) << "clear should set size to 0\n";
   };

   "insert_single"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 4, 5};

      auto it = v.insert(v.begin() + 2, 3);

      expect(v.size() == 5) << "insert should increment size\n";
      expect(*it == 3) << "insert should return iterator to the inserted element\n";
      expect(it == v.begin() + 2) << "insert should return iterator at the correct position\n";
      expect(v[0] == 1) << "insert should preserve elements before insertion point\n";
      expect(v[1] == 2) << "insert should preserve elements before insertion point\n";
      expect(v[2] == 3) << "insert should insert the element at the specified position\n";
      expect(v[3] == 4) << "insert should move elements after insertion point\n";
      expect(v[4] == 5) << "insert should move elements after insertion point\n";
   };

   "insert_multiple"_test = [] {
      inplace_vector<int, 10> v = {1, 5};

      auto it = v.insert(v.begin() + 1, 3, 42);

      expect(v.size() == 5) << "insert should increment size by count\n";
      expect(*it == 42) << "insert should return iterator to the first inserted element\n";
      expect(it == v.begin() + 1) << "insert should return iterator at the correct position\n";
      expect(v[0] == 1) << "insert should preserve elements before insertion point\n";
      expect(v[1] == 42) << "insert should insert the elements at the specified position\n";
      expect(v[2] == 42) << "insert should insert the elements at the specified position\n";
      expect(v[3] == 42) << "insert should insert the elements at the specified position\n";
      expect(v[4] == 5) << "insert should move elements after insertion point\n";
   };

   "insert_range"_test = [] {
      inplace_vector<int, 10> v = {1, 5};
      int arr[] = {2, 3, 4};

      auto it = v.insert(v.begin() + 1, std::begin(arr), std::end(arr));

      expect(v.size() == 5) << "insert should increment size by range size\n";
      expect(*it == 2) << "insert should return iterator to the first inserted element\n";
      expect(it == v.begin() + 1) << "insert should return iterator at the correct position\n";
      expect(v[0] == 1) << "insert should preserve elements before insertion point\n";
      expect(v[1] == 2) << "insert should insert the elements from the range\n";
      expect(v[2] == 3) << "insert should insert the elements from the range\n";
      expect(v[3] == 4) << "insert should insert the elements from the range\n";
      expect(v[4] == 5) << "insert should move elements after insertion point\n";
   };

   "insert_range_containers"_test = [] {
      inplace_vector<int, 10> v = {1, 5};
      std::array<int, 3> arr = {2, 3, 4};

      auto it = v.insert_range(v.begin() + 1, arr);

      expect(v.size() == 5) << "insert_range should increment size by range size\n";
      expect(*it == 2) << "insert_range should return iterator to the first inserted element\n";
      expect(it == v.begin() + 1) << "insert_range should return iterator at the correct position\n";
      expect(v[0] == 1) << "insert_range should preserve elements before insertion point\n";
      expect(v[1] == 2) << "insert_range should insert the elements from the range\n";
      expect(v[2] == 3) << "insert_range should insert the elements from the range\n";
      expect(v[3] == 4) << "insert_range should insert the elements from the range\n";
      expect(v[4] == 5) << "insert_range should move elements after insertion point\n";
   };

   "insert_initializer_list"_test = [] {
      inplace_vector<int, 10> v = {1, 5};

      auto it = v.insert(v.begin() + 1, {2, 3, 4});

      expect(v.size() == 5) << "insert should increment size by list size\n";
      expect(*it == 2) << "insert should return iterator to the first inserted element\n";
      expect(it == v.begin() + 1) << "insert should return iterator at the correct position\n";
      expect(v[0] == 1) << "insert should preserve elements before insertion point\n";
      expect(v[1] == 2) << "insert should insert the elements from the initializer list\n";
      expect(v[2] == 3) << "insert should insert the elements from the initializer list\n";
      expect(v[3] == 4) << "insert should insert the elements from the initializer list\n";
      expect(v[4] == 5) << "insert should move elements after insertion point\n";
   };

   "erase_single"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      auto it = v.erase(v.begin() + 2);

      expect(v.size() == 4) << "erase should decrement size\n";
      expect(*it == 4) << "erase should return iterator to the element after the erased one\n";
      expect(it == v.begin() + 2) << "erase should return iterator at the correct position\n";
      expect(v[0] == 1) << "erase should preserve elements before erasure point\n";
      expect(v[1] == 2) << "erase should preserve elements before erasure point\n";
      expect(v[2] == 4) << "erase should move elements after erasure point\n";
      expect(v[3] == 5) << "erase should move elements after erasure point\n";
   };

   "erase_range"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      auto it = v.erase(v.begin() + 1, v.begin() + 4);

      expect(v.size() == 2) << "erase should decrement size by range size\n";
      expect(*it == 5) << "erase should return iterator to the element after the erased range\n";
      expect(it == v.begin() + 1) << "erase should return iterator at the correct position\n";
      expect(v[0] == 1) << "erase should preserve elements before erasure range\n";
      expect(v[1] == 5) << "erase should move elements after erasure range\n";
   };

   "swap"_test = [] {
      inplace_vector<int, 10> v1 = {1, 2, 3};
      inplace_vector<int, 10> v2 = {4, 5, 6, 7};

      v1.swap(v2);

      expect(v1.size() == 4) << "swap should exchange sizes\n";
      expect(v2.size() == 3) << "swap should exchange sizes\n";

      expect(v1[0] == 4) << "swap should exchange elements\n";
      expect(v1[1] == 5) << "swap should exchange elements\n";
      expect(v1[2] == 6) << "swap should exchange elements\n";
      expect(v1[3] == 7) << "swap should exchange elements\n";

      expect(v2[0] == 1) << "swap should exchange elements\n";
      expect(v2[1] == 2) << "swap should exchange elements\n";
      expect(v2[2] == 3) << "swap should exchange elements\n";

      // Test non-member swap
      swap(v1, v2);

      expect(v1.size() == 3) << "non-member swap should exchange sizes\n";
      expect(v2.size() == 4) << "non-member swap should exchange sizes\n";

      expect(v1[0] == 1) << "non-member swap should exchange elements\n";
      expect(v1[1] == 2) << "non-member swap should exchange elements\n";
      expect(v1[2] == 3) << "non-member swap should exchange elements\n";

      expect(v2[0] == 4) << "non-member swap should exchange elements\n";
      expect(v2[1] == 5) << "non-member swap should exchange elements\n";
      expect(v2[2] == 6) << "non-member swap should exchange elements\n";
      expect(v2[3] == 7) << "non-member swap should exchange elements\n";
   };

   "append_range"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3};
      std::array<int, 3> arr = {4, 5, 6};

      v.append_range(arr);

      expect(v.size() == 6) << "append_range should increment size by range size\n";
      expect(v[0] == 1) << "append_range should preserve existing elements\n";
      expect(v[1] == 2) << "append_range should preserve existing elements\n";
      expect(v[2] == 3) << "append_range should preserve existing elements\n";
      expect(v[3] == 4) << "append_range should append elements from the range\n";
      expect(v[4] == 5) << "append_range should append elements from the range\n";
      expect(v[5] == 6) << "append_range should append elements from the range\n";
   };

   "non_member_erase"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 2, 4, 2, 5};

      auto count = glz::erase(v, 2);

      expect(count == 3) << "erase should return the number of removed elements\n";
      expect(v.size() == 4) << "erase should remove all matching elements\n";
      expect(v[0] == 1) << "erase should preserve non-matching elements\n";
      expect(v[1] == 3) << "erase should preserve non-matching elements\n";
      expect(v[2] == 4) << "erase should preserve non-matching elements\n";
      expect(v[3] == 5) << "erase should preserve non-matching elements\n";
   };

   "non_member_erase_if"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5, 6, 7, 8, 9};

      auto count = glz::erase_if(v, [](int x) { return x % 2 == 0; });

      expect(count == 4) << "erase_if should return the number of removed elements\n";
      expect(v.size() == 5) << "erase_if should remove all matching elements\n";
      expect(v[0] == 1) << "erase_if should preserve non-matching elements\n";
      expect(v[1] == 3) << "erase_if should preserve non-matching elements\n";
      expect(v[2] == 5) << "erase_if should preserve non-matching elements\n";
      expect(v[3] == 7) << "erase_if should preserve non-matching elements\n";
      expect(v[4] == 9) << "erase_if should preserve non-matching elements\n";
   };
};

suite fallible_apis_tests = [] {
   "try_push_back"_test = [] {
      inplace_vector<int, 3> v;

      auto ptr1 = v.try_push_back(1);
      expect(ptr1 != nullptr) << "try_push_back should return non-null if successful\n";
      expect(*ptr1 == 1) << "try_push_back should return pointer to the inserted element\n";
      expect(v.size() == 1) << "try_push_back should increment size if successful\n";

      auto ptr2 = v.try_push_back(2);
      expect(ptr2 != nullptr) << "try_push_back should return non-null if successful\n";
      expect(*ptr2 == 2) << "try_push_back should return pointer to the inserted element\n";
      expect(v.size() == 2) << "try_push_back should increment size if successful\n";

      auto ptr3 = v.try_push_back(3);
      expect(ptr3 != nullptr) << "try_push_back should return non-null if successful\n";
      expect(*ptr3 == 3) << "try_push_back should return pointer to the inserted element\n";
      expect(v.size() == 3) << "try_push_back should increment size if successful\n";

      auto ptr4 = v.try_push_back(4);
      expect(ptr4 == nullptr) << "try_push_back should return null if capacity would be exceeded\n";
      expect(v.size() == 3) << "try_push_back should not change size if unsuccessful\n";
   };

   "try_emplace_back"_test = [] {
      inplace_vector<std::pair<int, int>, 3> v;

      auto ptr1 = v.try_emplace_back(1, 10);
      expect(ptr1 != nullptr) << "try_emplace_back should return non-null if successful\n";
      expect(ptr1->first == 1) << "try_emplace_back should return pointer to the inserted element\n";
      expect(ptr1->second == 10) << "try_emplace_back should return pointer to the inserted element\n";
      expect(v.size() == 1) << "try_emplace_back should increment size if successful\n";

      auto ptr2 = v.try_emplace_back(2, 20);
      expect(ptr2 != nullptr) << "try_emplace_back should return non-null if successful\n";
      expect(ptr2->first == 2) << "try_emplace_back should return pointer to the inserted element\n";
      expect(ptr2->second == 20) << "try_emplace_back should return pointer to the inserted element\n";
      expect(v.size() == 2) << "try_emplace_back should increment size if successful\n";

      auto ptr3 = v.try_emplace_back(3, 30);
      expect(ptr3 != nullptr) << "try_emplace_back should return non-null if successful\n";
      expect(ptr3->first == 3) << "try_emplace_back should return pointer to the inserted element\n";
      expect(ptr3->second == 30) << "try_emplace_back should return pointer to the inserted element\n";
      expect(v.size() == 3) << "try_emplace_back should increment size if successful\n";

      auto ptr4 = v.try_emplace_back(4, 40);
      expect(ptr4 == nullptr) << "try_emplace_back should return null if capacity would be exceeded\n";
      expect(v.size() == 3) << "try_emplace_back should not change size if unsuccessful\n";
   };

   "try_append_range"_test = [] {
      inplace_vector<int, 5> v = {1, 2};
      std::array<int, 4> arr = {3, 4, 5, 6};

      auto it = v.try_append_range(arr);

      expect(v.size() == 5) << "try_append_range should insert elements up to capacity\n";
      expect(v[0] == 1) << "try_append_range should preserve existing elements\n";
      expect(v[1] == 2) << "try_append_range should preserve existing elements\n";
      expect(v[2] == 3) << "try_append_range should insert elements from range\n";
      expect(v[3] == 4) << "try_append_range should insert elements from range\n";
      expect(v[4] == 5) << "try_append_range should insert elements from range\n";

      expect(it != arr.end()) << "try_append_range should return iterator to first non-inserted element\n";
      expect(*it == 6) << "try_append_range should return iterator to first non-inserted element\n";
   };
};

suite unchecked_apis_tests = [] {
   "unchecked_push_back"_test = [] {
      inplace_vector<int, 3> v;

      auto& ref1 = v.unchecked_push_back(1);
      expect(ref1 == 1) << "unchecked_push_back should return reference to the inserted element\n";
      expect(v.size() == 1) << "unchecked_push_back should increment size\n";

      auto& ref2 = v.unchecked_push_back(2);
      expect(ref2 == 2) << "unchecked_push_back should return reference to the inserted element\n";
      expect(v.size() == 2) << "unchecked_push_back should increment size\n";

      auto& ref3 = v.unchecked_push_back(3);
      expect(ref3 == 3) << "unchecked_push_back should return reference to the inserted element\n";
      expect(v.size() == 3) << "unchecked_push_back should increment size\n";

      // We can't test the precondition violation directly in a unit test
      // as it would invoke undefined behavior
   };

   "unchecked_emplace_back"_test = [] {
      inplace_vector<std::pair<int, int>, 3> v;

      auto& ref1 = v.unchecked_emplace_back(1, 10);
      expect(ref1.first == 1) << "unchecked_emplace_back should return reference to the inserted element\n";
      expect(ref1.second == 10) << "unchecked_emplace_back should return reference to the inserted element\n";
      expect(v.size() == 1) << "unchecked_emplace_back should increment size\n";

      auto& ref2 = v.unchecked_emplace_back(2, 20);
      expect(ref2.first == 2) << "unchecked_emplace_back should return reference to the inserted element\n";
      expect(ref2.second == 20) << "unchecked_emplace_back should return reference to the inserted element\n";
      expect(v.size() == 2) << "unchecked_emplace_back should increment size\n";

      auto& ref3 = v.unchecked_emplace_back(3, 30);
      expect(ref3.first == 3) << "unchecked_emplace_back should return reference to the inserted element\n";
      expect(ref3.second == 30) << "unchecked_emplace_back should return reference to the inserted element\n";
      expect(v.size() == 3) << "unchecked_emplace_back should increment size\n";

      // We can't test the precondition violation directly in a unit test
      // as it would invoke undefined behavior
   };
};

suite iterator_tests = [] {
   "iterators"_test = [] {
      inplace_vector<int, 10> v = {1, 2, 3, 4, 5};

      // Test begin/end
      expect(*v.begin() == 1) << "begin() should point to the first element\n";
      expect(*(v.end() - 1) == 5) << "end() should be one past the last element\n";

      // Test const iterators
      const auto& cv = v;
      expect(*cv.begin() == 1) << "const begin() should point to the first element\n";
      expect(*(cv.end() - 1) == 5) << "const end() should be one past the last element\n";

      // Test cbegin/cend
      expect(*v.cbegin() == 1) << "cbegin() should point to the first element\n";
      expect(*(v.cend() - 1) == 5) << "cend() should be one past the last element\n";

      // Test reverse iterators
      expect(*v.rbegin() == 5) << "rbegin() should point to the last element\n";
      expect(*(v.rend() - 1) == 1) << "rend() should be one past the first element (reversed)\n";

      // Test const reverse iterators
      expect(*cv.rbegin() == 5) << "const rbegin() should point to the last element\n";
      expect(*(cv.rend() - 1) == 1) << "const rend() should be one past the first element (reversed)\n";

      // Test crbegin/crend
      expect(*v.crbegin() == 5) << "crbegin() should point to the last element\n";
      expect(*(v.crend() - 1) == 1) << "crend() should be one past the first element (reversed)\n";

      // Test iteration
      int sum = 0;
      for (auto it = v.begin(); it != v.end(); ++it) {
         sum += *it;
      }
      expect(sum == 15) << "Iterator traversal should visit all elements\n";

      // Test range-based for loop
      sum = 0;
      for (const auto& x : v) {
         sum += x;
      }
      expect(sum == 15) << "Range-based for loop should visit all elements\n";

      // Test iterator modification
      for (auto it = v.begin(); it != v.end(); ++it) {
         *it *= 2;
      }

      expect(v[0] == 2) << "Iterators should allow modifying elements\n";
      expect(v[1] == 4) << "Iterators should allow modifying elements\n";
      expect(v[2] == 6) << "Iterators should allow modifying elements\n";
      expect(v[3] == 8) << "Iterators should allow modifying elements\n";
      expect(v[4] == 10) << "Iterators should allow modifying elements\n";
   };

   "algorithm_compatibility"_test = [] {
      inplace_vector<int, 10> v = {5, 2, 8, 1, 9};

      // Test std::sort
      std::sort(v.begin(), v.end());
      expect(v[0] == 1) << "std::sort should work with inplace_vector iterators\n";
      expect(v[1] == 2) << "std::sort should work with inplace_vector iterators\n";
      expect(v[2] == 5) << "std::sort should work with inplace_vector iterators\n";
      expect(v[3] == 8) << "std::sort should work with inplace_vector iterators\n";
      expect(v[4] == 9) << "std::sort should work with inplace_vector iterators\n";

      // Test std::find
      auto it = std::find(v.begin(), v.end(), 5);
      expect(it != v.end()) << "std::find should work with inplace_vector iterators\n";
      expect(*it == 5) << "std::find should work with inplace_vector iterators\n";

      // Test std::accumulate
      int sum = std::accumulate(v.begin(), v.end(), 0);
      expect(sum == 25) << "std::accumulate should work with inplace_vector iterators\n";

      // Test std::fill
      std::fill(v.begin(), v.end(), 42);
      for (const auto& x : v) {
         expect(x == 42) << "std::fill should work with inplace_vector iterators\n";
      }
   };
};

suite comparison_tests = [] {
   "equality_comparison"_test = [] {
      inplace_vector<int, 10> v1 = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v2 = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v3 = {5, 4, 3, 2, 1};
      inplace_vector<int, 10> v4 = {1, 2, 3, 4};

      expect(v1 == v2) << "Equal vectors should compare equal\n";
      expect(v1 != v3) << "Different vectors should compare not equal\n";
      expect(v1 != v4) << "Vectors with different sizes should compare not equal\n";
   };

   "three_way_comparison"_test = [] {
      inplace_vector<int, 10> v1 = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v2 = {1, 2, 3, 4, 5};
      inplace_vector<int, 10> v3 = {1, 2, 3, 5, 4};
      inplace_vector<int, 10> v4 = {1, 2, 3, 4};
      inplace_vector<int, 10> v5 = {1, 2, 3, 4, 6};

      expect(v1 == v2) << "Equal vectors should compare equal\n";
      expect(v1 < v3) << "Lexicographically smaller vector should compare less\n";
      expect(v3 > v1) << "Lexicographically greater vector should compare greater\n";
      expect(v1 > v4) << "Longer vector should compare greater\n";
      expect(v4 < v1) << "Shorter vector should compare less\n";
      expect(v1 < v5) << "Vector with smaller elements should compare less\n";
      expect(v5 > v1) << "Vector with greater elements should compare greater\n";
   };
};

suite edge_cases_tests = [] {
   "zero_capacity"_test = [] {
      inplace_vector<int, 0> v;

      expect(v.empty()) << "Vector with zero capacity should be empty\n";
      expect(v.size() == 0) << "Vector with zero capacity should have zero size\n";
      expect(v.capacity() == 0) << "Vector with zero capacity should have zero capacity\n";

#if __cpp_exceptions >= 199711L
      expect(throws([&v] { v.push_back(1); })) << "push_back on vector with zero capacity should throw\n";
#endif

      auto ptr = v.try_push_back(1);
      expect(ptr == nullptr) << "try_push_back on vector with zero capacity should return nullptr\n";

      // Test that the is_empty type trait works
      expect(std::is_empty_v<inplace_vector<int, 0>>) << "inplace_vector<T, 0> should be an empty type\n";
   };

   "non_default_constructible_type"_test = [] {
      struct NonDefault
      {
         int x;
         explicit NonDefault(int val) : x(val) {}
      };

      inplace_vector<NonDefault, 5> v;
      expect(v.empty()) << "Vector of non-default constructible type should start empty\n";

      v.emplace_back(42);
      expect(v.size() == 1) << "emplace_back should work with non-default constructible types\n";
      expect(v[0].x == 42) << "emplace_back should construct element correctly\n";

      v.push_back(NonDefault(43));
      expect(v.size() == 2) << "push_back should work with non-default constructible types\n";
      expect(v[1].x == 43) << "push_back should construct element correctly\n";

      // This should work, but with value-initialization
      // expect(throws([&] {
      //     inplace_vector<NonDefault, 5> v2(3);
      // })) << "Size constructor should throw for non-default constructible types\n";
   };

   "non_copyable_type"_test = [] {
      struct NonCopyable
      {
         int x;
         explicit NonCopyable(int val) : x(val) {}
         NonCopyable(const NonCopyable&) = delete;
         NonCopyable& operator=(const NonCopyable&) = delete;
         NonCopyable(NonCopyable&&) = default;
         NonCopyable& operator=(NonCopyable&&) = default;
      };

      inplace_vector<NonCopyable, 5> v;

      v.emplace_back(42);
      expect(v.size() == 1) << "emplace_back should work with non-copyable types\n";
      expect(v[0].x == 42) << "emplace_back should construct element correctly\n";

      v.push_back(NonCopyable(43));
      expect(v.size() == 2) << "push_back should work with non-copyable types (via move)\n";
      expect(v[1].x == 43) << "push_back should construct element correctly\n";

      // These should fail to compile:
      // inplace_vector<NonCopyable, 5> v2(3, NonCopyable(0));
      // v.insert(v.begin(), NonCopyable(44));
      // v.insert(v.begin(), 2, NonCopyable(44));
   };

   "move_only_type"_test = [] {
      struct MoveOnly
      {
         int x;
         explicit MoveOnly(int val) : x(val) {}
         MoveOnly(const MoveOnly&) = delete;
         MoveOnly& operator=(const MoveOnly&) = delete;
         MoveOnly(MoveOnly&&) = default;
         MoveOnly& operator=(MoveOnly&&) = default;
      };

      inplace_vector<MoveOnly, 5> v;

      v.emplace_back(42);
      expect(v.size() == 1) << "emplace_back should work with move-only types\n";
      expect(v[0].x == 42) << "emplace_back should construct element correctly\n";

      v.push_back(MoveOnly(43));
      expect(v.size() == 2) << "push_back should work with move-only types\n";
      expect(v[1].x == 43) << "push_back should construct element correctly\n";

      auto it = v.insert(v.begin(), MoveOnly(41));
      expect(v.size() == 3) << "insert should work with move-only types\n";
      expect(v[0].x == 41) << "insert should construct element correctly\n";
      expect(it == v.begin()) << "insert should return correct iterator\n";

      inplace_vector<MoveOnly, 5> v2;
      v2.emplace_back(50);

      v2 = std::move(v);
      expect(v2.size() == 3) << "Move assignment should work with move-only types\n";
      expect(v2[0].x == 41) << "Move assignment should preserve elements\n";
      expect(v2[1].x == 42) << "Move assignment should preserve elements\n";
      expect(v2[2].x == 43) << "Move assignment should preserve elements\n";
   };

   "trivial_type_optimizations"_test = [] {
      // For trivially copyable types, special optimizations should apply

      // Check that trivially copyable types propagate
      using TrivialVector = inplace_vector<int, 10>;
      expect(std::is_trivially_copyable_v<TrivialVector>)
         << "inplace_vector of trivially copyable type should be trivially copyable\n";

      // Test move semantics for trivially copyable types
      TrivialVector v1 = {1, 2, 3, 4, 5};
      TrivialVector v2(std::move(v1));

      expect(v2.size() == 5) << "Move constructor should copy size for trivial types\n";
      expect(v1.size() == 5) << "Moved-from vector should retain size for trivial types\n";

      for (int i = 0; i < 5; ++i) {
         expect(v1[i] == i + 1) << "Moved-from vector should retain elements for trivial types\n";
         expect(v2[i] == i + 1) << "Move constructor should copy elements for trivial types\n";
      }

      // Test trivial swap optimization
      TrivialVector v3 = {10, 20, 30};
      TrivialVector v4 = {40, 50, 60, 70};

      v3.swap(v4);

      expect(v3.size() == 4) << "Swap should exchange sizes\n";
      expect(v4.size() == 3) << "Swap should exchange sizes\n";

      expect(v3[0] == 40) << "Swap should exchange elements\n";
      expect(v3[1] == 50) << "Swap should exchange elements\n";
      expect(v3[2] == 60) << "Swap should exchange elements\n";
      expect(v3[3] == 70) << "Swap should exchange elements\n";

      expect(v4[0] == 10) << "Swap should exchange elements\n";
      expect(v4[1] == 20) << "Swap should exchange elements\n";
      expect(v4[2] == 30) << "Swap should exchange elements\n";
   };
};

struct TrackingDestructor
{
   int value;
   static inline int last_destroyed_value = -1;

   TrackingDestructor(int v) : value(v) {}
   ~TrackingDestructor() { last_destroyed_value = value; }
};

suite pop_back_tests = [] {
   "pop_back_destroy_correct_element"_test = [] {
      inplace_vector<TrackingDestructor, 5> v;
      v.emplace_back(10);
      v.emplace_back(20);
      v.emplace_back(30);

      // Before pop_back: v = [10, 20, 30], size = 3
      // back() should point to element with value 30
      expect(v.back().value == 30) << "back() should point to last element";

      TrackingDestructor::last_destroyed_value = -1;
      v.pop_back();

      // After pop_back: size should be 2, and value 30 should have been destroyed
      expect(v.size() == 2) << "size should be decremented";
      expect(TrackingDestructor::last_destroyed_value == 30)
         << "The element with value 30 should have been destroyed, but got: "
         << TrackingDestructor::last_destroyed_value;

      // The remaining elements should be [10, 20]
      expect(v[0].value == 10) << "First element should still be 10";
      expect(v[1].value == 20) << "Second element should still be 20";
      expect(v.back().value == 20) << "back() should now point to element with value 20";
   };
};

suite zero_capacity_tests = [] {
   "zero_capacity_comparison"_test = [] {
      inplace_vector<int, 0> v1;
      inplace_vector<int, 0> v2;

      bool result = (v1 == v2);
      expect(result) << "Two empty zero-capacity vectors should be equal";
   };

   "zero_capacity_assignment"_test = [] {
      inplace_vector<int, 0> v1;
      inplace_vector<int, 0> v2;

      v1 = v2;
      expect(v1 == v2) << "Assignment should work for zero-capacity vectors";
   };
};

suite storage_access_tests = [] {
   "trivial_type_comparison_consistency"_test = [] {
      inplace_vector<int, 5> v1{1, 2, 3};
      inplace_vector<int, 5> v2{1, 2, 3};

      expect(v1 == v2) << "Identical vectors should be equal";
   };

   "assign_method_storage_access"_test = [] {
      std::vector<int> source{1, 2, 3, 4, 5};

      inplace_vector<int, 10> v1;
      inplace_vector<int, 10> v2;

      v1.assign(source.begin(), source.end());
      v2.assign_range(source);

      expect(v1 == v2) << "assign() and assign_range() should produce identical results";
      expect(v1.size() == 5) << "Size should match source";

      for (size_t i = 0; i < v1.size(); ++i) {
         expect(v1[i] == source[i]) << "Elements should match source at index " << i;
      }
   };
};

suite swap_bug_tests = [] {
   "swap_trivial_types"_test = [] {
      inplace_vector<int, 5> v1{1, 2, 3};
      inplace_vector<int, 5> v2{4, 5, 6, 7};

      std::vector<int> orig_v1(v1.begin(), v1.end());
      std::vector<int> orig_v2(v2.begin(), v2.end());

      v1.swap(v2);

      expect(v1.size() == orig_v2.size()) << "v1 should have v2's original size";
      expect(v2.size() == orig_v1.size()) << "v2 should have v1's original size";

      for (size_t i = 0; i < v1.size(); ++i) {
         expect(v1[i] == orig_v2[i]) << "v1 element " << i << " should match v2's original";
      }

      for (size_t i = 0; i < v2.size(); ++i) {
         expect(v2[i] == orig_v1[i]) << "v2 element " << i << " should match v1's original";
      }
   };

   "swap_with_different_sizes"_test = [] {
      inplace_vector<int, 10> v1{1, 2};
      inplace_vector<int, 10> v2{3, 4, 5, 6, 7};

      size_t orig_v1_size = v1.size();
      size_t orig_v2_size = v2.size();

      v1.swap(v2);

      expect(v1.size() == orig_v2_size) << "Sizes should be swapped";
      expect(v2.size() == orig_v1_size) << "Sizes should be swapped";
   };
};

#if __cpp_exceptions
suite bounds_checking_tests = [] {
   "at_method_exception_type"_test = [] {
      inplace_vector<int, 5> v{1, 2, 3};

      try {
         v.at(10);
         expect(false) << "at() should have thrown an exception";
      }
      catch (const std::out_of_range&) {
         expect(true) << "at() correctly threw std::out_of_range";
      }
      catch (...) {
         expect(false) << "at() threw unexpected exception type";
      }
   };
};
#endif

int main() { return 0; }
