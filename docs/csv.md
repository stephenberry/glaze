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

    expect(!glz::read_csv<glz::colwise>(obj, input_col));

    expect(obj.num1[0] == 11);
    expect(obj.num2[2] == 66);
    expect(obj.maybe[3] == false);
    expect(obj.v3s[0] == std::array{1, 1, 1});
    expect(obj.v3s[1] == std::array{2, 2, 2});
    expect(obj.v3s[2] == std::array{3, 3, 3});
    expect(obj.v3s[3] == std::array{4, 4, 4});

    std::string out{};

    expect(!glz::write<glz::opts{.format = glz::csv, .layout = glz::colwise}>(obj, out));
    expect(out ==
           R"(num1,num2,maybe,v3s[0],v3s[1],v3s[2]
11,22,1,1,1,1
33,44,1,2,2,2
55,66,0,3,3,3
77,88,0,4,4,4
)");

    expect(!glz::write_file_csv<glz::colwise>(obj, "csv_test_colwise.csv", std::string{}));
```

## Headerless CSV for Struct Vectors

You can read and write vectors of structs without headers by disabling headers in `opts_csv`.

```c++
struct data_point { int id; float value; std::string name; };

std::vector<data_point> vec = {
  {1, 10.5f, "Point A"},
  {2, 20.3f, "Point B"}
};

std::string out;
glz::write<glz::opts_csv{.use_headers = false}>(vec, out);
// out:
// 1,10.5,Point A\n
// 2,20.3,Point B\n

std::vector<data_point> back;
glz::read<glz::opts_csv{.use_headers = false}>(back, out);
```

Notes:
- Fields are in declaration order when `use_headers = false`.
- You can also parse CSV that contains a header row by skipping it:

```c++
glz::read<glz::opts_csv{
  .use_headers = false,
  .skip_header_row = true
}>(back, headered_csv_string);
```

## 2D Arrays (matrix-style CSV)

Glaze supports reading and writing 2D arrays such as `std::vector<std::vector<T>>`:

Row-wise (default):

```c++
std::string csv = "1,2,3\n4,5,6\n";
std::vector<std::vector<int>> m;
glz::read<glz::opts_csv{.use_headers = false}>(m, csv);
// m == {{1,2,3},{4,5,6}}

std::string out2;
glz::write<glz::opts_csv{.use_headers = false}>(m, out2);
```

Column-wise (transpose on IO):

```c++
std::vector<std::vector<int>> cols = {{1,4},{2,5},{3,6}}; // 3 columns
std::string out;
glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(cols, out);
// out is row-wise CSV: 1,2,3\n4,5,6\n

std::vector<std::vector<int>> back;
glz::read<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(back, out);
// back == cols
```

Options:
- `skip_header_row` — skip the first row when reading (useful when ingesting headered CSV into 2D arrays without using headers).
- `validate_rectangular` — enforce equal column counts across rows on read.
  - On failure, `read` returns `constraint_violated` and sets a message in `ctx.custom_error_message`.

## CSV Quoting and Special Values

- Strings and chars are quoted when needed; embedded quotes are escaped by doubling them.
- Quoted fields preserve commas, quotes, and newlines (handles both `\n` and `\r\n`).
- Empty fields are parsed as default values (e.g., `0` for numbers, empty string for strings, `false` for booleans).
- Booleans accept `true`/`false` (case-insensitive) and `0`/`1`; empty boolean fields default to `false`.

Examples:

```c++
std::vector<std::string> row = {"Normal", "Has,comma", "Has \"quotes\"", "Multi\nline"};
std::vector<std::vector<std::string>> m = {row};
std::string out;
glz::write<glz::opts_csv{.use_headers = false}>(m, out);
// out contains properly quoted and escaped CSV

std::vector<std::vector<std::string>> back;
glz::read<glz::opts_csv{.use_headers = false}>(back, out);
```

## Choosing the Options API

For CSV, prefer `opts_csv` to access CSV-specific options:

```c++
glz::write<glz::opts_csv{.layout = glz::colwise, .use_headers = false}>(obj, out);
glz::read<glz::opts_csv{.use_headers = false, .skip_header_row = true}>(obj, in);
```

The helper convenience functions (`read_csv`, `write_csv`) remain available for basic cases.
