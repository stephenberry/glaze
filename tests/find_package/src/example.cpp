#include "example.hpp"

#include <iostream>

int main()
{
   using namespace example;
   std::vector<person> directory;
   directory.emplace_back(person{"John", "Doe", 33});
   directory.emplace_back(person{"Alice", "Right", 22});

   std::string buffer{};
   std::ignore = glz::write_json(directory, buffer);

   std::cout << buffer << "\n\n";

   std::array<person, 2> another_directory;
   [[maybe_unused]] const auto err = glz::read_json(another_directory, buffer);

   std::string another_buffer{};
   std::ignore = glz::write_json(another_directory, another_buffer);

   std::cout << "Directories are " << (buffer != another_buffer ? "NOT" : "") << "the same!\n";

   const auto success = buffer == another_buffer;
   return static_cast<int>(!success);
}
