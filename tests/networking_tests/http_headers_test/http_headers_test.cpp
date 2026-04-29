#include "glaze/net/http_headers.hpp"

#include <concepts>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ut/ut.hpp"

using namespace ut;

using field_entry = std::pair<std::string, std::string>;

[[nodiscard]] bool fields_equal(auto&& range, std::initializer_list<field_entry> expected)
{
   std::vector<field_entry> actual;
   for (const auto& field : range) {
      actual.emplace_back(field.name, field.value);
   }
   return actual == std::vector<field_entry>{expected};
}

[[nodiscard]] bool strings_equal(auto&& range, std::initializer_list<std::string> expected)
{
   std::vector<std::string> actual;
   for (const auto& value : range) {
      actual.emplace_back(value);
   }
   return actual == std::vector<std::string>{expected};
}

template <class Headers>
concept has_rvalue_begin = requires { std::declval<Headers&&>().begin(); };

template <class Headers>
concept has_rvalue_end = requires { std::declval<Headers&&>().end(); };

template <class Headers>
concept has_rvalue_cbegin = requires { std::declval<Headers&&>().cbegin(); };

template <class Headers>
concept has_rvalue_cend = requires { std::declval<Headers&&>().cend(); };

template <class Headers>
concept has_rvalue_find = requires { std::declval<Headers&&>().find(std::string_view{"name"}); };

template <class Headers>
concept has_rvalue_fields = requires { std::declval<Headers&&>().fields(std::string_view{"name"}); };

template <class Headers>
concept has_rvalue_names = requires { std::declval<Headers&&>().names(); };

template <class Headers>
concept has_rvalue_values = requires { std::declval<Headers&&>().values(); };

template <class Headers>
concept has_rvalue_named_values = requires { std::declval<Headers&&>().values(std::string_view{"name"}); };

template <class Headers>
concept has_rvalue_first_value = requires { std::declval<Headers&&>().first_value(std::string_view{"name"}); };

template <class Headers>
concept has_rvalue_add = requires { std::declval<Headers&&>().add(std::string{}, std::string{}); };

static_assert(std::ranges::viewable_range<glz::http_headers>);
static_assert(std::forward_iterator<glz::http_headers::range_iterator>);
static_assert(std::forward_iterator<glz::http_headers::const_range_iterator>);
static_assert(
   std::same_as<decltype(std::declval<glz::http_headers&>().find(std::string_view{})), glz::http_headers::iterator>);
static_assert(std::same_as<decltype(std::declval<const glz::http_headers&>().find(std::string_view{})),
                           glz::http_headers::const_iterator>);
static_assert(std::same_as<decltype(std::declval<glz::http_headers&>().add(std::string{}, std::string{})),
                           glz::http_headers::iterator>);
static_assert(std::same_as<decltype(std::declval<const glz::http_headers&>().occurrences(std::string_view{})), size_t>);
static_assert(
   std::same_as<std::ranges::range_reference_t<decltype(std::declval<glz::http_headers&>().fields(std::string_view{}))>,
                glz::http_headers::value_type&>);
static_assert(
   std::same_as<
      std::ranges::range_reference_t<decltype(std::declval<const glz::http_headers&>().fields(std::string_view{}))>,
      const glz::http_headers::value_type&>);
static_assert(std::same_as<std::ranges::range_value_t<decltype(std::declval<const glz::http_headers&>().names())>,
                           std::string_view>);
static_assert(std::same_as<std::ranges::range_value_t<decltype(std::declval<const glz::http_headers&>().values())>,
                           std::string_view>);
static_assert(std::same_as<
              std::ranges::range_value_t<decltype(std::declval<const glz::http_headers&>().values(std::string_view{}))>,
              std::string_view>);

static_assert(!has_rvalue_begin<glz::http_headers>);
static_assert(!has_rvalue_end<glz::http_headers>);
static_assert(!has_rvalue_begin<const glz::http_headers>);
static_assert(!has_rvalue_end<const glz::http_headers>);
static_assert(!has_rvalue_cbegin<const glz::http_headers>);
static_assert(!has_rvalue_cend<const glz::http_headers>);
static_assert(!has_rvalue_find<glz::http_headers>);
static_assert(!has_rvalue_find<const glz::http_headers>);
static_assert(!has_rvalue_fields<glz::http_headers>);
static_assert(!has_rvalue_fields<const glz::http_headers>);
static_assert(!has_rvalue_names<const glz::http_headers>);
static_assert(!has_rvalue_values<const glz::http_headers>);
static_assert(!has_rvalue_named_values<const glz::http_headers>);
static_assert(!has_rvalue_first_value<const glz::http_headers>);
static_assert(!has_rvalue_add<glz::http_headers>);

