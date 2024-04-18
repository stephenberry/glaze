# Maximum Floating Point Precision (Writing)

Glaze provides a compile time option (`float_max_write_precision`) to limit the precision when writing numbers to JSON. This improves performance when high precision is not needed in the resulting JSON.

The wrappers `glz::write_float32` and `glz::write_float64` set this compile time option on individual numbers, arrays, or objects. The wrapper `glz::write_float_full` turns off higher level float precision wrapper options.

Example unit tests:

```c++
struct write_precision_t
{
   double pi = std::numbers::pi_v<double>;
   
   struct glaze
   {
      using T = write_precision_t;
      static constexpr auto value = glz::object("pi", glz::write_float32<&T::pi>);
   };
};

suite max_write_precision_tests = [] {
   "max_write_precision"_test = [] {
      double pi = std::numbers::pi_v<double>;
      std::string json_double = glz::write_json(pi);
      
      constexpr glz::opts options{.float_max_write_precision = glz::float_precision::float32};
      std::string json_float = glz::write<options>(pi);
      expect(json_double != json_float);
      expect(json_float == glz::write_json(std::numbers::pi_v<float>));
      expect(!glz::read_json(pi, json_float));
      
      std::vector<double> double_array{ pi, 2 * pi };
      json_double = glz::write_json(double_array);
      json_float = glz::write<options>(double_array);
      expect(json_double != json_float);
      expect(json_float == glz::write_json(std::array{std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>}));
      expect(!glz::read_json(double_array, json_float));
   };
   
   "write_precision_t"_test = [] {
      write_precision_t obj{};
      std::string json_float = glz::write_json(obj);
      expect(json_float == R"({"pi":3.1415927})") << json_float;
   };
};
```

