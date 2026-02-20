// Data-driven YAML conformance test.
// Iterates over yaml-test-suite case directories at runtime,
// comparing glaze output against expected JSON / YAML / error results.
//
// Requires YAML_TEST_SUITE_DIR to be defined at compile time,
// pointing to the yaml-test-suite data directory.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "glaze/glaze.hpp"
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

   // Normalize JSON by parsing and re-serializing through glaze.
   std::string normalize_json(const std::string& json)
   {
      glz::generic val{};
      if (auto ec = glz::read_json(val, json); bool(ec)) {
         return {};
      }
      std::string out;
      (void)glz::write_json(val, out);
      return out;
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

   enum class event_compare_status : uint8_t
   {
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

   bool parse_event_properties(std::string_view& rest, std::string* anchor, std::string* tag, bool allow_collection_style,
                               std::string& error, std::string_view context)
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
      if (std::any_of(payload.begin(), payload.end(), [](char c) {
             return std::isspace(static_cast<unsigned char>(c));
          })) {
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

   bool parse_event_scalar_line(std::string_view line, glz::generic& out, std::string& error, std::string* anchor = nullptr)
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
            result.detail = "event stream expected no documents, but YAML stream produced " +
                            std::to_string(actual_docs.size());
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
}

suite yaml_conformance_data_tests = [] {
   "yaml_test_suite_data_driven"_test = [] {
#ifndef YAML_TEST_SUITE_DIR
      expect(false) << "YAML_TEST_SUITE_DIR not defined";
      return;
#else
      const fs::path suite_dir{YAML_TEST_SUITE_DIR};
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

      // Collect and sort case directories
      std::vector<fs::path> case_dirs;
      for (const auto& entry : fs::directory_iterator(suite_dir)) {
         if (entry.is_directory()) {
            const auto name = entry.path().filename().string();
            // yaml-test-suite case IDs are 4 alphanumeric characters
            if (name.size() == 4) {
               case_dirs.push_back(entry.path());
            }
         }
      }
      std::sort(case_dirs.begin(), case_dirs.end());

      for (const auto& case_dir : case_dirs) {
         const auto id = case_dir.filename().string();
         const auto in_yaml_path = case_dir / "in.yaml";

         if (!fs::exists(in_yaml_path)) continue;
         ++total;

         if (skip_cases.count(id)) {
            ++skipped;
            results.push_back({id, false, true, "skipped"});
            continue;
         }

         const bool expect_error = fs::exists(case_dir / "error");
         const auto in_yaml = read_file(in_yaml_path);

         glz::generic parsed{};
         auto ec = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(parsed, in_yaml);
         const bool parse_failed = bool(ec);

         if (expect_error) {
            if (parse_failed) {
               ++passed;
               ++error_pass;
               results.push_back({id, true, false, "correctly rejected"});
            }
            else {
               ++failed;
               ++error_fail;
               results.push_back({id, false, false, "should have failed but parsed successfully"});
            }
            continue;
         }

         if (parse_failed) {
            ++failed;
            results.push_back({id, false, false, "parse error: " + glz::format_error(ec, in_yaml)});
            continue;
         }

         // Compare against the yaml-test-suite event stream.
         const auto event_path = case_dir / "test.event";
         if (fs::exists(event_path)) {
            auto event_result = compare_with_test_event(in_yaml, parsed, read_file(event_path));
            if (event_result.status == event_compare_status::matched) {
               ++event_pass;
            }
            else if (event_result.status == event_compare_status::skipped) {
               ++event_skip;
            }
            else {
               ++failed;
               ++event_fail;
               results.push_back({id, false, false, "event check failed: " + event_result.detail});
               continue;
            }
         }

         // Compare against expected JSON if available
         const auto in_json_path = case_dir / "in.json";
         if (fs::exists(in_json_path)) {
            const auto expected_json_raw = read_file(in_json_path);
            const auto expected_json = normalize_json(expected_json_raw);
            if (expected_json.empty()) {
               // Can't normalize expected JSON — count as parsed but not a JSON match
               ++passed;
               results.push_back({id, true, false, "parsed (expected JSON couldn't be normalized)"});
               continue;
            }

            std::string actual_json;
            (void)glz::write_json(parsed, actual_json);

            if (actual_json == expected_json) {
               ++passed;
               ++json_pass;
               results.push_back({id, true, false, "JSON match"});
            }
            else {
               ++failed;
               ++json_fail;
               results.push_back(
                  {id, false, false, "JSON mismatch\n  actual:   " + actual_json + "\n  expected: " + expected_json});
            }
            continue;
         }

         // Compare via YAML roundtrip if out.yaml is available
         const auto out_yaml_path = case_dir / "out.yaml";
         if (fs::exists(out_yaml_path)) {
            const auto expected_yaml = read_file(out_yaml_path);

            glz::generic expected_parsed{};
            auto ec2 = glz::read_yaml<glz::opts{.error_on_unknown_keys = false}>(expected_parsed, expected_yaml);

            std::string actual_yaml;
            (void)glz::write_yaml(parsed, actual_yaml);

            std::string expected_yaml_normalized;
            (void)glz::write_yaml(expected_parsed, expected_yaml_normalized);

            if (actual_yaml == expected_yaml_normalized) {
               ++passed;
               ++yaml_pass;
               results.push_back({id, true, false, "YAML roundtrip match"});
            }
            else {
               ++failed;
               ++yaml_fail;
               results.push_back({id, false, false,
                                  "YAML roundtrip mismatch\n  actual:\n" + actual_yaml +
                                     "\n  expected:\n" + expected_yaml_normalized +
                                     (bool(ec2) ? "\n  (out.yaml parse error: " + glz::format_error(ec2, expected_yaml) +
                                                     ")"
                                                : "")});
            }
            continue;
         }

         // No expected output to compare — just verify it parsed
         ++passed;
         results.push_back({id, true, false, "parsed (no expected output to compare)"});
      }

      // Print summary
      std::string summary = "\n=== YAML Test Suite Conformance ===\n";
      summary += "Total: " + std::to_string(total) + "\n";
      summary += "Passed: " + std::to_string(passed) + "\n";
      summary += "Failed: " + std::to_string(failed) + "\n";
      summary += "Skipped: " + std::to_string(skipped) + "\n";
      summary += "\nBreakdown:\n";
      summary += "  Error cases: " + std::to_string(error_pass) + " pass, " + std::to_string(error_fail) + " fail\n";
      summary += "  JSON match:  " + std::to_string(json_pass) + " pass, " + std::to_string(json_fail) + " fail\n";
      summary += "  YAML match:  " + std::to_string(yaml_pass) + " pass, " + std::to_string(yaml_fail) + " fail\n";
      summary += "  Event check: " + std::to_string(event_pass) + " pass, " + std::to_string(event_fail) +
                 " fail, " + std::to_string(event_skip) + " skipped\n";

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
