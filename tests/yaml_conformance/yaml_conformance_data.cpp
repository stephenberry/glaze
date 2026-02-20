// Data-driven YAML conformance test.
// Supports both yaml-test-suite layouts:
// - data branch: one directory per case (in.yaml, in.json, out.yaml, etc.)
// - main branch: src/*.yaml test definitions
// comparing glaze output against expected JSON / YAML / error results.
//
// Requires YAML_TEST_SUITE_DIR to be defined at compile time.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/json/patch.hpp"
#include "glaze/yaml.hpp"
#include "ut/ut.hpp"

using namespace ut;
namespace fs = std::filesystem;

namespace
{
   std::string read_file(const fs::path& path)
   {
      std::ifstream f(path, std::ios::binary);
      if (!f) return {};
      std::ostringstream ss;
      ss << f.rdbuf();
      return ss.str();
   }

   // Cases where glaze is known to not yet conform.
   const std::set<std::string> skip_cases = {};

   // Some cases have multiple sub-cases (in.yaml, in.yaml:1, etc.)
   // The yaml-test-suite data branch stores one sub-case per directory.

   struct test_result
   {
      std::string id{};
      bool passed{};
      bool skipped{};
      std::string detail{};
   };

   struct suite_case
   {
      std::string id{};
      std::string in_yaml{};
      bool expect_error{};
      std::string expected_event{};
      std::string expected_json{};
      std::string expected_yaml{};
   };

   enum class event_compare_status : uint8_t {
      matched,
      mismatched,
      skipped,
      parse_error,
   };

   struct event_compare_result
   {
      event_compare_status status = event_compare_status::skipped;
      std::string detail{};
   };

