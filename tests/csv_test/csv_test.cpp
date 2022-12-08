// Distributed under the MIT license
// Developed by Anyar Inc.

#include "glaze/csv/csv.hpp"
#include "glaze/record/recorder.hpp"

#define BOOST_UT_DISABLE_MODULE 1

#include "boost/ut.hpp"


#include <cmath>
#include <deque>
#include <iostream>
#include <functional>
#include <numeric>
#include <sstream>

using namespace boost::ut;
using namespace glz;

suite csv_write = [] {

    "rowwise_to_file"_test = [] {

        std::vector<double> x, y;
        std::deque<bool> z;
        for (auto i = 0; i < 100; ++i) {
            const auto a = static_cast<double>(i);
            x.emplace_back(a);
            y.emplace_back(std::sin(a));
            z.emplace_back(i % 2 == 0);
        }

        to_csv_file("rowwise_to_file_test", "x", x, "y", y, "z", z);
    };

    "colwise_to_file"_test = [] {
        std::vector<double> x, y;
        std::deque<bool> z;
        for (auto i = 0; i < 100; ++i) {
            const auto a = static_cast<double>(i);
            x.emplace_back(a);
            y.emplace_back(std::sin(a));
            z.emplace_back(i % 2 == 0);
        }

        to_csv_file<false>("colwise_to_file_test", "z", z, "y", y, "x", x);
    };

    "vector_to_buffer"_test = [] {
        std::vector<double> data(25);
        std::iota(data.begin(), data.end(), 1);

        std::string buffer;
        write_csv(buffer, "data", data);
    };

    "deque_to_buffer"_test = [] {
        std::deque<double> data(25);
        std::iota(data.begin(), data.end(), 1);

        std::string buffer;
       write_csv(buffer, "data", data);
    };

    "array_to_buffer"_test = [] {
        std::array<double, 25> data;
        std::iota(data.begin(), data.end(), 1);

        std::string buffer;
       write_csv(buffer, "data", data);
    };

    "rowwise_map_to_buffer"_test = [] {
        std::vector<double> x, y;
        for (auto i = 0; i < 100; ++i) {
            const auto a = static_cast<double>(i);
            x.emplace_back(a);
            y.emplace_back(std::sin(a));
        }

        std::map<std::string, std::vector<double>> data;
        data["x"] = x;
        data["y"] = y;

        std::string buffer;
       write_csv(buffer, data);
    };

    "colwise_map_to_buffer"_test = [] {
        std::vector<double> x, y;
        for (auto i = 0; i < 100; ++i) {
            const auto a = static_cast<double>(i);
            x.emplace_back(a);
            y.emplace_back(std::sin(a));
        }

        std::map<std::string, std::vector<double>> data;
        data["x"] = x;
        data["y"] = y;

        std::string buffer;
       write_csv<false>(buffer, data);
    };

    "map_mismatch"_test = [] {
        std::map<std::string, std::vector<int>> data;

        for (int i = 0; i < 100; i++)
        {
            data["x"].emplace_back(i);
           if (i % 2) {
              data["y"].emplace_back(i);
           }
        }
       
       expect(nothrow([&] {
          std::string buffer;
         write_csv(buffer, data);
       }));
    };
};

suite csv_read = [] {

    "rowwise_from_file"_test = [] {
        std::vector<double> x, y;
        std::deque<bool> z;

        from_csv_file("rowwise_to_file_test", x, y, z);
    };

    "colwise_from_file"_test = [] {
        std::vector<double> x, y;
        std::deque<bool> z;

        from_csv_file<false>("colwise_to_file_test", z, y, x);
    };

    // more complicated. needs a small discussion on expectations
    //"from_file_to_map"_test = [] {

    //};

    "partial_data"_test = [] {
        std::vector<double> z;
       from_csv_file<false>("colwise_to_file_test", z);
    };

    "wrong_type"_test = [] {
        std::vector<std::string> letters = {"a", "b", "c", "d", "e",
                                               "f", "g", "h", "i", "j",
                                               "k", "l", "m", "n", "o"};

        to_csv_file("letters_file", "letters", letters);

        std::vector<double> not_letters;

        try
        {
           from_csv_file("letters_file", not_letters);

            expect(false);
        }
        catch (std::exception&)
        {
            expect(true);
        }
    };

};

suite csv_recorder = [] {
   "recorder_to_file"_test = [] {
      
      recorder<double, float> rec;
      
      double x = 0.0;
      float y = 0.f;
      
      rec["x"] = x;
      rec["y"] = y;
      
      for (int i = 0; i < 100; ++i)
      {
         x += 1.5;
         y += static_cast<float>(i);
         rec.update();
      }
      
      to_csv_file("recorder_out", rec);
   };
};

int main()
{
   return 0;
}
