# Stencil/Mustache (String Interpolation)

Glaze provides string interpolation for C++ structs through the `stencil` and `mustache` formats. These provide templating mechanisms for formatting structured data into strings, inspired by the Mustache templating language. This enables the generation of dynamic output by combining predefined templates with C++ structs.

## Basic Usage

```cpp
struct person
{
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
   bool employed{};
};

// Basic interpolation
std::string_view layout = R"({{first_name}} {{last_name}} is {{age}} years old)";
person p{"Henry", "Foster", 34};
auto result = glz::stencil(layout, p);
// Result: "Henry Foster is 34 years old"
```

> [!NOTE]
>
> `result` in these examples is a `std::expected<std::string, glz::error_ctx>`. Like most functions in Glaze (e.g. `glz::write_json`) you can also pass in your own string buffer as the last argument, in which case the return type is `glz::error_ctx`.

## Template Syntax Specification

### Variable Interpolation
- `{{key}}` - Replaces with the value of the specified field from the struct
- `{{{key}}}` - Triple braces for unescaped output (mustache format only)

### Boolean Sections
- `{{#boolean_key}} CONTENT {{/boolean_key}}` - Shows CONTENT if the boolean field is true
- `{{^boolean_key}} CONTENT {{/boolean_key}}` - Shows CONTENT if the boolean field is false (inverted section)

### Container Iteration
- `{{#container_key}} TEMPLATE {{/container_key}}` - Iterates over container elements, applying TEMPLATE to each item
- `{{^container_key}} CONTENT {{/container_key}}` - Shows CONTENT if the container is empty

### Comments
- `{{! This is a comment}}` - Comments are ignored during template processing

## Examples

### Boolean Sections

```cpp
std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Status: Employed, Age: {{age}}{{/employed}})";
person p{"Carol", "Davis", 30, true, true};
auto result = glz::stencil(layout, p);
// Result: "Carol Davis Status: Employed, Age: 30"
```

### Inverted Sections

```cpp
std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}})";
person p{"Henry", "Foster", 34, false}; // hungry is false
auto result = glz::stencil(layout, p);
// Result: "Henry Foster I'm not hungry"
```

### Container Iteration

```cpp
struct TodoItem {
   std::string text;
   bool completed;
   std::string priority;
   size_t id;
};

struct TodoList {
   std::string title;
   std::vector<TodoItem> items;
   bool has_items;
};

std::string_view layout = R"({{title}}: {{#items}}{{text}} ({{priority}}) {{/items}})";
TodoList list{
   "My Tasks", 
   {{"Buy milk", false, "normal", 1}, {"Call mom", true, "high", 2}}, 
   true
};
auto result = glz::stencil(layout, list);
// Result: "My Tasks: Buy milk (normal) Call mom (high) "
```

### Nested Sections

```cpp
std::string_view layout = R"({{#items}}{{text}} {{#completed}}✓{{/completed}}{{^completed}}○{{/completed}} {{/items}})";
TodoList list{"Mixed Tasks", {{"Task 1", false, "high", 1}, {"Task 2", true, "low", 2}}, true};
auto result = glz::stencil(layout, list);
// Result: "Task 1 ○Task 2 ✓"
```

### Empty Container Handling

```cpp
std::string_view layout = R"({{title}}{{^items}} - No items found{{/items}})";
TodoList empty_list{"Empty List", {}, false};
auto result = glz::stencil(layout, empty_list);
// Result: "Empty List - No items found"
```

## Mustache Format

Glaze provides `glz::mustache` with the same interface as `glz::stencil`, but with HTML escaping behavior:

- **Double braces** `{{key}}` - HTML-escaped output (safe for HTML)
- **Triple braces** `{{{key}}}` - Unescaped output (raw HTML)

### HTML Escaping Examples

```cpp
struct html_content {
   std::string title;
   std::string raw_html;
};

// Double braces escape HTML
std::string_view layout = R"(<h1>{{title}}</h1>)";
html_content content{"My <Script> Title", ""};
auto result = glz::mustache(layout, content);
// Result: "<h1>My &lt;Script&gt; Title</h1>"

// Triple braces preserve HTML
std::string_view layout2 = R"(<div>{{{raw_html}}}</div>)";
html_content content2{"", "<strong>Bold text</strong>"};
auto result2 = glz::mustache(layout2, content2);
// Result: "<div><strong>Bold text</strong></div>"
```

### HTML Entities Escaped

The following characters are escaped in mustache double-brace interpolation:
- `<` → `&lt;`
- `>` → `&gt;`
- `&` → `&amp;`
- `"` → `&quot;`
- `'` → `&#x27;`

## Advanced Features

### Complex Template Example

```cpp
std::string_view blog_template = R"(
<!DOCTYPE html>
<html>
<head>
    <title>{{title}}</title>
</head>
<body>
    <h1>{{title}}</h1>
    <p>{{description}}</p>
    <div class="content">
        {{{raw_html}}}
    </div>
    {{#has_items}}
    <ul>
        {{#items}}
        <li class="{{css_class}}">{{text}} {{#completed}}✓{{/completed}}</li>
        {{/items}}
    </ul>
    {{/has_items}}
</body>
</html>)";
```

### Error Handling

The stencil/mustache functions return `std::expected<std::string, glz::error_ctx>`. Common errors include:

- `glz::error_code::unknown_key` - Referenced field doesn't exist in struct
- `glz::error_code::unexpected_end` - Mismatched or missing closing tags
- `glz::error_code::syntax_error` - Malformed template syntax

> [!TIP]
>
> Use `glz::format_error` like the rest of Glaze to generate a nicely formatted error with context. Note that rather than passing the buffer into the formatter, pass in the layout/template string.

```cpp
std::string_view layout = R"({{bad_key}} {{last_name}} {{age}})";

person p{"Henry", "Foster", 34};
auto result = glz::stencil(layout, p);
expect(result.error());
auto error_msg = glz::format_error(result, layout);
```

In this example `error_msg` will look like:

```
1:10: unknown_key
   {{bad_key}} {{last_name}} {{age}}
            ^
```

## StencilCount

Glaze also provides `glz::stencilcount` for automatic numbering in documents.

> [!NOTE]
>
> To use `glz::stencilcount`, you must explicitly include its header:
>
> ```c++
> #include "glaze/stencil/stencilcount.hpp"
> ```

Example usage:
```cpp
std::string_view layout = R"(# About
## {{+}} {{first_name}} {{last_name}}
{{++}} {{first_name}} is {{age}} years old.

## {{+}} Hobbies
{{++}} Outdoor
{{+++}} Running
{{+++}} Hiking
)";

person p{"Henry", "Foster", 34};
auto result = glz::stencilcount(layout, p);
// Result:
// # About
// ## 1. Henry Foster
// 1.1 Henry is 34 years old.
// 
// ## 2. Hobbies
// 2.1 Outdoor
// 2.1.1 Running
// 2.1.2 Hiking
```

### StencilCount Syntax

- `{{+}}` - Major section number (1, 2, 3, ...)
- `{{++}}` - Sub-section number (1.1, 1.2, 2.1, ...)
- `{{+++}}` - Sub-sub-section number (1.1.1, 1.1.2, ...)
- And so on for deeper nesting levels

### Requirements

- Structs must be reflectable (using Glaze reflection) or be glaze objects
- Boolean fields are used for section conditions
- All field names in templates must exist in the struct