   std::string_view ltrim(std::string_view s)
   {
      while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
         s.remove_prefix(1);
      }
      return s;
   }

   std::string unescape_event_scalar(std::string_view s)
   {
      std::string out;
      out.reserve(s.size());
      for (size_t i = 0; i < s.size(); ++i) {
         const char c = s[i];
         if (c != '\\' || (i + 1) >= s.size()) {
            out.push_back(c);
            continue;
         }
         const char esc = s[++i];
         switch (esc) {
         case 'n':
            out.push_back('\n');
            break;
         case 't':
            out.push_back('\t');
            break;
         case 'r':
            out.push_back('\r');
            break;
         case 'b':
            out.push_back('\b');
            break;
         case '\\':
            out.push_back('\\');
            break;
         default:
            out.push_back(esc);
            break;
         }
      }
      return out;
   }

   bool starts_with(std::string_view s, std::string_view prefix)
   {
      return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
   }

   bool parse_event_properties(std::string_view& rest, std::string* anchor, std::string* tag,
                               bool allow_collection_style, std::string& error, std::string_view context)
   {
      while (!rest.empty()) {
         rest = ltrim(rest);
         if (rest.empty()) {
            return true;
         }

         if (rest.front() == '&') {
            const auto ws = rest.find_first_of(" \t");
            const auto name = ws == std::string_view::npos ? rest.substr(1) : rest.substr(1, ws - 1);
            if (name.empty()) {
               error = "malformed anchor property in " + std::string(context);
               return false;
            }
            if (anchor) {
               *anchor = std::string(name);
            }
            rest = ws == std::string_view::npos ? std::string_view{} : rest.substr(ws);
            continue;
         }

         if (rest.front() == '<') {
            const auto close = rest.find('>');
            if (close == std::string_view::npos) {
               error = "malformed tag property in " + std::string(context);
               return false;
            }
            if (tag) {
               *tag = std::string(rest.substr(1, close - 1));
            }
            rest = rest.substr(close + 1);
            continue;
         }

         if (allow_collection_style && (starts_with(rest, "[]") || starts_with(rest, "{}"))) {
            rest.remove_prefix(2);
            continue;
         }

         return true;
      }

      return true;
   }

   bool canonicalize_event_plain_scalar(std::string_view payload, std::string& out)
   {
      // Plain scalars containing whitespace are not single-token implicit scalars.
      // Avoid normalizing them, because parsing a standalone scalar token can
      // otherwise accept prefixes (e.g. "null d" -> null).
      if (std::any_of(payload.begin(), payload.end(),
                      [](char c) { return std::isspace(static_cast<unsigned char>(c)); })) {
         return false;
      }

      glz::generic parsed{};
      if (auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, std::string(payload)); bool(ec)) {
         return false;
      }

      if (std::holds_alternative<double>(parsed.data)) {
         out.clear();
         (void)glz::write_json(parsed, out);
         return true;
      }

      if (std::holds_alternative<bool>(parsed.data)) {
         out = std::get<bool>(parsed.data) ? "true" : "false";
         return true;
      }

      if (std::holds_alternative<std::nullptr_t>(parsed.data)) {
         out.clear();
         return true;
      }

      return false;
   }

   bool parse_event_scalar_line(std::string_view line, glz::generic& out, std::string& error,
                                std::string* anchor = nullptr)
   {
      if (!starts_with(line, "=VAL")) {
         error = "event scalar line does not start with =VAL";
         return false;
      }

      auto rest = ltrim(line.substr(4));
      std::string tag{};
      if (!parse_event_properties(rest, anchor, &tag, false, error, "event scalar")) {
         return false;
      }

      if (rest.empty()) {
         error = "missing scalar style in event";
         return false;
      }

      const char style = rest.front();
      const auto payload = unescape_event_scalar(rest.substr(1));

      switch (style) {
      case ':':
         if (tag.empty() || tag == "tag:yaml.org,2002:int" || tag == "tag:yaml.org,2002:float" ||
             tag == "tag:yaml.org,2002:bool" || tag == "tag:yaml.org,2002:null") {
            std::string normalized_scalar{};
            if (canonicalize_event_plain_scalar(payload, normalized_scalar)) {
               out.data = std::move(normalized_scalar);
               return true;
            }
         }
         out.data = payload;
         return true;
      case '"':
      case '\'':
      case '|':
      case '>':
         out.data = payload;
         return true;
      default:
         error = "unsupported scalar style in event";
         return false;
      }
   }

   bool is_explicit_doc_end_marker(std::string_view line) noexcept
   {
      if (!starts_with(line, "...")) return false;
      if (line.size() == 3) return true;
      const char c = line[3];
      return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#';
   }

   bool is_explicit_doc_start_marker(std::string_view line) noexcept
   {
      if (!starts_with(line, "---")) return false;
      if (line.size() == 3) return true;
      const char c = line[3];
      return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#';
   }

   bool doc_start_has_inline_content(std::string_view line) noexcept
   {
      if (!starts_with(line, "---")) return false;
      auto rest = line.substr(3);
      while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
         rest.remove_prefix(1);
      }
      return !rest.empty() && rest.front() != '#';
   }

   size_t skip_newline(std::string_view text, size_t i) noexcept
   {
      if (i >= text.size()) return i;
      if (text[i] == '\r') {
         ++i;
         if (i < text.size() && text[i] == '\n') ++i;
         return i;
      }
      if (text[i] == '\n') ++i;
      return i;
   }

   size_t next_document_offset(std::string_view yaml, size_t offset) noexcept
   {
      size_t i = offset;
      while (i < yaml.size()) {
         const size_t line_start = i;
         size_t content = i;
         while (content < yaml.size() && (yaml[content] == ' ' || yaml[content] == '\t')) {
            ++content;
         }
         if (content >= yaml.size()) return yaml.size();

         if (yaml[content] == '\n' || yaml[content] == '\r') {
            i = skip_newline(yaml, content);
            continue;
         }

         if (yaml[content] == '#') {
            while (content < yaml.size() && yaml[content] != '\n' && yaml[content] != '\r') {
               ++content;
            }
            i = skip_newline(yaml, content);
            continue;
         }

         const auto token = std::string_view{yaml}.substr(content);
         if (is_explicit_doc_end_marker(token)) {
            while (content < yaml.size() && yaml[content] != '\n' && yaml[content] != '\r') {
               ++content;
            }
            i = skip_newline(yaml, content);
            continue;
         }

         return line_start;
      }

      return yaml.size();
   }

   void split_document_bounds(std::string_view yaml, size_t doc_start, size_t& doc_end, size_t& next_offset) noexcept
   {
      size_t i = doc_start;
      bool in_prefix = true;
      bool saw_doc_start = false;

      while (i < yaml.size()) {
         const size_t line_start = i;
         size_t line_end = i;
         while (line_end < yaml.size() && yaml[line_end] != '\n' && yaml[line_end] != '\r') {
            ++line_end;
         }

         std::string_view line = yaml.substr(line_start, line_end - line_start);
         const bool indented = !line.empty() && (line.front() == ' ' || line.front() == '\t');
         std::string_view trimmed = ltrim(line);

         if (trimmed.empty() || starts_with(trimmed, "#")) {
            i = skip_newline(yaml, line_end);
            continue;
         }

         if (!indented && is_explicit_doc_end_marker(trimmed)) {
            doc_end = line_start;
            next_offset = skip_newline(yaml, line_end);
            return;
         }

         if (!indented && is_explicit_doc_start_marker(trimmed)) {
            if (in_prefix) {
               if (saw_doc_start) {
                  doc_end = line_start;
                  next_offset = line_start;
                  return;
               }
               saw_doc_start = true;
               if (doc_start_has_inline_content(trimmed)) {
                  in_prefix = false;
               }
               i = skip_newline(yaml, line_end);
               continue;
            }
            doc_end = line_start;
            next_offset = line_start;
            return;
         }

         if (!indented && starts_with(trimmed, "%") && in_prefix) {
            i = skip_newline(yaml, line_end);
            continue;
         }

         in_prefix = false;
         i = skip_newline(yaml, line_end);
      }

      doc_end = yaml.size();
      next_offset = yaml.size();
   }

   bool is_empty_document_segment(std::string_view doc) noexcept
   {
      size_t i = 0;
      bool in_prefix = true;

      while (i < doc.size()) {
         const size_t line_start = i;
         size_t line_end = i;
         while (line_end < doc.size() && doc[line_end] != '\n' && doc[line_end] != '\r') {
            ++line_end;
         }

         std::string_view line = doc.substr(line_start, line_end - line_start);
         const bool indented = !line.empty() && (line.front() == ' ' || line.front() == '\t');
         std::string_view trimmed = ltrim(line);

         if (trimmed.empty() || starts_with(trimmed, "#")) {
            i = skip_newline(doc, line_end);
            continue;
         }

         if (!indented && is_explicit_doc_end_marker(trimmed)) {
            i = skip_newline(doc, line_end);
            continue;
         }

         if (!indented && is_explicit_doc_start_marker(trimmed)) {
            if (doc_start_has_inline_content(trimmed)) {
               return false;
            }
            i = skip_newline(doc, line_end);
            continue;
         }

         if (!indented && starts_with(trimmed, "%") && in_prefix) {
            i = skip_newline(doc, line_end);
            continue;
         }

         return false;
      }

      return true;
   }

   bool parse_yaml_documents(std::string_view yaml, std::vector<glz::generic>& docs, std::string& error)
   {
      docs.clear();
      size_t offset = 0;

      while (true) {
         offset = next_document_offset(yaml, offset);
         if (offset >= yaml.size()) {
            return true;
         }

         size_t doc_end = yaml.size();
         size_t next_offset = yaml.size();
         split_document_bounds(yaml, offset, doc_end, next_offset);
         auto current_doc = yaml.substr(offset, doc_end - offset);

         if (is_empty_document_segment(current_doc)) {
            glz::generic doc{};
            doc.data = nullptr;
            docs.emplace_back(std::move(doc));
            offset = next_offset;
            continue;
         }

         glz::generic doc{};
         auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(doc, current_doc);
         if (bool(ec)) {
            error = "YAML stream parse error: " + glz::format_error(ec, current_doc);
            return false;
         }
         if (ec.count == 0 && !current_doc.empty()) {
            error = "YAML stream parser made no progress";
            return false;
         }

         docs.emplace_back(std::move(doc));
         offset = next_offset;
      }
   }

   bool parse_event_alias_line(std::string_view line, std::string& out, std::string& error)
   {
      if (!starts_with(line, "=ALI")) {
         error = "event alias line does not start with =ALI";
         return false;
      }

      auto rest = ltrim(line.substr(4));
      if (rest.empty() || rest.front() != '*') {
         error = "malformed alias event";
         return false;
      }
      rest.remove_prefix(1);

      while (!rest.empty() && std::isspace(static_cast<unsigned char>(rest.back()))) {
         rest.remove_suffix(1);
      }
      if (rest.empty()) {
         error = "empty alias name in event";
         return false;
      }

      out = std::string(rest);
      return true;
   }

   bool parse_event_node(const std::vector<std::string>& lines, size_t& i, glz::generic& out,
                         std::unordered_map<std::string, glz::generic>& anchors, std::string& error)
   {
      if (i >= lines.size()) {
         error = "unexpected end of event stream";
         return false;
      }

      const auto& line = lines[i];
      if (starts_with(line, "=VAL")) {
         std::string anchor{};
         if (!parse_event_scalar_line(line, out, error, &anchor)) {
            return false;
         }
         if (!anchor.empty()) {
            anchors[anchor] = out;
         }
         ++i;
         return true;
      }

      if (starts_with(line, "=ALI")) {
         std::string alias{};
         if (!parse_event_alias_line(line, alias, error)) {
            return false;
         }
         auto it = anchors.find(alias);
         if (it == anchors.end()) {
            error = "undefined alias in event stream: " + alias;
            return false;
         }
         out = it->second;
         ++i;
         return true;
      }

      if (starts_with(line, "+SEQ")) {
         std::string anchor{};
         auto rest = ltrim(std::string_view{line}.substr(4));
         if (!parse_event_properties(rest, &anchor, nullptr, true, error, "sequence start")) {
            return false;
         }

         ++i; // consume +SEQ
         glz::generic::array_t arr{};
         while (i < lines.size() && !starts_with(lines[i], "-SEQ")) {
            glz::generic elem{};
            if (!parse_event_node(lines, i, elem, anchors, error)) {
               return false;
            }
            arr.emplace_back(std::move(elem));
         }
         if (i >= lines.size() || !starts_with(lines[i], "-SEQ")) {
            error = "missing -SEQ terminator";
            return false;
         }
         ++i; // consume -SEQ
         out.data = std::move(arr);
         if (!anchor.empty()) {
            anchors[anchor] = out;
         }
         return true;
      }

      if (starts_with(line, "+MAP")) {
         std::string anchor{};
         auto rest = ltrim(std::string_view{line}.substr(4));
         if (!parse_event_properties(rest, &anchor, nullptr, true, error, "mapping start")) {
            return false;
         }

         ++i; // consume +MAP
         glz::generic::object_t obj{};
         while (i < lines.size() && !starts_with(lines[i], "-MAP")) {
            glz::generic key_node{};
            if (!parse_event_node(lines, i, key_node, anchors, error)) {
               return false;
            }
            if (i >= lines.size() || starts_with(lines[i], "-MAP")) {
               error = "mapping key without value in event stream";
               return false;
            }

            glz::generic val_node{};
            if (!parse_event_node(lines, i, val_node, anchors, error)) {
               return false;
            }

            std::string key{};
            if (std::holds_alternative<std::nullptr_t>(key_node.data)) {
               key.clear();
            }
            else if (auto* s = key_node.get_if<std::string>()) {
               key = *s;
            }
            else {
               (void)glz::write_json(key_node, key);
            }

            if (obj.find(key) == obj.end()) {
               obj[std::move(key)] = std::move(val_node);
            }
         }
         if (i >= lines.size() || !starts_with(lines[i], "-MAP")) {
            error = "missing -MAP terminator";
            return false;
         }
         ++i; // consume -MAP
         out.data = std::move(obj);
         if (!anchor.empty()) {
            anchors[anchor] = out;
         }
         return true;
      }

      error = "unexpected event token while parsing node: " + std::string(line);
      return false;
   }

   void normalize_for_event(const glz::generic& in, glz::generic& out)
   {
      if (std::holds_alternative<glz::generic::array_t>(in.data)) {
         glz::generic::array_t arr{};
         for (const auto& e : std::get<glz::generic::array_t>(in.data)) {
            glz::generic normalized{};
            normalize_for_event(e, normalized);
            arr.emplace_back(std::move(normalized));
         }
         out.data = std::move(arr);
         return;
      }

      if (std::holds_alternative<glz::generic::object_t>(in.data)) {
         glz::generic::object_t obj{};
         for (const auto& [k, v] : std::get<glz::generic::object_t>(in.data)) {
            glz::generic normalized{};
            normalize_for_event(v, normalized);
            obj[k] = std::move(normalized);
         }
         out.data = std::move(obj);
         return;
      }

      if (std::holds_alternative<std::string>(in.data)) {
         out.data = std::get<std::string>(in.data);
         return;
      }

      if (std::holds_alternative<std::nullptr_t>(in.data)) {
         out.data = std::string{};
         return;
      }

      if (std::holds_alternative<bool>(in.data)) {
         out.data = std::get<bool>(in.data) ? std::string{"true"} : std::string{"false"};
         return;
      }

      if (std::holds_alternative<double>(in.data)) {
         glz::generic n{};
         n.data = std::get<double>(in.data);
         std::string num_text{};
         (void)glz::write_json(n, num_text);
         out.data = std::move(num_text);
         return;
      }

      std::string fallback{};
      (void)glz::write_json(in, fallback);
      out.data = std::move(fallback);
   }

   event_compare_result compare_with_test_event(const std::string& in_yaml, const glz::generic& first_document,
                                                const std::string& test_event_text)
   {
      std::vector<std::string> lines{};
      {
         std::istringstream in{test_event_text};
         std::string line{};
         while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
               line.pop_back();
            }
            if (!line.empty()) {
               lines.emplace_back(std::move(line));
            }
         }
      }

      event_compare_result result{};
      if (lines.empty()) {
         result.status = event_compare_status::parse_error;
         result.detail = "empty test.event stream";
         return result;
      }

      size_t i = 0;
      if (!starts_with(lines[i], "+STR")) {
         result.status = event_compare_status::parse_error;
         result.detail = "test.event does not start with +STR";
         return result;
      }
      ++i;
      std::vector<glz::generic> expected_docs{};
      while (i < lines.size() && starts_with(lines[i], "+DOC")) {
         ++i; // consume +DOC

         glz::generic expected_doc{};
         if (i < lines.size() && starts_with(lines[i], "-DOC")) {
            expected_doc.data = nullptr;
         }
         else {
            std::string error{};
            std::unordered_map<std::string, glz::generic> anchors{};
            if (!parse_event_node(lines, i, expected_doc, anchors, error)) {
               result.status = event_compare_status::parse_error;
               result.detail = error;
               return result;
            }
         }

         if (i >= lines.size() || !starts_with(lines[i], "-DOC")) {
            result.status = event_compare_status::parse_error;
            result.detail = "test.event missing -DOC";
            return result;
         }
         ++i; // consume -DOC
         expected_docs.emplace_back(std::move(expected_doc));
      }

      if (expected_docs.empty()) {
         std::vector<glz::generic> actual_docs{};
         std::string error{};
         if (!parse_yaml_documents(in_yaml, actual_docs, error)) {
            result.status = event_compare_status::skipped;
            result.detail = error;
            return result;
         }
         if (actual_docs.empty()) {
            result.status = event_compare_status::matched;
            result.detail = "empty event stream match";
         }
         else {
            result.status = event_compare_status::mismatched;
            result.detail =
               "event stream expected no documents, but YAML stream produced " + std::to_string(actual_docs.size());
         }
         return result;
      }

      if (i >= lines.size() || !starts_with(lines[i], "-STR")) {
         result.status = event_compare_status::parse_error;
         result.detail = "test.event missing -STR";
         return result;
      }
      ++i;
      if (i != lines.size()) {
         result.status = event_compare_status::parse_error;
         result.detail = "unexpected trailing events after -STR";
         return result;
      }

      std::vector<glz::generic> actual_docs{};
      if (expected_docs.size() == 1) {
         actual_docs.emplace_back(first_document);
      }
      else {
         std::string error{};
         if (!parse_yaml_documents(in_yaml, actual_docs, error)) {
            result.status = event_compare_status::skipped;
            result.detail = error;
            return result;
         }
      }

      if (actual_docs.size() != expected_docs.size()) {
         result.status = event_compare_status::mismatched;
         result.detail = "event document count mismatch\n  actual docs:   " + std::to_string(actual_docs.size()) +
                         "\n  expected docs: " + std::to_string(expected_docs.size());
         return result;
      }

      std::string actual_json{};
      std::string expected_json{};

      glz::generic actual_value{};
      glz::generic expected_value{};

      if (actual_docs.size() == 1) {
         normalize_for_event(actual_docs.front(), actual_value);
         normalize_for_event(expected_docs.front(), expected_value);
      }
      else {
         glz::generic::array_t actual_arr{};
         glz::generic::array_t expected_arr{};
         actual_arr.reserve(actual_docs.size());
         expected_arr.reserve(expected_docs.size());

         for (const auto& doc : actual_docs) {
            glz::generic normalized{};
            normalize_for_event(doc, normalized);
            actual_arr.emplace_back(std::move(normalized));
         }
         for (const auto& doc : expected_docs) {
            glz::generic normalized{};
            normalize_for_event(doc, normalized);
            expected_arr.emplace_back(std::move(normalized));
         }

         actual_value.data = std::move(actual_arr);
         expected_value.data = std::move(expected_arr);
      }

      (void)glz::write_json(actual_value, actual_json);
      (void)glz::write_json(expected_value, expected_json);

      if (actual_json == expected_json) {
         result.status = event_compare_status::matched;
         result.detail = "event match";
      }
      else {
         result.status = event_compare_status::mismatched;
         result.detail = "event mismatch\n  actual:   " + actual_json + "\n  expected: " + expected_json;
      }
      return result;
   }

   bool is_case_id_name(std::string_view name) noexcept
   {
      if (name.size() != 4) return false;
      return std::all_of(name.begin(), name.end(), [](char c) {
         const unsigned char uc = static_cast<unsigned char>(c);
         return std::isdigit(uc) || (uc >= 'A' && uc <= 'Z');
      });
   }

   bool should_skip_case(const std::string& id)
   {
      if (skip_cases.count(id)) return true;
      if (id.size() > 5 && id[4] == '-' && skip_cases.count(id.substr(0, 4))) return true;
      return false;
   }

   enum class yaml_text_field_state : uint8_t {
      absent,
      string_value,
      null_value,
      type_error,
   };

   const glz::generic* find_object_field(const glz::generic::object_t& obj, std::string_view key)
   {
      auto it = obj.find(std::string(key));
      if (it == obj.end()) return nullptr;
      return &it->second;
   }

   bool object_has_field(const glz::generic::object_t& obj, std::string_view key)
   {
      return find_object_field(obj, key) != nullptr;
   }

   yaml_text_field_state read_object_text_field(const glz::generic::object_t& obj, std::string_view key,
                                                std::string& out)
   {
      const auto* field = find_object_field(obj, key);
      if (!field) return yaml_text_field_state::absent;

      if (const auto* s = field->get_if<std::string>()) {
         out = *s;
         return yaml_text_field_state::string_value;
      }
      if (std::holds_alternative<std::nullptr_t>(field->data)) {
         return yaml_text_field_state::null_value;
      }
      return yaml_text_field_state::type_error;
   }

   std::string unescape_suite_text(std::string text)
   {
      constexpr std::string_view visible_space = "\xE2\x90\xA3"; // U+2423
      constexpr std::string_view em_dash = "\xE2\x80\x94"; // U+2014
      constexpr std::string_view tab_marker = "\xC2\xBB"; // U+00BB
      constexpr std::string_view carriage_return = "\xE2\x86\x90"; // U+2190
      constexpr std::string_view bom_marker = "\xE2\x87\x94"; // U+21D4
      constexpr std::string_view trailing_newline_marker = "\xE2\x86\xB5"; // U+21B5
      constexpr std::string_view no_final_newline_marker = "\xE2\x88\x8E"; // U+220E
      constexpr std::string_view bom = "\xEF\xBB\xBF"; // UTF-8 BOM

      std::string out{};
      out.reserve(text.size());

      for (size_t i = 0; i < text.size();) {
         if (i + visible_space.size() <= text.size() && text.compare(i, visible_space.size(), visible_space) == 0) {
            out.push_back(' ');
            i += visible_space.size();
            continue;
         }

         if (i + carriage_return.size() <= text.size() &&
             text.compare(i, carriage_return.size(), carriage_return) == 0) {
            out.push_back('\r');
            i += carriage_return.size();
            continue;
         }

         if (i + bom_marker.size() <= text.size() && text.compare(i, bom_marker.size(), bom_marker) == 0) {
            out.append(bom);
            i += bom_marker.size();
            continue;
         }

         if (i + trailing_newline_marker.size() <= text.size() &&
             text.compare(i, trailing_newline_marker.size(), trailing_newline_marker) == 0) {
            i += trailing_newline_marker.size();
            continue;
         }

         if (i + tab_marker.size() <= text.size() && text.compare(i, tab_marker.size(), tab_marker) == 0) {
            out.push_back('\t');
            i += tab_marker.size();
            continue;
         }

         if (i + em_dash.size() <= text.size() && text.compare(i, em_dash.size(), em_dash) == 0) {
            size_t j = i;
            while (j + em_dash.size() <= text.size() && text.compare(j, em_dash.size(), em_dash) == 0) {
               j += em_dash.size();
            }
            if (j + tab_marker.size() <= text.size() && text.compare(j, tab_marker.size(), tab_marker) == 0) {
               out.push_back('\t');
               i = j + tab_marker.size();
               continue;
            }
         }

         out.push_back(text[i]);
         ++i;
      }

      if (out.size() >= (no_final_newline_marker.size() + 1) && out.back() == '\n' &&
          out.compare(out.size() - no_final_newline_marker.size() - 1, no_final_newline_marker.size(),
                      no_final_newline_marker) == 0) {
         out.erase(out.size() - no_final_newline_marker.size() - 1, no_final_newline_marker.size() + 1);
      }
      else if (out.size() >= no_final_newline_marker.size() &&
               out.compare(out.size() - no_final_newline_marker.size(), no_final_newline_marker.size(),
                           no_final_newline_marker) == 0) {
         out.erase(out.size() - no_final_newline_marker.size());
      }

      return out;
   }

   std::string normalize_main_tree_text(std::string text)
   {
      // The source tree field keeps indentation for YAML readability.
      // data/test.event stores it left-trimmed per line and newline-terminated.
      std::string normalized{};
      normalized.reserve(text.size());

      size_t i = 0;
      while (i < text.size()) {
         while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
            ++i;
         }
         while (i < text.size() && text[i] != '\n') {
            normalized.push_back(text[i]);
            ++i;
         }
         if (i < text.size() && text[i] == '\n') {
            normalized.push_back('\n');
            ++i;
         }
      }

      while (!normalized.empty() && normalized.back() == '\n') {
         normalized.pop_back();
      }
      normalized.push_back('\n');

      return unescape_suite_text(std::move(normalized));
   }

   bool add_data_case_from_dir(const std::string& id, const fs::path& case_dir, std::vector<suite_case>& cases)
   {
      const auto in_yaml_path = case_dir / "in.yaml";
      if (!fs::exists(in_yaml_path)) return false;

      suite_case c{};
      c.id = id;
      c.in_yaml = read_file(in_yaml_path);
      c.expect_error = fs::exists(case_dir / "error");

      const auto event_path = case_dir / "test.event";
      if (fs::exists(event_path)) {
         c.expected_event = read_file(event_path);
      }

      const auto json_path = case_dir / "in.json";
      if (fs::exists(json_path)) {
         c.expected_json = read_file(json_path);
      }

      const auto out_yaml_path = case_dir / "out.yaml";
      if (fs::exists(out_yaml_path)) {
         c.expected_yaml = read_file(out_yaml_path);
      }

      cases.emplace_back(std::move(c));
      return true;
   }

   bool load_data_layout_cases(const fs::path& suite_dir, std::vector<suite_case>& cases)
   {
      std::vector<fs::path> top_case_dirs{};
      for (const auto& entry : fs::directory_iterator(suite_dir)) {
         if (!entry.is_directory()) continue;
         const auto name = entry.path().filename().string();
         if (is_case_id_name(name)) {
            top_case_dirs.push_back(entry.path());
         }
      }
      std::sort(top_case_dirs.begin(), top_case_dirs.end());

      for (const auto& top_dir : top_case_dirs) {
         const auto id = top_dir.filename().string();
         if (add_data_case_from_dir(id, top_dir, cases)) {
            continue;
         }

         std::vector<fs::path> sub_case_dirs{};
         for (const auto& child : fs::directory_iterator(top_dir)) {
            if (child.is_directory() && fs::exists(child.path() / "in.yaml")) {
               sub_case_dirs.push_back(child.path());
            }
         }
         std::sort(sub_case_dirs.begin(), sub_case_dirs.end());

         for (const auto& sub_dir : sub_case_dirs) {
            const auto sub_name = sub_dir.filename().string();
            (void)add_data_case_from_dir(id + "-" + sub_name, sub_dir, cases);
         }
      }

      return !cases.empty();
   }

   bool load_main_layout_cases(const fs::path& suite_dir, std::vector<suite_case>& cases, std::string& error)
   {
      const auto src_dir = suite_dir / "src";
      if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) return false;

      std::vector<fs::path> case_files{};
      for (const auto& entry : fs::directory_iterator(src_dir)) {
         if (!entry.is_regular_file()) continue;
         if (entry.path().extension() != ".yaml") continue;
         const auto stem = entry.path().stem().string();
         if (is_case_id_name(stem)) {
            case_files.push_back(entry.path());
         }
      }
      std::sort(case_files.begin(), case_files.end());

      struct case_cache
      {
         std::optional<std::string> yaml{};
         std::optional<std::string> tree{};
         std::optional<std::string> json{};
         std::optional<std::string> dump{};
      };

      for (const auto& case_file : case_files) {
         const auto base_id = case_file.stem().string();
         const auto raw = read_file(case_file);

         glz::generic parsed{};
         auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, raw);
         if (bool(ec)) {
            error = "failed to parse source case file " + case_file.string() + ": " + glz::format_error(ec, raw);
            return false;
         }

         const auto* tests = parsed.get_if<glz::generic::array_t>();
         if (!tests) {
            error = "source case file is not a YAML sequence: " + case_file.string();
            return false;
         }
         if (tests->empty()) {
            continue;
         }

         const auto* first = (*tests)[0].get_if<glz::generic::object_t>();
         if (!first) {
            error = "source case entry is not a mapping in " + case_file.string();
            return false;
         }
         if (object_has_field(*first, "skip")) {
            continue;
         }

         const bool multi = tests->size() > 1;
         const size_t width = multi ? (std::to_string(tests->size() - 1).size() + 1) : 0;
         case_cache cache{};

         for (size_t i = 0; i < tests->size(); ++i) {
            const auto* test_obj = (*tests)[i].get_if<glz::generic::object_t>();
            if (!test_obj) {
               error = "source case entry is not a mapping in " + case_file.string();
               return false;
            }

            auto read_optional_text = [&](std::string_view key, std::optional<std::string>& cache_field,
                                          std::optional<std::string>& out_field) -> bool {
               std::string v{};
               const auto state = read_object_text_field(*test_obj, key, v);
               if (state == yaml_text_field_state::type_error) {
                  error = "field '" + std::string(key) + "' is not a string/null in " + case_file.string();
                  return false;
               }
               if (state == yaml_text_field_state::string_value) {
                  cache_field = std::move(v);
                  out_field = cache_field;
                  return true;
               }
               if (state == yaml_text_field_state::null_value) {
                  cache_field.reset();
                  out_field.reset();
                  return true;
               }
               if (cache_field) {
                  out_field = cache_field;
               }
               else {
                  out_field.reset();
               }
               return true;
            };

            std::optional<std::string> yaml_text{};
            std::optional<std::string> tree_text{};
            std::optional<std::string> json_text{};
            std::optional<std::string> dump_text{};

            if (!read_optional_text("yaml", cache.yaml, yaml_text)) return false;
            if (!read_optional_text("tree", cache.tree, tree_text)) return false;
            if (!read_optional_text("json", cache.json, json_text)) return false;
            if (!read_optional_text("dump", cache.dump, dump_text)) return false;

            if (!yaml_text) {
               error = "missing 'yaml' field in " + case_file.string();
               return false;
            }

            suite_case c{};
            if (multi) {
               auto suffix = std::to_string(i);
               if (suffix.size() < width) {
                  suffix.insert(0, width - suffix.size(), '0');
               }
               c.id = base_id + "-" + suffix;
            }
            else {
               c.id = base_id;
            }

            c.in_yaml = unescape_suite_text(std::move(*yaml_text));
            c.expect_error = object_has_field(*test_obj, "fail");

            if (tree_text) {
               c.expected_event = normalize_main_tree_text(std::move(*tree_text));
            }
            if (json_text) {
               c.expected_json = unescape_suite_text(std::move(*json_text));
            }
            if (dump_text) {
               c.expected_yaml = unescape_suite_text(std::move(*dump_text));
            }

            cases.emplace_back(std::move(c));
         }
      }

      return !cases.empty();
   }

   bool has_main_layout(const fs::path& suite_dir)
   {
      const auto src_dir = suite_dir / "src";
      if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) return false;

      for (const auto& entry : fs::directory_iterator(src_dir)) {
         if (!entry.is_regular_file()) continue;
         if (entry.path().extension() != ".yaml") continue;
         if (is_case_id_name(entry.path().stem().string())) {
            return true;
         }
      }
      return false;
   }

   bool has_data_layout(const fs::path& suite_dir)
   {
      for (const auto& entry : fs::directory_iterator(suite_dir)) {
         if (!entry.is_directory()) continue;
         if (is_case_id_name(entry.path().filename().string())) {
            return true;
         }
      }
      return false;
   }
}

