// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct person
{
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
   bool employed{};
};

struct TodoItem
{
   std::string text;
   bool completed;
   std::string priority;
   std::string category;
   size_t id;
   size_t index;
};

struct TodoList
{
   std::string title;
   std::vector<TodoItem> items;
   bool has_items;
   size_t total_count;
};

suite stencil_tests = [] {
   "todo_list"_test = [] {
      std::string_view layout =
         R"({{#items}} {{text}} {{#completed}}✓ {{/completed}}{{^completed}}○ {{/completed}} {{/items}})";

      TodoList list{
         "Mixed Tasks", {{"Task 1", false, "high", "home", 1, 0}, {"Task 2", true, "low", "home", 1, 0}}, true, 2};

      auto result = glz::stencil(layout, list);
      expect(result == " Task 1 ○   Task 2 ✓  ");
   };

   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p);
      expect(result == "Henry Foster 34");
   };

   "person formatted error"_test = [] {
      std::string_view layout = R"({{bad_key}} {{last_name}} {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p);
      expect(result.error());
      auto error_msg = glz::format_error(result, layout);
      expect(error_msg == R"(1:10: unknown_key
   {{bad_key}} {{last_name}} {{age}}
            ^)")
         << error_msg;
   };

   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}}, age: {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster, age: 34") << result;
   };

   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last}}, age: {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unknown_key);
   };

   "comment"_test = [] {
      std::string_view layout = R"({{first_name}} {{! This is a comment }}{{last_name}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster") << result;
   };

   // **Regular Section Tests (#)**

   "section_true"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}})";

      person p{"Alice", "Johnson", 28, true, true}; // employed is true
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Alice Johnson Employed") << result;
   };

   "section_false"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}})";

      person p{"Bob", "Smith", 45, false, false}; // employed is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Bob Smith ") << result; // The section should be skipped
   };

   "section_with_inner_placeholders"_test = [] {
      std::string_view layout =
         R"({{first_name}} {{last_name}} {{#employed}}Status: Employed, Age: {{age}}{{/employed}})";

      person p{"Carol", "Davis", 30, true, true};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Carol Davis Status: Employed, Age: 30") << result;
   };

   "section_with_extra_text"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}}. Welcome!)";

      person p{"Dave", "Miller", 40, true, true};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Dave Miller Employed. Welcome!") << result;
   };

   "section_with_extra_text_skipped"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}}. Welcome!)";

      person p{"Eve", "Wilson", 22, true, false}; // employed is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Eve Wilson . Welcome!") << result;
   };

   "nested_sections"_test = [] {
      std::string_view layout =
         R"({{first_name}} {{last_name}} {{#employed}}Status: Employed {{#hungry}}and Hungry{{/hungry}}{{/employed}})";

      person p1{"Frank", "Taylor", 50, true, true}; // employed is true, hungry is true
      auto result1 = glz::stencil(layout, p1);
      expect(result1 == "Frank Taylor Status: Employed and Hungry");

      person p2{"Grace", "Anderson", 0, false, true}; // employed is true, hungry is false
      auto result2 = glz::stencil(layout, p2);
      expect(result2 == "Grace Anderson Status: Employed ") << result2.value();
   };

   "section_unknown_key"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#unknown}}Should not appear{{/unknown}})";

      person p{"Henry", "Foster", 34, false, true};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unknown_key);
   };

   "section_mismatched_closing_tag"_test = [] {
      std::string_view layout =
         R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employment}})"; // Mismatched closing tag

      person p{"Ivy", "Thomas", 29, false, true};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unexpected_end);
   };

   // **Inverted Section Tests**

   "inverted_section_true"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}})";

      person p{"Henry", "Foster", 34, false}; // hungry is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster I'm not hungry") << result;
   };

   "inverted_section_false"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}})";

      person p{"Henry", "Foster", 34, true}; // hungry is true
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster ") << result; // The inverted section should be skipped
   };

   "inverted_section_with_extra_text_true"_test = [] {
      std::string_view layout =
         R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}}. Have a nice day!)";

      person p{"Henry", "Foster", 34, false}; // hungry is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster I'm not hungry. Have a nice day!") << result;
   };

   "inverted_section_with_extra_text_false"_test = [] {
      std::string_view layout =
         R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}}. Have a nice day!)";

      person p{"Henry", "Foster", 34, true}; // hungry is true
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster . Have a nice day!") << result;
   };

   "nested_inverted_section"_test = [] {
      std::string_view layout =
         R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry {{^employed}}and not employed{{/employed}}{{/hungry}})";

      person p1{"Henry", "Foster", 34, false, false};
      auto result1 = glz::stencil(layout, p1).value_or("error");
      expect(result1 == "Henry Foster I'm not hungry and not employed") << result1;

      person p2{"Henry", "Foster", 34, false, true};
      auto result2 = glz::stencil(layout, p2).value_or("error");
      expect(result2 == "Henry Foster I'm not hungry ") << result2;

      person p3{"Henry", "Foster", 34, true, false};
      std::string_view layout_skip =
         R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry {{^employed}}and not employed{{/employed}}{{/hungry}})";
      auto result3 = glz::stencil(layout_skip, p3).value_or("error");
      expect(result3 == "Henry Foster ") << result3;
   };

   "inverted_section_unknown_key"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^unknown}}Should not appear{{/unknown}})";

      person p{"Henry", "Foster", 34, false};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unknown_key);
   };

   "inverted_section_mismatched_closing_tag"_test = [] {
      std::string_view layout =
         R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hunger}})"; // Mismatched closing tag

      person p{"Henry", "Foster", 34, false};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unexpected_end);
   };
};

