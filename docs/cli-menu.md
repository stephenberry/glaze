# Command Line Interface (CLI) Menu

Glaze makes it easy to generate command line interface menus from nested structs. Make structs of callable and combine them to build your menu hierarchy. The command names will be reflected from the function names or use a `glz::meta` that you provide.

## Example

```c++
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

struct my_nested_menu
{
   my_functions first_menu{};
   more_functions second_menu{};
   std::function<int(int)> user_number = [](int x) { return x; };
   std::function<std::string(const std::string&)> user_string = [](const auto& str) { return str; };
   a_special_function special{};
};

void nested_menu()
{
   my_nested_menu menu{};
   glz::run_cli_menu(menu); // show the command line interface menu
}
```

The top menu when printed to console will look like:

```
================================
  1   first_menu
  2   second_menu
  3   user_number
  4   user_string
  5   special
  6   Exit Menu
--------------------------------
cmd> 
```

