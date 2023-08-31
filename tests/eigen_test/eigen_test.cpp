// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/eigen.hpp"

#include <Eigen/Core>
#include <any>
#include <chrono>
#include <iostream>
#include <iterator>
#include <random>

#include "boost/ut.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

int main()
{
   using namespace boost::ut;
   "write_json"_test = [] {
      Eigen::Matrix<double, 2, 2> m{};
      m << 5, 1, 1, 7;
      std::string json{};
      glz::write_json(m, json);
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

   "binary"_test = [] {
      Eigen::Matrix<double, 2, 2> m{};
      m << 1, 2, 3, 4;
      std::vector<std::byte> b;
      glz::write_binary(m, b);
      Eigen::Matrix<double, 2, 2> e{};
      expect(!glz::read_binary(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "binary"_test = [] {
      Eigen::MatrixXd m(2, 2);
      m << 1, 2, 3, 4;
      std::vector<std::byte> b;
      glz::write_binary(m, b);
      Eigen::MatrixXd e(2, 2);
      expect(!glz::read_binary(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "array"_test = [] {
      Eigen::Vector3d m{1, 2, 3};
      std::vector<std::byte> b;
      glz::write_binary(m, b);
      Eigen::Vector3d e{};
      expect(!glz::read_binary(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "dynamic array"_test = [] {
      Eigen::VectorXd m(10);
      for (int i = 0; i < m.size(); ++i) {
         m[i] = i;
      }
      std::string b;
      glz::write_json(m, b);
      Eigen::VectorXd e{};
      expect(glz::read_json(e, b) == glz::error_code::none);
      const bool boolean = m == e;
      expect(boolean);
   };

   "dynamic array binary"_test = [] {
      Eigen::VectorXd m(10);
      for (int i = 0; i < m.size(); ++i) {
         m[i] = i;
      }
      std::vector<std::byte> b;
      glz::write_binary(m, b);
      Eigen::VectorXd e{};
      expect(!glz::read_binary(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "Eigen::VectorXcd"_test = [] {
      Eigen::VectorXcd m(10);
      for (int i = 0; i < m.size(); ++i) {
         m[i] = {double(i), 2 * double(i)};
      }
      std::vector<std::byte> b;
      glz::write_binary(m, b);
      Eigen::VectorXcd e{};
      expect(!glz::read_binary(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };

   "Eigen::MatrixXcd"_test = [] {
      Eigen::MatrixXcd m(3, 3);
      for (int i = 0; i < m.size(); ++i) {
         m.array()(i) = {double(i), 2 * double(i)};
      }
      std::vector<std::byte> b;
      glz::write_binary(m, b);
      Eigen::MatrixXcd e(3, 3);
      expect(!glz::read_binary(e, b));
      const bool boolean = m == e;
      expect(boolean);
   };
}