suite http_headers_ctor_tests = [] {
   "default_ctor_creates_empty_headers"_test = [] {
      glz::http_headers headers{};

      expect(headers.empty());
      expect(headers.size() == 0U);
      expect(headers.begin() == headers.end());
   };

   "initializer_list_ctor_preserves_field_order"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
         {"Set-Cookie", "a=1"},
      };

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                      {"Set-Cookie", "a=1"},
                                   }));
   };

   "clear_removes_all_fields"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
      };

      headers.clear();

      expect(headers.empty());
      expect(headers.size() == 0U);
      expect(headers.begin() == headers.end());
   };

   "begin_and_end_iterate_over_all_fields"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
         {"Connection", "keep-alive"},
      };

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                      {"Connection", "keep-alive"},
                                   }));
   };

   "cbegin_and_cend_iterate_over_all_fields"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
         {"Connection", "keep-alive"},
      };

      std::vector<field_entry> actual;
      for (auto iterator = headers.cbegin(); iterator != headers.cend(); ++iterator) {
         actual.emplace_back(iterator->name, iterator->value);
      }

      expect(actual == std::vector<field_entry>{
                          {"Host", "example.com"},
                          {"Accept", "application/json"},
                          {"Connection", "keep-alive"},
                       });
   };
};

suite http_headers_lookup_tests = [] {
   "contains_is_case_insensitive"_test = [] {
      const glz::http_headers headers{
         {"Content-Type", "application/json"},
      };

      expect(headers.contains("content-type"));
      expect(headers.contains("CONTENT-TYPE"));
      expect(headers.contains("CoNtEnT-TyPe"));
   };

   "contains_returns_false_for_non_matching_names"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
      };

      expect(!headers.contains("Hosts"));
      expect(!headers.contains("Hos"));
      expect(!headers.contains("Host "));
      expect(!headers.contains("X-Host"));
   };

   "find_returns_first_matching_field_case_insensitively"_test = [] {
      glz::http_headers headers{
         {"Set-Cookie", "first=1"},
         {"Host", "example.com"},
         {"set-cookie", "second=2"},
      };

      const auto iterator = headers.find("SET-COOKIE");

      expect(iterator != headers.end());
      expect(iterator->name == "Set-Cookie");
      expect(iterator->value == "first=1");
   };

   "find_returns_end_when_field_is_missing"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
      };

      expect(headers.find("missing") == headers.end());
   };

   "non_const_find_allows_mutating_matching_field"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
      };

      auto iterator = headers.find("host");
      iterator->value = "api.example.com";

      const auto value = headers.first_value("HOST");
      expect(value.has_value());
      expect(*value == "api.example.com");
   };

   "occurrences_counts_all_matching_fields_case_insensitively"_test = [] {
      const glz::http_headers headers{
         {"Set-Cookie", "a=1"},
         {"Host", "example.com"},
         {"set-cookie", "b=2"},
         {"SET-COOKIE", "c=3"},
      };

      expect(headers.occurrences("set-cookie") == 3U);
      expect(headers.occurrences("SET-COOKIE") == 3U);
   };

   "occurrences_returns_zero_when_field_is_missing"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
      };

      expect(headers.occurrences("missing") == 0U);
   };

   "first_value_returns_first_matching_value"_test = [] {
      const glz::http_headers headers{
         {"Set-Cookie", "first=1"},
         {"set-cookie", "second=2"},
      };

      const auto value = headers.first_value("SET-COOKIE");

      expect(value.has_value());
      expect(*value == "first=1");
   };

   "first_value_returns_nullopt_when_field_is_missing"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
      };

      expect(!headers.first_value("Content-Length").has_value());
   };

   "lookup_functions_support_empty_header_names"_test = [] {
      const glz::http_headers headers{
         {"", "first-empty"},
         {"Host", "example.com"},
         {"", "second-empty"},
      };

      expect(headers.contains(""));
      expect(headers.occurrences("") == 2U);

      const auto value = headers.first_value("");
      expect(value.has_value());
      expect(*value == "first-empty");
   };
};

