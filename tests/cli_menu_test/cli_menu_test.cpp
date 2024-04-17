// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/ext/cli_menu.hpp"

struct my_functions
{
   std::function<void()> hello = [] { std::printf("Hello\n"); };
   std::function<void()> world = [] { std::printf("World\n"); };
};

template <>
struct glz::meta<my_functions>
{
   using T = my_functions;
   static constexpr auto value = object("hi", &T::hello, &T::world);
};

struct four_t
{
   four_t(glz::make_reflectable) {} // required for reflection since this struct has no members

   std::pair<std::string, int> operator()() { return {"four", 4}; }
};

struct more_functions
{
   std::function<std::string()> one = [] { return "one"; };
   std::function<int()> two = [] { return 2; };
   std::function<std::string_view()> three = [] { return "three"; };
   four_t four{};
};

struct a_special_function
{
   a_special_function(glz::make_reflectable) {}

   glz::raw_json operator()(const std::tuple<int, bool>& in)
   {
      return std::to_string(std::get<0>(in)) + " | " + std::to_string(std::get<1>(in));
   }
};

struct get_pair_t
{
   get_pair_t(glz::make_reflectable) {}

   auto operator()() { return std::pair{"Key", 51}; }
};

struct my_nested_menu
{
   int ignore_me{};
   my_functions first_menu{};
   more_functions second_menu{};
   std::function<int(int)> user_number = [](int x) { return x; };
   std::function<std::string(const std::string&)> user_string = [](const auto& str) { return str; };
   a_special_function special{};
   get_pair_t get_pair;
   std::function<std::string(const glz::help<std::string, "Enter a JSON pointer:">&)> request_json_pointer =
      [](const auto& str) { return str.value; };
   std::function<std::string_view()> help_name = []() { return glz::name_v<glz::help<std::string, "So helpful!">>; };
};

void nested_menu()
{
   my_nested_menu menu{};
   glz::run_cli_menu(menu); // show the command line interface menu
}

int main()
{
   nested_menu();

   return 0;
}