struct html_content
{
   std::string title{};
   std::string description{};
   std::string raw_html{};
   std::string safe_text{};
};

struct person_with_html
{
   std::string description = "Working <hard>";
   std::string raw_html = "<strong>Employed</strong>";
   bool employed = true;
};

suite mustache_tests = [] {
   "double_braces_escape_html"_test = [] {
      std::string_view layout = R"(<h1>{{title}}</h1><p>{{description}}</p>)";

      html_content content{"My <Script> Title", "A description with & ampersands and \"quotes\"", "", ""};

      auto result = glz::mustache(layout, content).value_or("error");
      expect(result ==
             "<h1>My &lt;Script&gt; Title</h1><p>A description with &amp; ampersands and &quot;quotes&quot;</p>")
         << result;
   };

   "triple_braces_no_escape"_test = [] {
      std::string_view layout = R"(<div>{{{raw_html}}}</div>)";

      html_content content{"", "", "<strong>Bold text</strong> & <em>italic</em>", ""};

      auto result = glz::mustache(layout, content).value_or("error");
      expect(result == "<div><strong>Bold text</strong> & <em>italic</em></div>") << result;
   };

   "mixed_escaping"_test = [] {
      std::string_view layout = R"(<h1>{{title}}</h1><div>{{{raw_html}}}</div><p>{{description}}</p>)";

      html_content content{"Article <Title>", "Safe & sound content", "<span class=\"highlight\">Important!</span>",
                           ""};

      auto result = glz::mustache(layout, content).value_or("error");
      std::string expected =
         "<h1>Article &lt;Title&gt;</h1>"
         "<div><span class=\"highlight\">Important!</span></div>"
         "<p>Safe &amp; sound content</p>";
      expect(result == expected) << result;
   };

   "all_html_entities_escaped"_test = [] {
      std::string_view layout = R"({{safe_text}})";

      html_content content{"", "", "", "<>&\"'"};

      auto result = glz::mustache(layout, content).value_or("error");
      expect(result == "&lt;&gt;&amp;&quot;&#x27;") << result;
   };

   "triple_braces_preserve_all_chars"_test = [] {
      std::string_view layout = R"({{{raw_html}}})";

      html_content content{"", "", "<>&\"'", ""};

      auto result = glz::mustache(layout, content).value_or("error");
      expect(result == "<>&\"'") << result;
   };

   "section_with_html_escaping"_test = [] {
      std::string_view layout = R"({{#employed}}<p>Status: {{description}} & {{{raw_html}}}</p>{{/employed}})";

      person_with_html p;
      auto result = glz::mustache(layout, p).value_or("error");
      expect(result == "<p>Status: Working &lt;hard&gt; & <strong>Employed</strong></p>") << result;
   };

   "empty_content_handling"_test = [] {
      std::string_view layout = R"(Before: {{title}} | {{{raw_html}}} | After)";

      html_content content{"", "", "", ""};

      auto result = glz::mustache(layout, content).value_or("error");
      expect(result == "Before:  |  | After") << result;
   };

   "malformed_triple_braces"_test = [] {
      std::string_view layout = R"({{{title}})"; // Missing closing brace

      html_content content{"Test", "", "", ""};

      auto result = glz::mustache(layout, content);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::syntax_error);
   };

   "nested_quotes_escaping"_test = [] {
      std::string_view layout = R"(<div title="{{description}}">Content</div>)";

      html_content content{"", "A \"quoted\" value with 'apostrophes'", "", ""};

      auto result = glz::mustache(layout, content).value_or("error");
      expect(result == "<div title=\"A &quot;quoted&quot; value with &#x27;apostrophes&#x27;\">Content</div>")
         << result;
   };
};

