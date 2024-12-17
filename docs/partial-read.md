# Partial Read

At times it is not necessary to read the entire JSON document, but rather just a header or some of the initial fields.

This document describes a limited approach using the `partial_read` option, to quickly exit after parsing fields of interest.

For more advanced (and still performant) partial reading, Glaze provides compile-time and run-time [JMESPath support](./JMESPath.md).

# Partial reading with glz::opts

`partial_read` is a compile time flag in `glz::opts` that indicates only existing array and object elements should be read into, and once the memory has been read, parsing returns without iterating through the rest of the document.

> [!NOTE]
>
> Once any sub-object is read, the parsing will finish. This ensures high performance with short circuiting.

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

Example: read only the fields present in a struct and then short circuit the parse

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

## Unit Test Examples

```c++
struct Header
{
   std::string id{};
   std::string type{};
};

struct HeaderFlipped
{
   std::string type{};
   std::string id{};
};

struct NestedPartialRead
{
   std::string method{};
   Header header{};
   int number{};
};

suite partial_read_tests = [] {
   using namespace ut;
   
   static constexpr glz::opts partial_read{.partial_read = true};

   "partial read"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value"})";

      expect(!glz::read<partial_read>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read 2"_test = [] {
      Header h{};
      // closing curly bracket is missing
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value")";

      expect(!glz::read<partial_read>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read unknown key"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(glz::read<partial_read>(h, buf) == glz::error_code::unknown_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read unknown key 2"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read don't read garbage"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"garbage})";

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read missing key"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value"})";

      expect(glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf) != glz::error_code::missing_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read missing key 2"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value"})";

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read HeaderFlipped"_test = [] {
      HeaderFlipped h{};
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value"})";

      expect(not glz::read<partial_read>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read HeaderFlipped unknown key"_test = [] {
      HeaderFlipped h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(glz::read<partial_read>(h, buf) == glz::error_code::unknown_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read unknown key 2 HeaderFlipped"_test = [] {
      HeaderFlipped h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type","another_field":409845})";

      expect(glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf) == glz::error_code::none);
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };
};

suite nested_partial_read_tests = [] {
   using namespace ut;
   
   static constexpr glz::opts partial_read{.partial_read = true};

   "nested object partial read"_test = [] {
      NestedPartialRead n{};
      std::string buf =
         R"({"method":"m1","header":{"id":"51e2affb","type":"message_type","unknown key":"value"},"number":51})";

      expect(not glz::read<partial_read>(n, buf));
      expect(n.method == "m1");
      expect(n.header.id == "51e2affb");
      expect(n.header.type == "message_type");
      expect(n.number == 0);
   };

   "nested object partial read, don't read garbage"_test = [] {
      NestedPartialRead n{};
      std::string buf =
         R"({"method":"m1","header":{"id":"51e2affb","type":"message_type","unknown key":"value",garbage},"number":51})";

      expect(not glz::read<partial_read>(n, buf));
      expect(n.method == "m1");
      expect(n.header.id == "51e2affb");
      expect(n.header.type == "message_type");
      expect(n.number == 0);
   };
};
```

