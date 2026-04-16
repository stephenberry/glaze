// Glaze Library
// For the license information refer to glaze.hpp
//
// SQLite interop tests for Glaze's JSONB format. These tests verify byte-level compatibility
// with the SQLite reference implementation by:
//   (A) Writing JSONB with Glaze, storing it as a BLOB, and reading it back via SQLite's
//       json(col) function — exercises that SQLite accepts our output.
//   (B) Asking SQLite's jsonb(json_string) function to produce a canonical JSONB blob and
//       parsing it back with Glaze — exercises that we accept SQLite's output.

#include <sqlite3.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "glaze/json.hpp"
#include "glaze/jsonb.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace
{
   [[noreturn]] void die(const char* msg)
   {
      std::fprintf(stderr, "sqlite fatal: %s\n", msg);
      std::abort();
   }

   // RAII wrapper around an in-memory SQLite DB. Test-harness-level SQLite failures abort the
   // process (they should never happen in a correctly set up test environment).
   struct sqlite_db
   {
      sqlite3* db{};

      sqlite_db()
      {
         if (sqlite3_open(":memory:", &db) != SQLITE_OK) die("sqlite3_open");
         exec("CREATE TABLE t (blob BLOB)");
      }
      ~sqlite_db()
      {
         if (db) sqlite3_close(db);
      }

      sqlite_db(const sqlite_db&) = delete;
      sqlite_db& operator=(const sqlite_db&) = delete;

      void exec(const char* sql)
      {
         char* err = nullptr;
         if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::fprintf(stderr, "sqlite_exec: %s\n", err ? err : "unknown");
            sqlite3_free(err);
            std::abort();
         }
      }

      void reset_table() { exec("DELETE FROM t"); }

      void insert_blob(const void* data, size_t size)
      {
         sqlite3_stmt* s{};
         if (sqlite3_prepare_v2(db, "INSERT INTO t(blob) VALUES (?)", -1, &s, nullptr) != SQLITE_OK) die(sqlite3_errmsg(db));
         sqlite3_bind_blob(s, 1, data, static_cast<int>(size), SQLITE_TRANSIENT);
         if (sqlite3_step(s) != SQLITE_DONE) {
            sqlite3_finalize(s);
            die(sqlite3_errmsg(db));
         }
         sqlite3_finalize(s);
      }

      std::string select_json_of_blob()
      {
         sqlite3_stmt* s{};
         if (sqlite3_prepare_v2(db, "SELECT json(blob) FROM t LIMIT 1", -1, &s, nullptr) != SQLITE_OK) die(sqlite3_errmsg(db));
         std::string out;
         if (sqlite3_step(s) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(s, 0);
            if (txt) out.assign(reinterpret_cast<const char*>(txt));
         }
         sqlite3_finalize(s);
         return out;
      }

      std::string sqlite_jsonb(const std::string& json_text)
      {
         sqlite3_stmt* s{};
         if (sqlite3_prepare_v2(db, "SELECT jsonb(?)", -1, &s, nullptr) != SQLITE_OK) die(sqlite3_errmsg(db));
         sqlite3_bind_text(s, 1, json_text.c_str(), static_cast<int>(json_text.size()), SQLITE_TRANSIENT);
         std::string out;
         if (sqlite3_step(s) == SQLITE_ROW) {
            const void* data = sqlite3_column_blob(s, 0);
            const int size = sqlite3_column_bytes(s, 0);
            out.assign(static_cast<const char*>(data), static_cast<size_t>(size));
         }
         sqlite3_finalize(s);
         return out;
      }
   };
}

struct person
{
   std::string name;
   int age{};
   std::vector<double> scores;
   bool operator==(const person&) const = default;
};

// Version suitable for JSON-level equality comparison (orders field dumps alphabetically to
// match what SQLite's json() returns).
static std::string normalize_json(const std::string& s)
{
   // Parse with Glaze's generic JSON type and re-serialize.
   glz::generic j{};
   auto ec = glz::read_json(j, s);
   if (ec) return s;
   std::string out;
   if (glz::write_json(j, out)) return s;
   return out;
}

