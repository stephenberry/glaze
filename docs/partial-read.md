# Partial Read

At times it is not necessary to read the entire JSON document, but rather just a header or some of the initial fields. Glaze provides multiple solutions:

-  `partial_read` in `glz::opts`
-  `partial_read` in `glz::meta`
- Partial reading via [JSON Pointer syntax](./json-pointer-syntax.md)

> [!NOTE]
>
> If you wish partial reading to work on nested objects, you must also turn on `.partial_read_nested = true` in `glz::opts`.

# Partial reading with glz::opts

`partial_read` is a compile time flag in `glz::opts` that indicates only existing array and object elements should be read into, and once the memory has been read, parsing returns without iterating through the rest of the document.

> A [wrapper](./wrappers.md) by the same name also exists.

Example: read only the first two elements into a `std::tuple`

```c++
std::string s = R"(["hello",88,"a string we don't care about"])";
std::tuple<std::string, int> obj{};
expect(!glz::read<glz::opts{.partial_read = true}>(obj, s));
expect(std::get<0>(obj) == "hello");
expect(std::get<1>(obj) == 88);
```

Example: read only the first two elements into a `std::vector`

```c++
std::string s = R"([1,2,3,4,5])";
std::vector<int> v(2);
expect(!glz::read<glz::opts{.partial_read = true}>(v, s));
expect(v.size() == 2);
expect(v[0] = 1);
expect(v[1] = 2);
```

Example: read only the allocated element in a `std::map`

```c++
std::string s = R"({"1":1,"2":2,"3":3})";
std::map<std::string, int> obj{{"2", 0}};
expect(!glz::read<glz::opts{.partial_read = true}>(obj, s));
expect(obj.size() == 1);
expect(obj.at("2") = 2);
```

Example: read only the fields present in a struct

```c++
struct partial_struct
{
   std::string string{};
   int32_t integer{};
};
```

```c++
std::string s = R"({"integer":400,"string":"ha!",ignore})";
partial_struct obj{};
expect(!glz::read<glz::opts{.partial_read = true}>(obj, s));
expect(obj.string == "ha!");
expect(obj.integer == 400);
```

# Partial reading with glz::meta

Glaze allows a `partial_read` flag that can be set to `true` within the glaze metadata.

```json
{
  "id": "187d3cb2-942d-484c-8271-4e2141bbadb1",
  "type": "message_type"
            .....
  // the rest of the large JSON
}
```

When `partial_read` is `true`, parsing will end once all the keys defined in the struct have been parsed.

## partial_read_nested

If your object that you wish to only read part of is nested within other objects, set `partial_read_nested = true` so that Glaze will properly parse the parent objects by skipping to the end of the partially read object.

## Example

```c++
struct Header
{
   std::string id{};
   std::string type{};
};

template <>
struct glz::meta<Header>
{
   static constexpr auto partial_read = true;
};
```

```c++
Header h{};
std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value"})";

expect(glz::read_json(h, buf) == glz::error_code::none);
// Glaze will stop reading after "type" is parsed
expect(h.id == "51e2affb");
expect(h.type == "message_type");
```

## Unit Test Examples

```c++
struct Header
{
   std::string id{};
   std::string type{};
};

template <>
struct glz::meta<Header>
{
   static constexpr auto partial_read = true;
};

struct NestedPartialRead
{
   std::string method{};
   Header header{};
   int number{};
};

suite partial_read_tests = [] {
   using namespace boost::ut;

   "partial read"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value"})";

      expect(glz::read_json(h, buf) == glz::error_code::none);
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read 2"_test = [] {
      Header h{};
      // closing curly bracket is missing
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value"})";

      expect(glz::read_json(h, buf) == glz::error_code::none);
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read unknown key"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(glz::read_json(h, buf) == glz::error_code::unknown_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read unknown key 2"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(glz::read<glz::opts{.error_on_unknown_keys = false}>(h, buf) == glz::error_code::none);
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read missing key"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value"})";

      expect(glz::read<glz::opts{.error_on_unknown_keys = false}>(h, buf) != glz::error_code::missing_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read missing key 2"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb",""unknown key":"value"})";

      expect(glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = false}>(h, buf) !=
             glz::error_code::none);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };
};

suite nested_partial_read_tests = [] {
   using namespace boost::ut;

   "nested object partial read"_test = [] {
      NestedPartialRead n{};
      std::string buf =
         R"({"method":"m1","header":{"id":"51e2affb","type":"message_type","unknown key":"value"},"number":51})";

      expect(glz::read_json(n, buf) == glz::error_code::unknown_key);
      expect(n.method == "m1");
      expect(n.header.id == "51e2affb");
      expect(n.header.type == "message_type");
      expect(n.number == 0);
   };

   "nested object partial read 2"_test = [] {
      NestedPartialRead n{};
      std::string buf =
         R"({"method":"m1","header":{"id":"51e2affb","type":"message_type","unknown key":"value"},"number":51})";

      expect(glz::read<glz::opts{.partial_read_nested = true}>(n, buf) == glz::error_code::none);
      expect(n.method == "m1");
      expect(n.header.id == "51e2affb");
      expect(n.header.type == "message_type");
   };
};
```