suite mustache_example_tests = [] {
   "blog_post_template"_test = [] {
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
</body>
</html>)";

      html_content blog_post{"My <Amazing> Blog Post", "This post discusses \"HTML\" & safety",
                             "<p>This is <strong>formatted content</strong> that should render as HTML.</p>", ""};

      auto result = glz::mustache(blog_template, blog_post).value_or("error");

      // Check that title is escaped in both places
      expect(result.find("My &lt;Amazing&gt; Blog Post") != std::string::npos) << "Title should be escaped";
      // Check that description is escaped
      expect(result.find("This post discusses &quot;HTML&quot; &amp; safety") != std::string::npos)
         << "Description should be escaped";
      // Check that raw HTML is preserved
      expect(result.find("<p>This is <strong>formatted content</strong>") != std::string::npos)
         << "Raw HTML should be preserved";
   };
};

struct Person
{
   std::string name;
   uint32_t age;
   bool active;
};

struct Team
{
   std::string name;
   std::vector<Person> members;
   bool has_members;
};

// User's original structs
struct TodoItemData
{
   std::string text;
   bool completed;
   std::string css_class;
   size_t id;
   size_t index;
   std::string priority;
   std::string category;
   std::string priority_class;
};

struct TodoListData
{
   std::string component_id;
   std::vector<TodoItemData> items;
   bool has_items;
   size_t total_items;
   size_t completed_items;
   size_t pending_items;
};

suite mustache_container_iteration_tests = [] {
   "basic_container_iteration"_test = [] {
      std::string_view layout = R"({{title}}: {{#items}}{{text}} {{/items}})";

      TodoList list{
         "My Tasks", {{"Task 1", false, "high", "work", 1, 0}, {"Task 2", true, "low", "personal", 2, 1}}, true, 2};

      auto result = glz::mustache(layout, list).value_or("error");
      expect(result == "My Tasks: Task 1 Task 2 ") << result;
   };

   "container_with_multiple_properties"_test = [] {
      std::string_view layout = R"({{#items}}[{{id}}:{{text}}:{{priority}}] {{/items}})";

      TodoList list{"Tasks",
                    {{"Buy milk", false, "normal", "shopping", 1, 0}, {"Call mom", true, "high", "personal", 2, 1}},
                    true,
                    2};

      auto result = glz::mustache(layout, list).value_or("error");
      expect(result == "[1:Buy milk:normal] [2:Call mom:high] ") << result;
   };

   "empty_container_iteration"_test = [] {
      std::string_view layout = R"({{title}}: {{#items}}{{text}}{{/items}})";

      TodoList empty_list{"Empty List", {}, false, 0};

      auto result = glz::mustache(layout, empty_list).value_or("error");
      expect(result == "Empty List: ") << result;
   };

   "container_with_nested_boolean_sections"_test = [] {
      std::string_view layout =
         R"({{#items}}{{text}} {{#completed}}✓{{/completed}}{{^completed}}○{{/completed}} {{/items}})";

      TodoList list{"Mixed Tasks",
                    {{"Task 1", false, "high", "work", 1, 0},
                     {"Task 2", true, "low", "personal", 2, 1},
                     {"Task 3", false, "normal", "home", 3, 2}},
                    true,
                    3};

      auto result = glz::mustache(layout, list).value_or("error");
      expect(result == "Task 1 ○ Task 2 ✓ Task 3 ○ ") << result;
   };

   "inverted_container_section_empty"_test = [] {
      std::string_view layout = R"({{title}}{{^items}} - No items found{{/items}})";

      TodoList empty_list{"Empty List", {}, false, 0};

      auto result = glz::mustache(layout, empty_list).value_or("error");
      expect(result == "Empty List - No items found") << result;
   };

   "inverted_container_section_not_empty"_test = [] {
      std::string_view layout = R"({{title}}{{^items}} - No items found{{/items}})";

      TodoList list{"Tasks", {{"Task 1", false, "high", "work", 1, 0}}, true, 1};

      auto result = glz::mustache(layout, list).value_or("error");
      expect(result == "Tasks") << result;
   };

   "mixed_boolean_and_container_sections"_test = [] {
      std::string_view layout =
         R"({{title}} {{#has_items}}({{total_count}} items): {{#items}}{{text}}{{#completed}} ✓{{/completed}} | {{/items}}{{/has_items}}{{^has_items}}No items yet{{/has_items}})";

      TodoList list_with_items{
         "Active List", {{"Task 1", true, "high", "work", 1, 0}, {"Task 2", false, "low", "personal", 2, 1}}, true, 2};

      auto result1 = glz::mustache(layout, list_with_items).value_or("error");
      expect(result1 == "Active List (2 items): Task 1 ✓ | Task 2 | ") << result1;

      TodoList empty_list{"Empty List", {}, false, 0};
      auto result2 = glz::mustache(layout, empty_list).value_or("error");
      expect(result2 == "Empty List No items yet") << result2;
   };

   "container_iteration_with_html_escaping"_test = [] {
      std::string_view layout = R"({{#items}}<p>{{text}}</p>{{/items}})";

      TodoList list{"HTML Test", {{"<script>alert('test')</script>", false, "high", "security", 1, 0}}, true, 1};

      auto result = glz::mustache(layout, list).value_or("error");
      expect(result == "<p>&lt;script&gt;alert(&#x27;test&#x27;)&lt;/script&gt;</p>") << result;
   };

   "nested_container_structures"_test = [] {
      std::string_view layout = R"({{name}}: {{#members}}{{name}} ({{age}}){{#active}} *{{/active}} | {{/members}})";

      Team team{"Engineering", {{"Alice", 30, true}, {"Bob", 25, false}, {"Charlie", 35, true}}, true};

      auto result = glz::mustache(layout, team).value_or("error");
      expect(result == "Engineering: Alice (30) * | Bob (25) | Charlie (35) * | ") << result;
   };
};

