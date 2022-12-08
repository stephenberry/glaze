// Distributed under the MIT license
// Developed by Anyar Inc.

#pragma once

#include <fstream>
#include <sstream>

#include "fmt/format.h"
#include "fmt/compile.h"

#include "glaze/record/recorder.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/core/common.hpp"
#include "glaze/core/format.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/core/write.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/error.hpp"
#include "glaze/core/write_chars.hpp"

namespace glz
{
   template<class T>
   inline void convert_value(std::stringstream& buffer, T& var) requires std::is_floating_point_v<T>
   {
       buffer >> var;
   }

   template <class T>
   inline void convert_value(std::stringstream& buffer, T& var) requires std::is_integral_v<T>
   {
       if constexpr (std::is_same_v<T, bool>) 
       {
           if (buffer.str().compare("1") == 0) {
               var = true;
           }
           else if(buffer.str().compare("0") == 0) {
               var = false;
           }
           else {
               buffer.setstate(std::ios::failbit);
           }
       }
       else {
           buffer >> var;
       }
   }
   
   template<class T>
   inline void read_csv(const std::string& buffer, T& container)
   {
       std::stringstream convert(buffer);
       typename T::value_type temp;
       convert_value(convert, temp);

       if (!convert) {
          throw std::runtime_error("csv | could not convert to type");
       }

       container.push_back(temp);
   }

   template <bool RowWise = true>
   inline void read_csv(std::fstream& file, is_std_tuple auto&& items)
   {
       static constexpr auto N = size_v<decltype(items)>;

       if constexpr (RowWise) 
       {
           // for each container
           // read out contents of line

           for_each<N>([&](auto I) {
               auto& item = std::get<I>(items);

               std::string buffer; 
               std::getline(file, buffer);

               std::stringstream line(buffer);

               // first value should be the name
               std::string name;
               std::getline(line, name, ',');

               for (std::string value; std::getline(line, value, ',');)
               {
                   read_csv(value, item);
               }
           });
       }
       else
       {
           // first row should be names
           std::string names;
           std::getline(file, names);

           for (std::string buffer; std::getline(file, buffer);)
           {
               std::stringstream line(buffer);

               for_each<N>([&](auto I) {
                   auto& item = std::get<I>(items);

                   std::string value;
                   if (std::getline(line, value, ',')) {
                       read_csv(value, item);
                   }
               });
           }
       }
   }

   // TODO: change fstream to generic buffer

   template <bool RowWise = true, class... Args>
   inline void from_csv_file(const std::string_view file_name, Args&&... args)
   {
       std::fstream file(std::string{ file_name } + ".csv", std::ios::in);
      
       read_csv<RowWise>(file, std::make_tuple(std::forward<Args>(args)...));
   }
}


