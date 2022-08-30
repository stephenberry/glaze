// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/read.hpp"
#include "glaze/write.hpp"
#include "glaze/overwrite.hpp"
#include "glaze/file_ops.hpp"

#include <filesystem>
#include <unordered_set>
#include <vector>

namespace glz
{
   struct pointer
   {
      std::string ptr{};
      raw_json data{};
   };

   struct interface_t
   {
      std::vector<std::string> includes{};
      raw_json data{};
      std::vector<pointer> pointers{};

      void clear() {
         includes.clear();
         data.str.clear();
         pointers.clear();
      }
   };
}

template <>
struct glz::meta<glaze::interface_t> {
   using T = glaze::interface_t;
   static constexpr auto value = object("includes", &T::includes, "data", &T::data, "pointers",
                     &T::pointers);
};

namespace glz
{
   template <class Value>
   struct interface_t_parser
   {
      std::function<void(std::string&, std::filesystem::path const&)> get_buffer = [](std::string& buffer, std::filesystem::path const& path) {
         file_to_buffer(buffer, path.generic_string());
      };

      std::unordered_set<std::string> parsing_files{};
      std::string buffer{};
      interface_t spec{};

      void parse(Value& data, std::string_view filename,
                 std::filesystem::path const& working_directory)
      {
         const auto path =
            std::filesystem::canonical(relativize_if_not_absolute(
            working_directory, std::filesystem::path(filename)));
         auto path_str = path.string();
         if (parsing_files.contains(path_str)) {
            throw std::runtime_error("Circular includes detected");
         }
         parsing_files.insert(path_str);

         get_buffer(buffer, path);

         spec.clear();
         read_json(spec, buffer);

         for (auto&& include : spec.includes) {
            parse(data, include,
                  relativize_if_not_absolute(working_directory, include).parent_path());
         }

         read_json(data, spec.data.str);

         for (auto&& pointer : spec.pointers) {
            overwrite(data, pointer.ptr, pointer.data);
         }

         parsing_files.erase(path_str);
      }
   };
}  // namespace glaze
