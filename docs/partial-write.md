# Partial Write

Glaze supports partial object writing by providing an array of JSON pointers of the fields that should be written.

```c++
struct animals_t
{
   std::string lion = "Lion";
   std::string tiger = "Tiger";
   std::string panda = "Panda";

   struct glaze
   {
      using T = animals_t;
      static constexpr auto value = glz::object(&T::lion, &T::tiger, &T::panda);
   };
};

struct zoo_t
{
   animals_t animals{};
   std::string name{"My Awesome Zoo"};

   struct glaze
   {
      using T = zoo_t;
      static constexpr auto value = glz::object(&T::animals, &T::name);
   };
};

"partial write"_test = [] {
    static constexpr auto partial = glz::json_ptrs("/name", "/animals/tiger");

    zoo_t obj{};
    std::string s{};
    const auto ec = glz::write_json<partial>(obj, s);
    expect(!ec);
    expect(s == R"({"animals":{"tiger":"Tiger"},"name":"My Awesome Zoo"})") << s;
 };
```

