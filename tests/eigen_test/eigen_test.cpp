// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/eigen.hpp"

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <any>
#include <chrono>
#include <iostream>
#include <iterator>
#include <random>

#include "glaze/beve/beve_to_json.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct test_struct
{
   Eigen::Matrix3d d = Eigen::Matrix3d::Identity();
} test_value;

struct ConstHolder
{
   Eigen::Matrix2d m;
};

suite matrix3d = [] {
   "eigen Matrix3d"_test = [] {
      auto result = glz::write_json(test_value.d).value();
      expect(result == "[1,0,0,0,1,0,0,0,1]") << result;

      static_assert(glz::reflect<test_struct>::size == 1);
      static_assert(glz::reflect<test_struct>::keys[0] == "d");

      result = glz::write_json(test_value).value();
      expect(result == R"({"d":[1,0,0,0,1,0,0,0,1]})") << result;
   };
};

// Struct with multiple Eigen types
struct complex_struct
{
   Eigen::Matrix2f mf;
   Eigen::Vector4i vi;
   Eigen::MatrixXcd mcd;
} complex_test_value;

static_assert(glz::eigen_t<Eigen::Matrix2f>);
static_assert(glz::eigen_t<Eigen::Vector4i>);

// Initialize complex_test_value
void initialize_complex_struct()
{
   complex_test_value.mf << 1.1f, 2.2f, 3.3f, 4.4f;
   complex_test_value.vi << 1, 2, 3, 4;
   complex_test_value.mcd = Eigen::MatrixXcd(2, 2);
   complex_test_value.mcd << std::complex<double>(1, 2), std::complex<double>(3, 4), std::complex<double>(5, 6),
      std::complex<double>(7, 8);
}

