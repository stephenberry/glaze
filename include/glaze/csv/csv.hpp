// Distributed under the MIT license
// Developed by Anyar Inc.

#pragma once

#include <array>
#include <iterator>
#include <fstream>
#include <string_view>
#include <tuple>

#include <sstream>

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif  // !FMT_HEADER_ONLY
#include "fmt/format.h"
#include "fmt/compile.h"

#include "glaze/record/recorder.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/tuple.hpp"

namespace glaze
{
    template <class ...T>
    size_t container_size(const std::variant<T...>& v)
    {
        return std::visit([](auto&& x) -> size_t {
           using ContainerType = std::decay_t<decltype(x)>;
           if constexpr (std::same_as<ContainerType, std::monostate>) {
              throw std::runtime_error("container_size constainer is monostate");
           }
           else {
              return x.size();
           }
        }, v);
    }
   
   template <size_t Offset, class Tuple, std::size_t...Is>
   auto tuple_splitter_impl(Tuple&& tuple, std::index_sequence<Is...>) {
      return std::make_tuple(std::get<Is * 2 + Offset>(tuple)...);
   }
   
   template <class Tuple, std::size_t...Is>
   auto tuple_splitter(Tuple&& tuple) {
      static constexpr auto N = std::tuple_size_v<Tuple>;
      static constexpr auto index_seq = std::make_index_sequence<N / 2>{};
      return std::make_pair(tuple_splitter_impl<0>(tuple, index_seq), tuple_splitter_impl<1>(tuple, index_seq));
   }
   
   template <class Buffer>
   inline void write_csv(Buffer& buffer, const std::string_view sv) {
      buffer.append(sv);
   }
   
   template <class Buffer, class T>
   inline void write_csv(Buffer& buffer,
                         const T x) requires std::is_floating_point_v<T>
   {
      fmt::format_to(std::back_inserter(buffer), FMT_COMPILE("{}"), x);
   }
   
   template <class Buffer, class T>
   inline void write_csv(Buffer& buffer,
                         const T x) requires std::is_integral_v<T>
   {
      if constexpr (std::is_same_v<T, bool>) {
         if (x) {
            buffer.append("1");
         }
         else {
            buffer.append("0");
         }
      }
      else {
         fmt::format_to(std::back_inserter(buffer), FMT_COMPILE("{}"), x);
      }
   }
   
   template <bool RowWise = true, class Buffer, class Tuple>
   inline void write_csv(Buffer& buffer, Tuple&& tuple) requires is_tuple<Tuple>
   {
      const auto tuples = tuple_splitter(std::forward<Tuple>(tuple));
      static constexpr auto N = std::tuple_size_v<Tuple> / 2;
      
      const auto& data = std::get<1>(tuples);
      const auto n = std::get<0>(data).size();
      
      for_each<N>([&](auto I) {
         if (n != std::get<I>(data).size()) {
            throw std::runtime_error("csv::to_file | mismatching dimensions");
         }
      });
      
      // write titles
      const auto& titles = std::get<0>(tuples);
      if constexpr (RowWise) {
         for_each<N>([&](auto I) {
            write_csv(buffer, std::get<I>(titles));
            write_csv(buffer, ",");
            
            const auto& x = std::get<I>(data);
            for (size_t i = 0; i < n; ++i) {
               write_csv(buffer, x[i]);
               write_csv(buffer, ",");
            }
            buffer.append("\n");
         });
      }
      else {
         for_each<N>([&](auto I) {
            write_csv(buffer, std::get<I>(titles));
            if constexpr (I != N - 1) {
               write_csv(buffer, ",");
            }
         });
         buffer.append("\n");
         
         // write out columns of data
         for (size_t i = 0; i < n; ++i) {
            for_each<N>([&](auto I) {
               write_csv(buffer, std::get<I>(data)[i]);
               if constexpr (I != N - 1) {
                  write_csv(buffer, ",");
               }
            });
            buffer.append("\n");
         }
      }
   }

   template <class Map>
   concept is_map = std::same_as<typename Map::value_type, std::pair<const typename Map::key_type, typename Map::mapped_type>>;
   
