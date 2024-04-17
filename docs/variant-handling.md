# Variant Handling

Glaze has full support `std::variant` when writing, and read support when either the type of the underlying JSON is deducible or the current type stored in the variant is the correct type. 

## Basic Types

Types can be auto-deduced if the variant contains at most one type matching each of the fundamental JSON types of [string, number, object, array boolean] or multiple object types. `std::variant<double, std::string>` could be auto deduced but `std::variant<double, float>` cannot be.

Write example:
```c++
std::variant<double, std::string> d = "not_a_fish";
auto s = glz::write_json(d);
expect(s == R"("not_a_fish")");
```

Read example:
```c++
std::variant<int32_t, double> x = 44;
glz::read_json(x, "33");
expect(std::get<int32_t>(x) == 33);
```

## Object Types

### Auto Deduction of Object Types
Objects can be auto deduced based on the presence of unique key combinations.
```c++
struct xy_t
{
   int x{};
   int y{};
};

template <>
struct glz::meta<xy_t>
{
   using T = xy_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};

struct yz_t
{
   int y{};
   int z{};
};

template <>
struct glz::meta<yz_t>
{
   using T = yz_t;
   static constexpr auto value = object("y", &T::y, "z", &T::z);
};

struct xz_t
{
   int x{};
   int z{};
};

template <>
struct glz::meta<xz_t>
{
   using T = xz_t;
   static constexpr auto value = object("x", &T::x, "z", &T::z);
};

suite metaobject_variant_auto_deduction = [] {
   "metaobject_variant_auto_deduction"_test = [] {
      std::variant<xy_t, yz_t, xz_t> var{};

      std::string b = R"({"y":1,"z":2})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<yz_t>(var));
      expect(std::get<yz_t>(var).y == 1);
      expect(std::get<yz_t>(var).z == 2);

      b = R"({"x":5,"y":7})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xy_t>(var));
      expect(std::get<xy_t>(var).x == 5);
      expect(std::get<xy_t>(var).y == 7);

      b = R"({"z":3,"x":4})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xz_t>(var));
      expect(std::get<xz_t>(var).z == 3);
      expect(std::get<xz_t>(var).x == 4);
   };
};
```
### Deduction of Tagged Object Types
If you don't want auto deduction, or if you need to deduce the type based on the value associated with a key, Glaze supports custom tags.

```c++
struct put_action
{
   std::map<std::string, int> data{};
};

template <>
struct glz::meta<put_action>
{
   using T = put_action;
   static constexpr auto value = object("data", &T::data);
};

struct delete_action
{
   std::string data{};
};

template <>
struct glz::meta<delete_action>
{
   using T = delete_action;
   static constexpr auto value = object("data", &T::data);
};

using tagged_variant = std::variant<put_action, delete_action>;

template <>
struct glz::meta<tagged_variant>
{
   static constexpr std::string_view tag = "action";
   static constexpr auto ids = std::array{"PUT", "DELETE"}; //Defaults to glz::name_v of the type if ids is not supplied
};

suite tagged_variant_tests = [] {
   "tagged_variant_write_tests"_test = [] {
      tagged_variant var = delete_action{{"the_internet"}};
      std::string s{};
      glz::write_json(var, s);
      expect(s == R"({"action":"DELETE","data":"the_internet"})");
   };
   
   "tagged_variant_read_tests"_test = [] {
      tagged_variant var{};
      expect(glz::read_json(var, R"({"action":"DELETE","data":"the_internet"})") == glz::error_code::none);
      expect(std::get<delete_action>(var).data == "the_internet");
   };
};
```

## BEVE to JSON

BEVE uses the variant index to denote the type in a variant. When calling `glz::beve_to_json`, variants will be written in JSON with `"index"` and `"value"` keys. The index indicates the type, which would correspond to a `std::variant` `index()` method.

```json
{
  "index": 1,
  "value": "my value"
}
```

> BEVE conversion to JSON does not support `string` tags, to simplify the specification and avoid bifurcation of variant handling. Using the index is more efficient in binary and more directly translated to `std::variant`.