suite mustache_list_template_tests = [] {
   "list_template"_test = [] {
      std::string_view layout = R"(
<div class="todo-list" data-component-id="{{component_id}}">
  <h2>Todo List</h2>
  <div class="stats">
    <span>Total: {{total_items}}</span>
    <span>Completed: {{completed_items}}</span>
    <span>Pending: {{pending_items}}</span>
  </div>
  {{#has_items}}
  <ul class="todo-items">
    {{#items}}
    <li class="todo-item {{css_class}} {{priority_class}}" data-id="{{id}}">
      <span class="todo-text">{{text}}</span>
      <span class="todo-status">{{#completed}}☑{{/completed}}{{^completed}}☐{{/completed}}</span>
      <span class="priority priority-{{priority}}">{{priority}}</span>
      <span class="category">{{category}}</span>
    </li>
    {{/items}}
  </ul>
  {{/has_items}}
  {{^has_items}}
  <div class="empty-state">
    <p>No items yet. Add some above!</p>
  </div>
  {{/has_items}}
</div>)";

      TodoListData data{"todo-123",
                        {{"Buy groceries", false, "pending", 1, 0, "high", "shopping", "priority-high"},
                         {"Finish report", true, "completed", 2, 1, "normal", "work", "priority-normal"},
                         {"Call dentist", false, "pending", 3, 2, "low", "health", "priority-low"}},
                        true,
                        3,
                        1,
                        2};

      auto result = glz::mustache(layout, data).value_or("error");

      // Verify key components are present
      expect(result.find("data-component-id=\"todo-123\"") != std::string::npos) << "Component ID should be rendered";
      expect(result.find("Total: 3") != std::string::npos) << "Total items should be rendered";
      expect(result.find("Completed: 1") != std::string::npos) << "Completed count should be rendered";
      expect(result.find("Pending: 2") != std::string::npos) << "Pending count should be rendered";

      // Check items are rendered
      expect(result.find("Buy groceries") != std::string::npos) << "First item text should be present";
      expect(result.find("Finish report") != std::string::npos) << "Second item text should be present";
      expect(result.find("Call dentist") != std::string::npos) << "Third item text should be present";

      // Check data attributes and classes
      expect(result.find("data-id=\"1\"") != std::string::npos) << "First item ID should be present";
      expect(result.find("data-id=\"2\"") != std::string::npos) << "Second item ID should be present";
      expect(result.find("data-id=\"3\"") != std::string::npos) << "Third item ID should be present";

      expect(result.find("class=\"todo-item pending priority-high\"") != std::string::npos)
         << "First item classes should be present";
      expect(result.find("class=\"todo-item completed priority-normal\"") != std::string::npos)
         << "Second item classes should be present";

      // Check completion status
      expect(result.find("☐") != std::string::npos) << "Unchecked boxes should be present";
      expect(result.find("☑") != std::string::npos) << "Checked box should be present";

      // Check priorities and categories
      expect(result.find("priority-high") != std::string::npos) << "High priority should be present";
      expect(result.find("shopping") != std::string::npos) << "Shopping category should be present";
      expect(result.find("work") != std::string::npos) << "Work category should be present";
      expect(result.find("health") != std::string::npos) << "Health category should be present";

      // Ensure empty state is not shown
      expect(result.find("No items yet") == std::string::npos) << "Empty state should not be shown";
   };

   "user_empty_todo_list"_test = [] {
      std::string_view layout = R"(
<div class="todo-list" data-component-id="{{component_id}}">
  <h2>Todo List</h2>
  {{#has_items}}
  <ul class="todo-items">
    {{#items}}
    <li>{{text}}</li>
    {{/items}}
  </ul>
  {{/has_items}}
  {{^has_items}}
  <div class="empty-state">
    <p>No items yet. Add some above!</p>
  </div>
  {{/has_items}}
</div>)";

      TodoListData empty_data{"empty-todo", {}, false, 0, 0, 0};

      auto result = glz::mustache(layout, empty_data).value_or("error");

      expect(result.find("data-component-id=\"empty-todo\"") != std::string::npos) << "Component ID should be rendered";
      expect(result.find("No items yet. Add some above!") != std::string::npos) << "Empty state should be shown";
      expect(result.find("<ul class=\"todo-items\">") == std::string::npos) << "Items list should not be present";
   };

   "user_htmx_form_template"_test = [] {
      std::string_view layout = R"(
<div class="todo-list" data-component-id="{{component_id}}">
  <form hx-post="/api/todo/addTodo" hx-target="closest .todo-list" hx-swap="outerHTML">
    <input type="text" name="text" placeholder="Add new item..." required />
    <button type="submit">Add</button>
  </form>
  {{#has_items}}
  <ul class="todo-items">
    {{#items}}
    <li class="todo-item" data-id="{{id}}">
      <form hx-post="/api/todo/toggleTodo" hx-target="closest .todo-list" hx-swap="outerHTML">
        <input type="hidden" name="index" value="{{index}}" />
        <button type="submit">{{#completed}}☑{{/completed}}{{^completed}}☐{{/completed}}</button>
      </form>
      <span>{{text}}</span>
      <form hx-post="/api/todo/deleteTodo" hx-target="closest .todo-list" hx-swap="outerHTML">
        <input type="hidden" name="index" value="{{index}}" />
        <button type="submit">🗑</button>
      </form>
    </li>
    {{/items}}
  </ul>
  {{/has_items}}
</div>)";

      TodoListData data{
         "htmx-todo", {{"Test HTMX", false, "testing", 1, 0, "normal", "dev", "priority-normal"}}, true, 1, 0, 1};

      auto result = glz::mustache(layout, data).value_or("error");

      // Verify HTMX attributes are preserved and not parsed as template syntax
      expect(result.find("hx-post=\"/api/todo/addTodo\"") != std::string::npos)
         << "HTMX post attribute should be preserved";
      expect(result.find("hx-target=\"closest .todo-list\"") != std::string::npos)
         << "HTMX target attribute should be preserved";
      expect(result.find("hx-swap=\"outerHTML\"") != std::string::npos) << "HTMX swap attribute should be preserved";
      expect(result.find("value=\"0\"") != std::string::npos) << "Index value should be rendered";
      expect(result.find("Test HTMX") != std::string::npos) << "Item text should be present";
      expect(result.find("☐") != std::string::npos) << "Unchecked box should be present";
   };
};

#include "glaze/stencil/stencilcount.hpp"

suite stencilcount_tests = [] {
   "basic docstencil"_test = [] {
      std::string_view layout = R"(# About
## {{+}} {{first_name}} {{last_name}}
{{++}} {{first_name}} is {{age}} years old.

## {{+}} Hobbies
{{++}} Outdoor
{{+++}} Running
{{+++}} Hiking
{{+++}} Camping
{{++}} Indoor
{{+++}} Board Games
{{+++}} Cooking

## {{+}} Education
{{++}} College
{{+++}} Math
{{+++}} English
)";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencilcount(layout, p).value_or("error");
      expect(result == R"(# About
## 1. Henry Foster
1.1 Henry is 34 years old.

## 2. Hobbies
2.1 Outdoor
2.1.1 Running
2.1.2 Hiking
2.1.3 Camping
2.1 Indoor
2.1.1 Board Games
2.1.2 Cooking

## 3. Education
3.1 College
3.1.1 Math
3.1.2 English
)") << result;
   };
};

int main() { return 0; }
