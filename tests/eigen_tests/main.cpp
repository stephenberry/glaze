// Glaze Library
// For the license information refer to glaze.hpp

#include <chrono>
#include <iostream>
#include <random>
#include <any>

#include "boost/ut.hpp"
#include "glaze/json_ptr.hpp"
#include "glaze/overwrite.hpp"
#include "glaze/read.hpp"
#include "glaze/write.hpp"
#include "glaze/eigen.hpp"

#include <Eigen/Core>
#include <iterator>

int main()
{
  using namespace boost::ut;
  "write"_test = [] {
     Eigen::Matrix<double, -1, -1> m{};
     m.resize(2, 2);
     m << 5, 1, 1, 7;
     std::string json{};
     glaze::write_json(m, json);
     expect(json == "[[2,2],[5,1,1,7]]");
  };

  "write"_test = [] {
     Eigen::Matrix<double, -1, -1> m{};
     glaze::read_json(m, "[[2,1],[7,4]]");
     expect(m.rows() == 2);
     expect(m.cols() == 1);
     expect(m(0,0) == 7);
     expect(m(1,0) == 4);
  };
}