suite http_headers_field_range_tests = [] {
   "fields_returns_all_matching_fields_in_original_order"_test = [] {
      glz::http_headers headers{
         {"Set-Cookie", "a=1"},          {"Host", "example.com"}, {"set-cookie", "b=2"},
         {"Accept", "application/json"}, {"SET-COOKIE", "c=3"},
      };

      expect(fields_equal(headers.fields("set-cookie"), {
                                                           {"Set-Cookie", "a=1"},
                                                           {"set-cookie", "b=2"},
                                                           {"SET-COOKIE", "c=3"},
                                                        }));
   };

   "fields_returns_empty_range_when_name_is_missing"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
      };

      expect(fields_equal(headers.fields("missing"), {}));
   };

   "fields_is_case_insensitive"_test = [] {
      glz::http_headers headers{
         {"Content-Type", "application/json"},
         {"content-type", "text/plain"},
      };

      expect(fields_equal(headers.fields("CONTENT-TYPE"), {
                                                             {"Content-Type", "application/json"},
                                                             {"content-type", "text/plain"},
                                                          }));
   };

   "non_const_fields_allows_mutating_each_matching_field"_test = [] {
      glz::http_headers headers{
         {"Set-Cookie", "a=1"},
         {"Host", "example.com"},
         {"set-cookie", "b=2"},
      };

      for (auto& field : headers.fields("set-cookie")) {
         field.value.append("; Secure");
      }

      expect(fields_equal(headers, {
                                      {"Set-Cookie", "a=1; Secure"},
                                      {"Host", "example.com"},
                                      {"set-cookie", "b=2; Secure"},
                                   }));
   };

   "const_fields_returns_only_matching_entries"_test = [] {
      const glz::http_headers headers{
         {"X-Trace", "first"},
         {"Host", "example.com"},
         {"x-trace", "second"},
      };

      expect(fields_equal(headers.fields("X-TRACE"), {
                                                        {"X-Trace", "first"},
                                                        {"x-trace", "second"},
                                                     }));
   };

   "fields_accepts_temporary_string_lookup_name"_test = [] {
      glz::http_headers headers{
         {"Accept", "application/json"},
         {"accept", "text/plain"},
      };

      expect(fields_equal(headers.fields(std::string{"ACCEPT"}), {
                                                                    {"Accept", "application/json"},
                                                                    {"accept", "text/plain"},
                                                                 }));
   };

   "matching_iterator_preincrement_skips_non_matching_entries"_test = [] {
      glz::http_headers headers{
         {"X-Trace", "first"},           {"Host", "example.com"}, {"x-trace", "second"},
         {"Accept", "application/json"}, {"X-TRACE", "third"},
      };

      auto matching_fields = headers.fields("x-trace");
      auto iterator = matching_fields.begin();

      expect(iterator->value == "first");

      ++iterator;
      expect(iterator->value == "second");

      ++iterator;
      expect(iterator->value == "third");

      ++iterator;
      expect(iterator == matching_fields.end());
   };

   "matching_iterator_postincrement_returns_previous_position"_test = [] {
      glz::http_headers headers{
         {"X-Trace", "first"},
         {"Host", "example.com"},
         {"x-trace", "second"},
      };

      auto matching_fields = headers.fields("x-trace");
      auto iterator = matching_fields.begin();
      const auto previous = iterator++;

      expect(previous->value == "first");
      expect(iterator->value == "second");
   };

   "matching_iterator_arrow_operator_exposes_current_field"_test = [] {
      glz::http_headers headers{
         {"Set-Cookie", "token=abc"},
      };

      auto matching_fields = headers.fields("set-cookie");
      auto iterator = matching_fields.begin();

      expect(iterator->name == "Set-Cookie");
      expect(iterator->value == "token=abc");
   };

   "matching_iterators_compare_equal_when_lookup_name_differs_only_by_case"_test = [] {
      glz::http_headers headers{
         {"Set-Cookie", "a=1"},
         {"Host", "example.com"},
         {"set-cookie", "b=2"},
      };

      auto lower = headers.fields("set-cookie");
      auto upper = headers.fields("SET-COOKIE");

      auto lower_iterator = lower.begin();
      auto upper_iterator = upper.begin();

      expect(lower_iterator == upper_iterator);

      ++lower_iterator;
      ++upper_iterator;

      expect(lower_iterator == upper_iterator);
      expect(lower.end() == upper.end());
   };

   "fields_support_empty_header_names"_test = [] {
      glz::http_headers headers{
         {"", "first-empty"},
         {"Host", "example.com"},
         {"", "second-empty"},
      };

      expect(fields_equal(headers.fields(""), {
                                                 {"", "first-empty"},
                                                 {"", "second-empty"},
                                              }));
   };

   "names_view_preserves_original_names_and_order"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
         {"", "empty-name"},
         {"set-cookie", "a=1"},
      };

      expect(strings_equal(headers.names(), {
                                               "Host",
                                               "",
                                               "set-cookie",
                                            }));
   };

   "values_view_preserves_original_values_and_order"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
         {"Empty-Value", ""},
         {"set-cookie", "a=1"},
      };

      expect(strings_equal(headers.values(), {
                                                "example.com",
                                                "",
                                                "a=1",
                                             }));
   };

   "values_with_name_returns_only_matching_values"_test = [] {
      const glz::http_headers headers{
         {"Set-Cookie", "a=1"},
         {"Host", "example.com"},
         {"set-cookie", "b=2"},
      };

      expect(strings_equal(headers.values("SET-COOKIE"), {
                                                            "a=1",
                                                            "b=2",
                                                         }));
   };

   "values_with_name_supports_empty_header_names"_test = [] {
      const glz::http_headers headers{
         {"", "first-empty"},
         {"Host", "example.com"},
         {"", "second-empty"},
      };

      expect(strings_equal(headers.values(""), {
                                                  "first-empty",
                                                  "second-empty",
                                               }));
   };

   "values_with_name_returns_empty_range_when_name_is_missing"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
      };

      expect(strings_equal(headers.values("missing"), {}));
   };
};