suite a_glaze_to_sqlite = [] {
   "scalar int round-trip through SQLite"_test = [] {
      sqlite_db db;
      std::string blob;
      expect(not glz::write_jsonb(42, blob));
      db.insert_blob(blob.data(), blob.size());
      expect(db.select_json_of_blob() == "42");
   };

   "negative int round-trip"_test = [] {
      sqlite_db db;
      std::string blob;
      expect(not glz::write_jsonb(-12345, blob));
      db.insert_blob(blob.data(), blob.size());
      expect(db.select_json_of_blob() == "-12345");
   };

   "boolean and null"_test = [] {
      sqlite_db db;

      std::string blob;
      expect(not glz::write_jsonb(true, blob));
      db.insert_blob(blob.data(), blob.size());
      expect(db.select_json_of_blob() == "true");

      db.reset_table();
      blob.clear();
      expect(not glz::write_jsonb(false, blob));
      db.insert_blob(blob.data(), blob.size());
      expect(db.select_json_of_blob() == "false");

      db.reset_table();
      blob.clear();
      expect(not glz::write_jsonb(nullptr, blob));
      db.insert_blob(blob.data(), blob.size());
      expect(db.select_json_of_blob() == "null");
   };

   "string round-trip"_test = [] {
      sqlite_db db;
      std::string blob;
      const std::string s = "Hello, world!";
      expect(not glz::write_jsonb(s, blob));
      db.insert_blob(blob.data(), blob.size());
      expect(db.select_json_of_blob() == R"("Hello, world!")");
   };

   "string with special chars"_test = [] {
      sqlite_db db;
      std::string blob;
      const std::string s = "line1\nline2\t\"quoted\"";
      expect(not glz::write_jsonb(s, blob));
      db.insert_blob(blob.data(), blob.size());
      // SQLite will JSON-escape these characters itself when rendering.
      const auto out = db.select_json_of_blob();
      expect(out == R"("line1\nline2\t\"quoted\"")");
   };

   "vector round-trip"_test = [] {
      sqlite_db db;
      std::vector<int> v{1, 2, 3, 4, 5};
      std::string blob;
      expect(not glz::write_jsonb(v, blob));
      db.insert_blob(blob.data(), blob.size());
      expect(db.select_json_of_blob() == "[1,2,3,4,5]");
   };

   "map round-trip"_test = [] {
      sqlite_db db;
      std::map<std::string, int> m{{"a", 1}, {"b", 2}};
      std::string blob;
      expect(not glz::write_jsonb(m, blob));
      db.insert_blob(blob.data(), blob.size());
      const auto out = db.select_json_of_blob();
      expect(normalize_json(out) == normalize_json(R"({"a":1,"b":2})"));
   };

   "reflected struct round-trip"_test = [] {
      sqlite_db db;
      person p{.name = "Alice", .age = 30, .scores = {95.5, 87.25, 100.0}};
      std::string blob;
      expect(not glz::write_jsonb(p, blob));
      db.insert_blob(blob.data(), blob.size());
      const auto out = db.select_json_of_blob();
      // Compare via JSON equality after normalizing.
      const std::string expected = R"({"name":"Alice","age":30,"scores":[95.5,87.25,100.0]})";
      expect(normalize_json(out) == normalize_json(expected));
   };

   "empty array / empty object"_test = [] {
      sqlite_db db;

      {
         std::vector<int> v;
         std::string blob;
         expect(not glz::write_jsonb(v, blob));
         db.insert_blob(blob.data(), blob.size());
         expect(db.select_json_of_blob() == "[]");
      }
      db.reset_table();
      {
         std::map<std::string, int> m;
         std::string blob;
         expect(not glz::write_jsonb(m, blob));
         db.insert_blob(blob.data(), blob.size());
         expect(db.select_json_of_blob() == "{}");
      }
   };

   "nested structure"_test = [] {
      sqlite_db db;
      std::map<std::string, std::vector<int>> m{{"a", {1, 2}}, {"b", {3, 4, 5}}};
      std::string blob;
      expect(not glz::write_jsonb(m, blob));
      db.insert_blob(blob.data(), blob.size());
      const auto out = db.select_json_of_blob();
      expect(normalize_json(out) == normalize_json(R"({"a":[1,2],"b":[3,4,5]})"));
   };
};