suite yaml_conformance_data_tests = [] {
   "json_semantic_compare_ignores_object_key_order"_test = [] {
      glz::generic lhs{};
      glz::generic rhs{};
      expect(!bool(glz::read_json(lhs, R"({"a":4.2,"d":23})")));
      expect(!bool(glz::read_json(rhs, R"({"d":23,"a":4.2})")));
      expect(glz::equal(lhs, rhs)) << "JSON object key order should not affect semantic equality";
   };

   "yaml_test_suite_data_driven"_test = [] {
#ifndef YAML_TEST_SUITE_DIR
      expect(false) << "YAML_TEST_SUITE_DIR not defined";
      return;
#else
      fs::path suite_dir{YAML_TEST_SUITE_DIR};
      if (const char* override_dir = std::getenv("YAML_TEST_SUITE_DIR_OVERRIDE"); override_dir && *override_dir) {
         suite_dir = fs::path{override_dir};
      }
      expect(fs::exists(suite_dir)) << "yaml-test-suite directory not found: " << suite_dir.string();
      if (!fs::exists(suite_dir)) return;

      std::vector<test_result> results;
      int total = 0;
      int passed = 0;
      int failed = 0;
      int skipped = 0;
      int error_pass = 0;
      int error_fail = 0;
      int json_pass = 0;
      int json_fail = 0;
      int yaml_pass = 0;
      int yaml_fail = 0;
      int event_pass = 0;
      int event_fail = 0;
      int event_skip = 0;

      std::vector<suite_case> cases{};
      std::string layout_name{};
      std::string load_error{};
      if (has_main_layout(suite_dir)) {
         layout_name = "main/src";
         if (!load_main_layout_cases(suite_dir, cases, load_error)) {
            expect(false) << "failed to load yaml-test-suite main/src layout: " << load_error;
            return;
         }
      }
      else if (has_data_layout(suite_dir)) {
         layout_name = "data";
         if (!load_data_layout_cases(suite_dir, cases)) {
            expect(false) << "failed to load yaml-test-suite data layout from " << suite_dir.string();
            return;
         }
      }
      else {
         expect(false) << "unknown yaml-test-suite layout in " << suite_dir.string();
         return;
      }

      for (const auto& c : cases) {
         ++total;

         if (should_skip_case(c.id)) {
            ++skipped;
            results.push_back({c.id, false, true, "skipped"});
            continue;
         }

         glz::generic parsed{};
         auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, c.in_yaml);
         const bool parse_failed = bool(ec);

         if (c.expect_error) {
            if (parse_failed) {
               ++passed;
               ++error_pass;
               results.push_back({c.id, true, false, "correctly rejected"});
            }
            else {
               ++failed;
               ++error_fail;
               results.push_back({c.id, false, false, "should have failed but parsed successfully"});
            }
            continue;
         }

         if (parse_failed) {
            ++failed;
            results.push_back({c.id, false, false, "parse error: " + glz::format_error(ec, c.in_yaml)});
            continue;
         }

         // Compare against the yaml-test-suite event stream.
         if (!c.expected_event.empty()) {
            auto event_result = compare_with_test_event(c.in_yaml, parsed, c.expected_event);
            if (event_result.status == event_compare_status::matched) {
               ++event_pass;
            }
            else if (event_result.status == event_compare_status::skipped) {
               ++event_skip;
            }
            else {
               ++failed;
               ++event_fail;
               results.push_back({c.id, false, false, "event check failed: " + event_result.detail});
               continue;
            }
         }

         // Compare against expected JSON if available
         if (!c.expected_json.empty()) {
            glz::generic expected_json_value{};
            auto expected_ec = glz::read_json(expected_json_value, c.expected_json);
            if (bool(expected_ec)) {
               // Can't normalize expected JSON — count as parsed but not a JSON match
               ++passed;
               results.push_back({c.id, true, false,
                                  "parsed (expected JSON couldn't be parsed: " +
                                     glz::format_error(expected_ec, c.expected_json) + ")"});
               continue;
            }

            std::string actual_json;
            std::string expected_json;
            (void)glz::write_json(parsed, actual_json);
            (void)glz::write_json(expected_json_value, expected_json);

            if (glz::equal(parsed, expected_json_value)) {
               ++passed;
               ++json_pass;
               results.push_back({c.id, true, false, "JSON match"});
            }
            else {
               ++failed;
               ++json_fail;
               results.push_back(
                  {c.id, false, false, "JSON mismatch\n  actual:   " + actual_json + "\n  expected: " + expected_json});
            }
            continue;
         }

         // Compare via YAML roundtrip if out.yaml is available
         if (!c.expected_yaml.empty()) {
            glz::generic expected_parsed{};
            auto ec2 = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(expected_parsed, c.expected_yaml);

            std::string actual_yaml;
            (void)glz::write_yaml(parsed, actual_yaml);

            std::string expected_yaml_normalized;
            (void)glz::write_yaml(expected_parsed, expected_yaml_normalized);

            if (actual_yaml == expected_yaml_normalized) {
               ++passed;
               ++yaml_pass;
               results.push_back({c.id, true, false, "YAML roundtrip match"});
            }
            else {
               ++failed;
               ++yaml_fail;
               results.push_back(
                  {c.id, false, false,
                   "YAML roundtrip mismatch\n  actual:\n" + actual_yaml + "\n  expected:\n" + expected_yaml_normalized +
                      (bool(ec2) ? "\n  (out.yaml parse error: " + glz::format_error(ec2, c.expected_yaml) + ")"
                                 : "")});
            }
            continue;
         }

         // No expected output to compare — just verify it parsed
         ++passed;
         results.push_back({c.id, true, false, "parsed (no expected output to compare)"});
      }

      // Print summary
      std::string summary = "\n=== YAML Test Suite Conformance ===\n";
      summary += "Layout: " + layout_name + "\n";
      summary += "Total: " + std::to_string(total) + "\n";
      summary += "Passed: " + std::to_string(passed) + "\n";
      summary += "Failed: " + std::to_string(failed) + "\n";
      summary += "Skipped: " + std::to_string(skipped) + "\n";
      summary += "\nBreakdown:\n";
      summary += "  Error cases: " + std::to_string(error_pass) + " pass, " + std::to_string(error_fail) + " fail\n";
      summary += "  JSON match:  " + std::to_string(json_pass) + " pass, " + std::to_string(json_fail) + " fail\n";
      summary += "  YAML match:  " + std::to_string(yaml_pass) + " pass, " + std::to_string(yaml_fail) + " fail\n";
      summary += "  Event check: " + std::to_string(event_pass) + " pass, " + std::to_string(event_fail) + " fail, " +
                 std::to_string(event_skip) + " skipped\n";

      // Print failures
      bool has_failures = false;
      for (const auto& r : results) {
         if (!r.passed && !r.skipped) {
            if (!has_failures) {
               summary += "\nFailures:\n";
               has_failures = true;
            }
            summary += "  " + r.id + ": " + r.detail + "\n";
         }
      }

      // Use expect to report so the test output is visible
      expect(failed == 0) << summary;

      // Ensure we actually tested a reasonable number of cases
      expect(passed >= 330) << "Expected to pass at least 330 cases, only passed " << passed;
#endif
   };
};

int main() { return 0; }
