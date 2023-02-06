#include "example.hpp"

int main()
{
   using namespace example;
   std::vector<person> directory;
   directory.emplace_back(person{"John", "Doe", 33});
   directory.emplace_back(person{"Alice", "Right", 22});
   
   std::string buffer{};
   glz::write_json(directory, buffer);
   
   std::cout << buffer << "\n\n";
   
   std::array<person, 2> another_directory;
   glz::read_json(another_directory, buffer);
   
   std::string another_buffer{};
   glz::write_json(another_directory, another_buffer);
   
   if (buffer == another_buffer) {
      std::cout << "Directories are the same!\n";
   }
   
   return 0;
}