   template <bool RowWise = true, class Buffer, class Map>
   inline void write_csv(Buffer& buffer, Map&& map) requires is_map<std::decay_t<Map>>
   {
      const auto N = map.size();

      size_t n = std::numeric_limits<size_t>::max();
      for (auto& [title, data] : map) {
         if (n == std::numeric_limits<size_t>::max()) {
            n = data.size();
         }
         else if (n != data.size()) {
            throw std::runtime_error("csv::to_file | mismatching dimensions");
         }
      }

      if constexpr (RowWise) {
         for (auto& [title, data] : map) {
            write_csv(buffer, title);
            write_csv(buffer, ",");

            for (size_t i = 0; i < n; ++i) {
               write_csv(buffer, data[i]);
               write_csv(buffer, ",");
            }
            buffer.append("\n");
         }
      }
      else {
         size_t I = 0;
         for (auto& [title, data] : map) {
            write_csv(buffer, title);
            if (I != N - 1) {
               write_csv(buffer, ",");
            }
            ++I;
         }
         buffer.append("\n");

         // write out columns of data
         for (size_t i = 0; i < n; ++i) {
            I = 0;
            for (auto& [title, data] : map) {
               write_csv(buffer, data[i]);
               if (I != N - 1) {
                  write_csv(buffer, ",");
               }
               ++I;
            }
            buffer.append("\n");
         }
      }
   }
   
   template <bool RowWise = true, class Buffer, class... Args> requires (sizeof...(Args) > 1)
     inline void write_csv(Buffer& buffer, Args&&... args)
   {
      write_csv<RowWise>(buffer, std::make_tuple(std::forward<Args>(args)...));
   }
   
   template <bool RowWise = true, class Buffer, class Variant>
   inline void write_csv(Buffer& buffer, recorder<Variant>& rec)
   {
       auto& map = rec.data;

       const auto N = map.size();

       size_t n = std::numeric_limits<size_t>::max();
       for (auto& [title, data] : map) {
           if (n == std::numeric_limits<size_t>::max()) {
               n = container_size(data.first);
           }
           else if (n != container_size(data.first)) {
               throw std::runtime_error("csv::to_file | mismatching dimensions");
           }
       }

       if constexpr (RowWise) {
           for (auto& [title, data] : map) {
             write_csv(buffer, title);
              write_csv(buffer, ",");

               std::visit([&](auto&& arg) {
                  using ContainerType = std::decay_t<decltype(arg)>;
                  if constexpr (std::same_as<ContainerType, std::monostate>) {
                     throw std::runtime_error("write_csv container is monostate");
                  }
                  else {
                     for (size_t i = 0; i < n; ++i) {
                        write_csv(buffer, arg[i]);
                        write_csv(buffer, ",");
                     }
                  }
               }, data.first);
               buffer.append("\n");
           }
       }
       else {
           size_t I = 0;
           for (auto& [title, data] : map) {
              write_csv(buffer, title);
               if (I != N - 1) {
                 write_csv(buffer, ",");
               }
               ++I;
           }
           buffer.append("\n");

           const auto n = map.size();
           for (size_t i = 0; i < n; ++i) {
               I = 0;
               for (auto& [title, data] : map) {
                   std::visit([&](auto&& arg) { write(buffer, arg[i]); }, data.first);
                   if (I != N - 1) {
                      write_csv(buffer, ",");
                   }
                   ++I;
               }
               buffer.append("\n");
           }
       }
   }
   
   template <bool RowWise = true, class... Args>
   inline void to_csv_file(const std::string_view file_name, Args&&... args)
   {
      std::string buffer;
      write_csv<RowWise>(buffer, std::forward<Args>(args)...);
      
      std::ios_base::sync_with_stdio(false);
      std::fstream file(std::string{file_name} + ".csv", std::ios::out);
      
      if (!file) {
         throw std::runtime_error(fmt::format("csv::to_file | file '{}' could not be created", file_name));
      }
      
      file.write(buffer.data(), buffer.size());
   }

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
          throw std::runtime_error("csv::from_file | could not convert to type");
       }

       container.push_back(temp);
   }

   template <bool RowWise = true, class Tuple>
   inline void read_csv(std::fstream& file, Tuple& items) requires is_tuple<Tuple>
   {
       static constexpr auto N = std::tuple_size_v<Tuple>;

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

   // TODO: change fstream to generic buffer? will that be useful?

   template <bool RowWise = true, class... Args>
   inline void from_csv_file(const std::string_view file_name, Args&&... args)
   {
       std::string buffer;

       std::fstream file(std::string{ file_name } + ".csv", std::ios::in);

       auto parameters = std::tie(std::forward<Args>(args)...);

       read_csv<RowWise>(file, parameters);
   }
}