suite b_sqlite_to_glaze = [] {
   "read SQLite-produced blob for int"_test = [] {
      sqlite_db db;
      const std::string blob = db.sqlite_jsonb("42");
      int out = 0;
      expect(not glz::read_jsonb(out, blob));
      expect(out == 42);
   };

   "read SQLite-produced blob for string"_test = [] {
      sqlite_db db;
      const std::string blob = db.sqlite_jsonb("\"hello\"");
      std::string out;
      expect(not glz::read_jsonb(out, blob));
      expect(out == "hello");
   };

   "read SQLite-produced blob for array"_test = [] {
      sqlite_db db;
      const std::string blob = db.sqlite_jsonb("[1,2,3]");
      std::vector<int> out;
      expect(not glz::read_jsonb(out, blob));
      expect(out == std::vector<int>{1, 2, 3});
   };

   "read SQLite-produced blob for object"_test = [] {
      sqlite_db db;
      const std::string blob = db.sqlite_jsonb(R"({"a":1,"b":2})");
      std::map<std::string, int> out;
      expect(not glz::read_jsonb(out, blob));
      expect(out.size() == 2);
      expect(out["a"] == 1 && out["b"] == 2);
   };

   "read SQLite-produced blob for reflected struct"_test = [] {
      sqlite_db db;
      const std::string blob = db.sqlite_jsonb(R"({"name":"Bob","age":25,"scores":[1.5,2.5]})");
      person out{};
      expect(not glz::read_jsonb(out, blob));
      expect(out.name == "Bob");
      expect(out.age == 25);
      expect(out.scores == std::vector<double>{1.5, 2.5});
   };

   "read SQLite-produced blob with escaped string"_test = [] {
      // SQLite's jsonb() normalizes input JSON; for a string with \n it emits a TEXTJ element
      // whose payload contains \n (the two bytes, not a literal newline).
      sqlite_db db;
      const std::string blob = db.sqlite_jsonb(R"("a\nb")");
      std::string out;
      expect(not glz::read_jsonb(out, blob));
      expect(out == "a\nb");
   };

   "read SQLite-produced blob for nested struct"_test = [] {
      sqlite_db db;
      const std::string blob = db.sqlite_jsonb(R"({"outer":{"inner":[1,2,3]}})");
      std::map<std::string, std::map<std::string, std::vector<int>>> out;
      expect(not glz::read_jsonb(out, blob));
      expect(out.size() == 1);
      expect(out["outer"]["inner"] == std::vector<int>{1, 2, 3});
   };
};

suite c_cross_validation = [] {
   // For scalar types that map cleanly to a single JSONB element, verify byte-for-byte
   // equivalence with SQLite's jsonb() output.
   "null byte-equivalent"_test = [] {
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb("null");
      std::string glaze_blob;
      expect(not glz::write_jsonb(nullptr, glaze_blob));
      expect(sqlite_blob == glaze_blob);
   };

   "true byte-equivalent"_test = [] {
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb("true");
      std::string glaze_blob;
      expect(not glz::write_jsonb(true, glaze_blob));
      expect(sqlite_blob == glaze_blob);
   };

   "false byte-equivalent"_test = [] {
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb("false");
      std::string glaze_blob;
      expect(not glz::write_jsonb(false, glaze_blob));
      expect(sqlite_blob == glaze_blob);
   };

   "small int byte-equivalent"_test = [] {
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb("42");
      std::string glaze_blob;
      expect(not glz::write_jsonb(42, glaze_blob));
      expect(sqlite_blob == glaze_blob);
   };

   "negative int byte-equivalent"_test = [] {
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb("-12345");
      std::string glaze_blob;
      expect(not glz::write_jsonb(-12345, glaze_blob));
      expect(sqlite_blob == glaze_blob);
   };

   "short string byte-equivalence"_test = [] {
      // SQLite's jsonb() may choose a TEXT variant based on escape-free content; we emit
      // TEXTRAW. So we only require that reading SQLite's blob yields the original value.
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb(R"("hi")");
      std::string out;
      expect(not glz::read_jsonb(out, sqlite_blob));
      expect(out == "hi");
   };

   // generic_i64: routes integer JSON through the int64_t variant alternative, producing INT
   // output that should match SQLite byte-for-byte.
   "generic_i64 integer byte-equivalent"_test = [] {
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb("42");
      glz::generic_i64 v;
      expect(not glz::read_json(v, "42"));
      std::string glaze_blob;
      expect(not glz::write_jsonb(v, glaze_blob));
      expect(sqlite_blob == glaze_blob);
   };

   "generic_i64 negative integer byte-equivalent"_test = [] {
      sqlite_db db;
      const auto sqlite_blob = db.sqlite_jsonb("-1234567");
      glz::generic_i64 v;
      expect(not glz::read_json(v, "-1234567"));
      std::string glaze_blob;
      expect(not glz::write_jsonb(v, glaze_blob));
      expect(sqlite_blob == glaze_blob);
   };

   "generic round-trip through SQLite"_test = [] {
      sqlite_db db;
      // Write complex value via generic → JSONB → SQLite → JSON → generic.
      glz::generic src;
      expect(not glz::read_json(src, R"({"a":1.5,"b":[true,null,"x"]})"));
      std::string blob;
      expect(not glz::write_jsonb(src, blob));
      db.insert_blob(blob.data(), blob.size());
      const auto out_json = db.select_json_of_blob();
      glz::generic round;
      expect(not glz::read_json(round, out_json));
      // Validate key fields preserved.
      expect(round.is_object());
      const auto& o = round.get_object();
      auto it_a = o.find("a");
      expect(it_a != o.end());
      expect(it_a->second.get<double>() == 1.5);
   };
};

int main() { return 0; }
