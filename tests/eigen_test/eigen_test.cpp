// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/ext/eigen.hpp"

#include <Eigen/Core>
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

int main()
{
   using namespace ut;
   "write_json"_test = [] {
      Eigen::Matrix<double, 2, 2> m{};
      m << 5, 1, 1, 7;
      std::string json{};
      expect(not glz::write_json(m, json));
      expect(json == "[5,1,1,7]");
   };

   "read_json"_test = [] {
      Eigen::Matrix<double, 2, 2> m{};
      expect(glz::read_json(m, "[2,1,7,4]") == glz::error_code::none);
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
      expect(glz::read_json(e, b) == glz::error_code::none);
      const bool boolean = m == e;
      expect(boolean);
   };

   "dynamic array beve"_test = [] {
      Eigen::VectorXd m(10);
      for (int i = 0; i < m.size(); ++i) {
         m[i] = i;
      }
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::VectorXd e{};
      expect(!glz::read_beve(e, b));
      const bool boolean = m == e;
      expect(boolean);
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

   "Eigen::MatrixXcd"_test = [] {
      Eigen::MatrixXcd m(3, 3);
      for (int i = 0; i < m.size(); ++i) {
         m.array()(i) = {double(i), 2 * double(i)};
      }
      std::string b;
      expect(not glz::write_beve(m, b));
      Eigen::MatrixXcd e(3, 3);
      expect(!glz::read_beve(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };
}