suite http_headers_mutation_tests = [] {
   "add_appends_field_and_returns_iterator_to_new_entry"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
      };

      const auto iterator = headers.add("Accept", "application/json");

      expect(iterator == std::prev(headers.end()));
      expect(iterator->name == "Accept");
      expect(iterator->value == "application/json");

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                   }));
   };

   "replace_adds_new_field_when_no_existing_field_matches"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
      };

      headers.replace("Accept", "application/json");

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                   }));
   };

   "replace_removes_all_matching_fields_and_appends_replacement"_test = [] {
      glz::http_headers headers{
         {"Set-Cookie", "a=1"},
         {"Host", "example.com"},
         {"set-cookie", "b=2"},
         {"Accept", "application/json"},
      };

      headers.replace("SET-COOKIE", "replacement=3");

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                      {"SET-COOKIE", "replacement=3"},
                                   }));
   };

   "replace_supports_empty_header_names"_test = [] {
      glz::http_headers headers{
         {"", "first-empty"},
         {"Host", "example.com"},
         {"", "second-empty"},
      };

      headers.replace("", "replacement-empty");

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"", "replacement-empty"},
                                   }));
   };

   "erase_removes_all_matching_fields_case_insensitively"_test = [] {
      glz::http_headers headers{
         {"Set-Cookie", "a=1"},
         {"Host", "example.com"},
         {"set-cookie", "b=2"},
         {"Accept", "application/json"},
      };

      headers.erase("SET-COOKIE");

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                   }));
   };

   "erase_of_missing_name_preserves_existing_fields"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
      };

      headers.erase("missing");

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                   }));
   };

   "erase_on_empty_container_is_noop"_test = [] {
      glz::http_headers headers{};

      headers.erase("missing");

      expect(headers.empty());
      expect(headers.size() == 0U);
   };

   "erase_supports_empty_header_names"_test = [] {
      glz::http_headers headers{
         {"", "first-empty"},
         {"Host", "example.com"},
         {"", "second-empty"},
      };

      headers.erase("");

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                   }));
   };

   "append_copy_appends_all_fields_and_preserves_source"_test = [] {
      glz::http_headers destination{
         {"Host", "example.com"},
      };
      const glz::http_headers source{
         {"Accept", "application/json"},
         {"Connection", "keep-alive"},
      };

      destination.append(source);

      expect(fields_equal(destination, {
                                          {"Host", "example.com"},
                                          {"Accept", "application/json"},
                                          {"Connection", "keep-alive"},
                                       }));

      expect(fields_equal(source, {
                                     {"Accept", "application/json"},
                                     {"Connection", "keep-alive"},
                                  }));
   };

   "append_copy_creates_independent_storage"_test = [] {
      glz::http_headers destination{
         {"Host", "example.com"},
      };
      glz::http_headers source{
         {"Accept", "application/json"},
      };

      destination.append(source);
      source.find("accept")->value = "text/plain";

      expect(fields_equal(destination, {
                                          {"Host", "example.com"},
                                          {"Accept", "application/json"},
                                       }));

      expect(fields_equal(source, {
                                     {"Accept", "text/plain"},
                                  }));
   };

   "append_copy_from_empty_container_is_noop"_test = [] {
      glz::http_headers destination{
         {"Host", "example.com"},
      };
      const glz::http_headers source{};

      destination.append(source);

      expect(fields_equal(destination, {
                                          {"Host", "example.com"},
                                       }));
   };

   "append_copy_to_self_is_noop"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
      };

      headers.append(headers);

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                   }));
   };

   "append_move_appends_all_fields_and_clears_source"_test = [] {
      glz::http_headers destination{
         {"Host", "example.com"},
      };
      glz::http_headers source{
         {"Accept", "application/json"},
         {"Connection", "keep-alive"},
      };

      destination.append(std::move(source));

      expect(fields_equal(destination, {
                                          {"Host", "example.com"},
                                          {"Accept", "application/json"},
                                          {"Connection", "keep-alive"},
                                       }));

      expect(source.empty());
      expect(source.size() == 0U);
   };

   "append_move_from_empty_container_is_noop"_test = [] {
      glz::http_headers destination{
         {"Host", "example.com"},
      };
      glz::http_headers source{};

      destination.append(std::move(source));

      expect(fields_equal(destination, {
                                          {"Host", "example.com"},
                                       }));

      expect(source.empty());
   };

   "append_move_to_self_is_noop"_test = [] {
      glz::http_headers headers{
         {"Host", "example.com"},
         {"Accept", "application/json"},
      };

      headers.append(std::move(headers));

      expect(fields_equal(headers, {
                                      {"Host", "example.com"},
                                      {"Accept", "application/json"},
                                   }));
   };
};

