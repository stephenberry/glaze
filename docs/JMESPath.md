# JMESPath Support

[JMESPath](https://jmespath.org/tutorial.html) is a query language for JSON.

Glaze currently has limited support for partial reading with JMESPath. Broader support will be added in the future.

- Compile time evaluated JMESPath expressions offer excellent performance.

Example:

```c++
struct Person
{
   std::string first_name{};
   std::string last_name{};
   uint16_t age{};
};

struct Family
{
   Person father{};
   Person mother{};
   std::vector<Person> children{};
};

struct Home
{
   Family family{};
   std::string address{};
};

suite jmespath_read_at_tests = [] {
   "compile-time read_jmespath"_test = [] {
      Home home{.family = {.father = {"Gilbert", "Fox", 28},
                           .mother = {"Anne", "Fox", 30},
                           .children = {{"Lilly"}, {"Vincent"}}}};

      std::string buffer{};
      expect(not glz::write_json(home, buffer));

      std::string first_name{};
      auto ec = glz::read_jmespath<"family.father.first_name">(first_name, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(first_name == "Gilbert");

      Person child{};
      expect(not glz::read_jmespath<"family.children[0]">(child, buffer));
      expect(child.first_name == "Lilly");
      expect(not glz::read_jmespath<"family.children[1]">(child, buffer));
      expect(child.first_name == "Vincent");
   };

   "run-time read_jmespath"_test = [] {
      Home home{.family = {.father = {"Gilbert", "Fox", 28},
                           .mother = {"Anne", "Fox", 30},
                           .children = {{"Lilly"}, {"Vincent"}}}};

      std::string buffer{};
      expect(not glz::write_json(home, buffer));

      std::string first_name{};
      auto ec = glz::read_jmespath("family.father.first_name", first_name, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(first_name == "Gilbert");

      Person child{};
      expect(not glz::read_jmespath("family.children[0]", child, buffer));
      expect(child.first_name == "Lilly");
      expect(not glz::read_jmespath("family.children[1]", child, buffer));
      expect(child.first_name == "Vincent");
   };
};
```

## Run-time Expressions

It can be expensive to tokenize at runtime. The runtime version of `glz::read_jmespath` takes in a `const glz::jmespath_expression&`. This allows expressions to be pre-computed, reused, and cached for better runtime performance.

```c++
Person child{};
// A runtime expression can be pre-computed and saved for more efficient lookups
glz::jmespath_expression expression{"family.children[0]"};
expect(not glz::read_jmespath(expression, child, buffer));
expect(child.first_name == "Lilly");
```

Note that this still works:

```c++
expect(not glz::read_jmespath("family.children[0]", child, buffer));
```

## Slices

```c++
suite jmespath_slice_tests = [] {
   "slice compile-time"_test = [] {
      std::vector<int> data{0,1,2,3,4,5,6,7,8,9};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));
      
      std::vector<int> slice{};
      expect(not glz::read_jmespath<"[0:5]">(slice, buffer));
      expect(slice.size() == 5);
      expect(slice[0] == 0);
      expect(slice[1] == 1);
      expect(slice[2] == 2);
      expect(slice[3] == 3);
      expect(slice[4] == 4);
   };
   
   "slice run-time"_test = [] {
      std::vector<int> data{0,1,2,3,4,5,6,7,8,9};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));
      
      std::vector<int> slice{};
      expect(not glz::read_jmespath("[0:5]", slice, buffer));
      expect(slice.size() == 5);
      expect(slice[0] == 0);
      expect(slice[1] == 1);
      expect(slice[2] == 2);
      expect(slice[3] == 3);
      expect(slice[4] == 4);
   };
   
   "slice compile-time multi-bracket"_test = [] {
      std::vector<std::vector<int>> data{{1,2},{3,4,5},{6,7}};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));
      
      int v{};
      expect(not glz::read_jmespath<"[1][2]">(v, buffer));
      expect(v == 5);
   };
   
   "slice run-time multi-bracket"_test = [] {
      std::vector<std::vector<int>> data{{1,2},{3,4,5},{6,7}};
      std::string buffer{};
      expect(not glz::write_json(data, buffer));
      
      int v{};
      expect(not glz::read_jmespath("[1][2]", v, buffer));
      expect(v == 5);
   };
};
```

