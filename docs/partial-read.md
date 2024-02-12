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