suite http_headers_serialization_tests = [] {
   "serialize_of_empty_headers_is_single_terminal_crlf"_test = [] {
      const glz::http_headers headers{};

      expect(headers.serialize() == "\r\n");
   };

   "serialize_of_single_header_uses_http_header_format"_test = [] {
      const glz::http_headers headers{
         {"Host", "example.com"},
      };

      expect(headers.serialize() == "Host: example.com\r\n\r\n");
   };

   "serialize_of_multiple_headers_preserves_order_and_duplicates"_test = [] {
      const glz::http_headers headers{
         {"Set-Cookie", "a=1"},
         {"Host", "example.com"},
         {"set-cookie", "b=2"},
      };

      expect(headers.serialize() == "Set-Cookie: a=1\r\nHost: example.com\r\nset-cookie: b=2\r\n\r\n");
   };

   "serialize_skips_fields_with_empty_names"_test = [] {
      const glz::http_headers headers{
         {"", "ignored-1"},
         {"Host", "example.com"},
         {"", "ignored-2"},
         {"Accept", "application/json"},
      };

      expect(headers.serialize() == "Host: example.com\r\nAccept: application/json\r\n\r\n");
   };

   "serialize_of_only_empty_names_is_single_terminal_crlf"_test = [] {
      const glz::http_headers headers{
         {"", "ignored-1"},
         {"", "ignored-2"},
      };

      expect(headers.serialize() == "\r\n");
   };

   "serialize_preserves_empty_values"_test = [] {
      const glz::http_headers headers{
         {"X-Empty", ""},
         {"Host", "example.com"},
      };

      expect(headers.serialize() == "X-Empty: \r\nHost: example.com\r\n\r\n");
   };

   "serialize_preserves_value_whitespace_and_punctuation"_test = [] {
      const glz::http_headers headers{
         {"Authorization", "Digest realm=\"api\", nonce=\"abc:123\", qop=\"auth\""},
         {"X-Whitespace", "  leading and trailing  "},
      };

      expect(headers.serialize() ==
             "Authorization: Digest realm=\"api\", nonce=\"abc:123\", qop=\"auth\"\r\nX-Whitespace:   leading and "
             "trailing  \r\n\r\n");
   };

   "serialize_ignores_empty_names_but_other_apis_still_keep_them"_test = [] {
      const glz::http_headers headers{
         {"", "first-empty"},
         {"Host", "example.com"},
         {"", "second-empty"},
      };

      expect(headers.occurrences("") == 2U);
      expect(strings_equal(headers.values(""), {
                                                  "first-empty",
                                                  "second-empty",
                                               }));
      expect(headers.serialize() == "Host: example.com\r\n\r\n");
   };
};

int main() {}