# Partial Read

At times it is not necessary to read the entire JSON document, but rather just a header or some of the initial fields. Glaze supports this feature by providing a `partial_read` flag that can be set to `true` within the glaze metadata.

```json
{
  "id": "187d3cb2-942d-484c-8271-4e2141bbadb1",
  "type": "message_type"
            .....
  // the rest of the large JSON
}
```

When `partial_read` is `true`, parsing will end once all the keys defined in the struct have been parsed.

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

>  If `error_on_unknown_keys` is set to `false`, then the JSON will be parsed until the end.

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

