# CSV (Comma Separated Values)

Glaze supports [CSV](https://en.wikipedia.org/wiki/Comma-separated_values) reading and writing for structs and standard map types.

By default Glaze writes out structures row-wise, which is more efficient if all the data is available at once. Column-wise CSV format is also supported.

> Note that if you want containers to have their CSV data aligned, they need to be the same length.

## API

```c++
glz::read_csv(obj, buffer); // row-wise 
glz::read_csv<glz::colwise>(obj, buffer); // column-wise

glz::write_csv(obj, buffer); // row-wise
glz::write_csv<glz::colwise>(obj, buffer); // column-wise
```

## Example

```c++
struct my_struct
{
   std::vector<int> num1{};
   std::deque<float> num2{};
   std::vector<bool> maybe{};
   std::vector<std::array<int, 3>> v3s{};
};
```

Reading/Writing column-wise:

```c++
std::string input_col =
       R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4)";

    my_struct obj{};

    glz::read_csv<glz::colwise>(obj, input_col);

    expect(obj.num1[0] == 11);
    expect(obj.num2[2] == 66);
    expect(obj.maybe[3] == false);
    expect(obj.v3s[0] == std::array{1, 1, 1});
    expect(obj.v3s[1] == std::array{2, 2, 2});
    expect(obj.v3s[2] == std::array{3, 3, 3});
    expect(obj.v3s[3] == std::array{4, 4, 4});

    std::string out{};

    glz::write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(obj, out);
    expect(out ==
           R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4
)");

    expect(!glz::write_file_csv<glz::colwise>(obj, "csv_test_colwise.csv", std::string{}));
```