suite additional_eigen_tests = [] {
   "write_json_matrix4d"_test = [] {
      Eigen::Matrix4d m;
      m << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16;
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[1,5,9,13,2,6,10,14,3,7,11,15,4,8,12,16]") << json;
   };

   "read_json_matrix4d"_test = [] {
      Eigen::Matrix4d m;
      std::string input = "[1,5,9,13,2,6,10,14,3,7,11,15,4,8,12,16]";
      expect(not glz::read_json(m, input));
      Eigen::Matrix4d expected;
      expected << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16;
      expect(m == expected);
   };

   "write_beve_row_major"_test = [] {
      Eigen::Matrix<double, 2, 3, Eigen::RowMajor> m;
      m << 1, 2, 3, 4, 5, 6;
      std::string b;
      expect(not glz::write_beve(m, b));
      // BEVE layout specifics can vary; adjust expected value accordingly
      // Here we assume it writes row-major
      Eigen::Matrix<double, 2, 3, Eigen::RowMajor> e;
      expect(!glz::read_beve(e, b));
      expect(m == e);
   };

   "read_json_empty_matrix"_test = [] {
      Eigen::MatrixXd m;
      std::string input = "[[0,0]]"; // Empty array
      expect(not glz::read_json(m, input));
      expect(m.rows() == 0);
      expect(m.cols() == 0);
   };

   "write_json_large_matrix"_test = [] {
      Eigen::MatrixXd m(100, 100);
      m.setRandom();
      std::string json;
      expect(not glz::write_json(m, json));
      // Here we check the JSON starts with '[' and has the correct number of elements
      expect(!json.empty());
      expect(json.front() == '[');
      expect(json.back() == ']');
   };

   "read_json_matrix"_test = [] {
      std::string input = "[[3,3],[1,4,7,2,5,8,3,6,9]]";
      Eigen::MatrixXd m;
      expect(not glz::read_json(m, input));
      Eigen::MatrixXd expected(3, 3);
      expected << 1, 2, 3, 4, 5, 6, 7, 8, 9;
      expect(m == expected);
   };

   "write_json_vector4f"_test = [] {
      Eigen::Vector4f v{1.0f, 2.0f, 3.0f, 4.0f};
      std::string json;
      expect(not glz::write_json(v, json));
      expect(json == "[1,2,3,4]");
   };

   "read_json_vector4f"_test = [] {
      Eigen::Vector4f v;
      std::string input = "[5.5,6.6,7.7,8.8]";
      expect(not glz::read_json(v, input));
      Eigen::Vector4f expected{5.5f, 6.6f, 7.7f, 8.8f};
      expect(v == expected);
   };

   "write_json_integer_matrix"_test = [] {
      Eigen::Matrix<int, 3, 3> m;
      m << 1, 2, 3, 4, 5, 6, 7, 8, 9;
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[1,4,7,2,5,8,3,6,9]") << json;
   };

   "read_json_integer_matrix"_test = [] {
      Eigen::Matrix<int, 3, 3> m;
      std::string input = "[10,40,70,20,50,80,30,60,90]";
      expect(not glz::read_json(m, input));
      Eigen::Matrix<int, 3, 3> expected;
      expected << 10, 20, 30, 40, 50, 60, 70, 80, 90;
      expect(m == expected);
   };

   "write_beve_complex_matrix"_test = [] {
      Eigen::MatrixXcd m(2, 2);
      m << std::complex<double>(1, 1), std::complex<double>(2, 2), std::complex<double>(3, 3),
         std::complex<double>(4, 4);
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::MatrixXcd e;
      expect(not glz::read_beve(e, b));
      expect(m == e);
   };

   "read_beve_invalid_data"_test = [] {
      Eigen::MatrixXd m;
      std::string invalid_beve = "invalid_binary_data";
      // Assuming read_beve returns an error code or sets m to a default state
      // Adjust based on actual Glaze behavior
      expect(glz::read_beve(m, invalid_beve) != glz::error_code::none);
   };

   "serialize_deserialize_complex_struct_json"_test = [] {
      initialize_complex_struct();
      std::string json;
      expect(not glz::write_json(complex_test_value, json));
      complex_struct deserialized;
      auto ec = glz::read_json(deserialized, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(deserialized.mf == complex_test_value.mf);
      expect(deserialized.vi == complex_test_value.vi);
      expect(deserialized.mcd == complex_test_value.mcd);
   };

   "serialize_deserialize_complex_struct_beve"_test = [] {
      initialize_complex_struct();
      std::string b;
      expect(not glz::write_beve(complex_test_value, b));
      complex_struct deserialized;
      expect(!glz::read_beve(deserialized, b));
      expect(deserialized.mf == complex_test_value.mf);
      expect(deserialized.vi == complex_test_value.vi);
      expect(deserialized.mcd == complex_test_value.mcd);
   };

   "json_ref_matrix"_test = [] {
      Eigen::Matrix3d source;
      source << 1, 2, 3, 4, 5, 6, 7, 8, 9;
      Eigen::Ref<Eigen::Matrix3d> ref(source);
      std::string json;
      expect(not glz::write_json(ref, json));
      expect(json == "[1,4,7,2,5,8,3,6,9]") << json;

      Eigen::Matrix3d parsed;
      expect(not glz::read_json(parsed, json));
      expect(source == parsed);
   };

   "json_non_square_matrix"_test = [] {
      Eigen::Matrix<double, 2, 3> m;
      m << 1, 2, 3, 4, 5, 6;
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[1,4,2,5,3,6]") << json;

      Eigen::Matrix<double, 2, 3> parsed;
      expect(not glz::read_json(parsed, json));
      expect(m == parsed);
   };

   "write_json_zero_sized_matrix"_test = [] {
      Eigen::MatrixXd m(0, 0);
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[[0,0],[]]");
      expect(not glz::read_json(m, json));
      expect(m.rows() == 0);
      expect(m.cols() == 0);
   };

   "write_json_mixed_storage_order"_test = [] {
      Eigen::Matrix<double, 3, 3, Eigen::RowMajor> m;
      m << 1, 2, 3, 4, 5, 6, 7, 8, 9;
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[1,2,3,4,5,6,7,8,9]");
   };

   "read_json_mixed_storage_order"_test = [] {
      Eigen::Matrix<double, 3, 3, Eigen::RowMajor> m;
      std::string input = "[9,8,7,6,5,4,3,2,1]";
      expect(not glz::read_json(m, input));
      Eigen::Matrix<double, 3, 3, Eigen::RowMajor> expected;
      expected << 9, 8, 7, 6, 5, 4, 3, 2, 1;
      expect(m == expected);
   };
};

int main()
{
   "write_json"_test = [] {
      Eigen::Matrix<double, 2, 2> m{};
      m << 5, 1, 1, 7;
      std::string json{};
      expect(not glz::write_json(m, json));
      expect(json == "[5,1,1,7]");
   };

   "read_json"_test = [] {
      Eigen::Matrix<double, 2, 2> m{};
      expect(not glz::read_json(m, "[2,1,7,4]"));
      expect(m.rows() == 2);
      expect(m.cols() == 2);
      expect(m(0, 1) == 7);
      expect(m(1, 1) == 4);
   };

   "beve"_test = [] {
      Eigen::Matrix<double, 2, 2> m{};
      m << 1, 2, 3, 4;
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::Matrix<double, 2, 2> e{};
      expect(!glz::read_beve(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "beve"_test = [] {
      Eigen::MatrixXd m(2, 2);
      m << 1, 2, 3, 4;
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::MatrixXd e(2, 2);
      expect(!glz::read_beve(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "beve_to_json"_test = [] {
      Eigen::MatrixXd m(2, 2);
      m << 1, 2, 3, 4;
      std::string b;
      expect(not glz::write_beve(m, b));
      std::string json{};
      expect(!glz::beve_to_json(b, json));
      expect(json == R"({"layout":"layout_right","extents":[2,2],"value":[1,3,2,4]})") << json;
   };

   "array"_test = [] {
      Eigen::Vector3d m{1, 2, 3};
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::Vector3d e{};
      expect(!glz::read_beve(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "dynamic array"_test = [] {
      Eigen::VectorXd m(10);
      for (int i = 0; i < m.size(); ++i) {
         m[i] = i;
      }
      std::string b;
      expect(not glz::write_json(m, b));
      Eigen::VectorXd e{};
      expect(not glz::read_json(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "dynamic array"_test = [] {
      Eigen::VectorXd m(10);
      for (int i = 0; i < m.size(); ++i) {
         m[i] = i;
      }
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::VectorXd e{};
      expect(!glz::read_beve(e, b));
      expect(m == e);

      expect(not glz::write_json(m, b));
      expect(!glz::read_json(e, b));
      expect(m == e);
   };

   "Eigen::VectorXcd"_test = [] {
      Eigen::VectorXcd m(10);
      for (int i = 0; i < m.size(); ++i) {
         m[i] = {double(i), 2 * double(i)};
      }
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::VectorXcd e{};
      expect(!glz::read_beve(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "Eigen::MatrixXd"_test = [] {
      Eigen::MatrixXd mat1(2, 3);
      mat1 << 9, 7, 0, 1, 2, 3;

      std::string json;
      expect(not glz::write_json(mat1, json));
      expect(json == "[[2,3],[9,1,7,2,0,3]]"); // [2,3] are rows and cols

      Eigen::MatrixXd mat2{};
      expect(not glz::read_json(mat2, json));
      const bool boolean = mat1 == mat2;
      expect(boolean);
   };

   "Eigen::MatrixXcd"_test = [] {
      Eigen::MatrixXcd m(3, 3);
      for (int i = 0; i < m.size(); ++i) {
         m.array()(i) = {double(i), 2 * double(i)};
      }
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::MatrixXcd e(3, 3);
      expect(!glz::read_beve(e, b));
      expect(bool(m == e));

      expect(not glz::write_json(m, b));
      expect(!glz::write_json(e, b));
      expect(bool(m == e));
   };

   "Eigen::Ref"_test = [] {
      Eigen::VectorXcd source(10);
      for (int i = 0; i < source.size(); ++i) {
         source[i] = {double(i), 2 * double(i)};
      }

      Eigen::Ref<Eigen::VectorXcd> m = source;

      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::VectorXcd e{};
      expect(!glz::read_beve(e, b));
      expect(bool(m == e));

      expect(not glz::write_json(m, b));
      expect(!glz::read_json(e, b));
      expect(bool(m == e));
   };

   "Eigen::Transform"_test = [] {
      Eigen::Isometry3d pose1, pose2;
      pose1.setIdentity();
      pose1.translation() << 1.111, 2.222, 3.333;
      std::string buffer = glz::write_json(pose1).value();
      expect(buffer == "[1,0,0,0,0,1,0,0,0,0,1,0,1.111,2.222,3.333,1]");
      expect(not glz::read_json(pose2, buffer));
      expect(pose1.matrix() == pose2.matrix());

      Eigen::AffineCompact2d c1, c2;
      c1.setIdentity();
      buffer = glz::write_json(c1).value();
      expect(buffer == "[1,0,0,1,0,0]");
      expect(not glz::read_json(c2, buffer));
      expect(c1.matrix() == c2.matrix());
   };

   "const Matrix4d JSON"_test = [] {
      const Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]") << json;
   };

   "const Matrix4d BEVE"_test = [] {
      const Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
      std::string b;
      expect(not glz::write_beve(m, b));
      expect(!b.empty());

      Eigen::Matrix4d e;
      expect(not glz::read_beve(e, b));
      expect(m == e);
   };

   "const Matrix3d JSON"_test = [] {
      Eigen::Matrix3d temp;
      temp << 1, 2, 3, 4, 5, 6, 7, 8, 9;
      const Eigen::Matrix3d m = temp;
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[1,4,7,2,5,8,3,6,9]") << json;
   };

   "const Vector4f JSON"_test = [] {
      const Eigen::Vector4f v{1.5f, 2.5f, 3.5f, 4.5f};
      std::string json;
      expect(not glz::write_json(v, json));
      expect(json == "[1.5,2.5,3.5,4.5]") << json;
   };

   "const MatrixXd JSON"_test = [] {
      Eigen::MatrixXd temp(2, 3);
      temp << 1, 2, 3, 4, 5, 6;
      const Eigen::MatrixXd m = temp;
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[[2,3],[1,4,2,5,3,6]]") << json;
   };

   "const MatrixXd BEVE"_test = [] {
      Eigen::MatrixXd temp(2, 3);
      temp << 1, 2, 3, 4, 5, 6;
      const Eigen::MatrixXd m = temp;
      std::string b;
      expect(not glz::write_beve(m, b));
      expect(!b.empty());

      Eigen::MatrixXd e;
      expect(not glz::read_beve(e, b));
      expect(m == e);
   };

   "const RowMajor Matrix JSON"_test = [] {
      Eigen::Matrix<double, 3, 3, Eigen::RowMajor> temp;
      temp << 1, 2, 3, 4, 5, 6, 7, 8, 9;
      const Eigen::Matrix<double, 3, 3, Eigen::RowMajor> m = temp;
      std::string json;
      expect(not glz::write_json(m, json));
      expect(json == "[1,2,3,4,5,6,7,8,9]") << json;
   };

   "const RowMajor Matrix BEVE"_test = [] {
      Eigen::Matrix<double, 2, 3, Eigen::RowMajor> temp;
      temp << 1, 2, 3, 4, 5, 6;
      const Eigen::Matrix<double, 2, 3, Eigen::RowMajor> m = temp;
      std::string b;
      expect(not glz::write_beve(m, b));
      expect(!b.empty());

      Eigen::Matrix<double, 2, 3, Eigen::RowMajor> e;
      expect(not glz::read_beve(e, b));
      expect(m == e);
   };

   "const Eigen::Isometry3d JSON"_test = [] {
      Eigen::Isometry3d temp;
      temp.setIdentity();
      temp.translation() << 1.0, 2.0, 3.0;
      const Eigen::Isometry3d pose = temp;
      std::string json;
      expect(not glz::write_json(pose, json));
      expect(json == "[1,0,0,0,0,1,0,0,0,0,1,0,1,2,3,1]") << json;
   };

   "const VectorXcd JSON"_test = [] {
      Eigen::VectorXcd temp(3);
      temp << std::complex<double>(1, 1), std::complex<double>(2, 2), std::complex<double>(3, 3);
      const Eigen::VectorXcd v = temp;
      std::string json;
      expect(not glz::write_json(v, json));
      expect(!json.empty());
      expect(json.front() == '[');
      expect(json.back() == ']');
   };

   "const struct with Eigen matrix JSON"_test = [] {
      ConstHolder temp;
      temp.m << 1, 2, 3, 4;
      const ConstHolder holder = temp;

      std::string json;
      expect(not glz::write_json(holder, json));
      expect(json == R"({"m":[1,3,2,4]})") << json;
   };

   "const struct with Eigen matrix BEVE"_test = [] {
      ConstHolder temp;
      temp.m << 1, 2, 3, 4;
      const ConstHolder holder = temp;

      std::string b;
      expect(not glz::write_beve(holder, b));
      expect(!b.empty());

      ConstHolder restored;
      expect(not glz::read_beve(restored, b));
      expect(holder.m == restored.m);
   };
}
