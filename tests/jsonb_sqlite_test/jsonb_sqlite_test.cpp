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

   // Probe whether the linked SQLite build includes the JSON1 extension (json()/jsonb()).
   // macOS ships a system SQLite without JSON1 so the symbols link fine but the functions
   // error at query time. We check once, cache the result, and skip all suites if missing.
   bool sqlite_has_jsonb()
   {
      static const bool value = [] {
         sqlite3* db{};
         if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return false;
         }
         sqlite3_stmt* s{};
         bool ok = false;
         if (sqlite3_prepare_v2(db, "SELECT jsonb('null')", -1, &s, nullptr) == SQLITE_OK) {
            ok = (sqlite3_step(s) == SQLITE_ROW);
            sqlite3_finalize(s);
         }
         sqlite3_close(db);
         if (!ok) {
            std::fprintf(stderr,
                         "jsonb_sqlite_test: linked SQLite (%s) lacks the JSON1 extension — all suites "
                         "skipped\n(this is expected on macOS, which ships SQLite without JSON support).\n",
                         sqlite3_libversion());
         }
         return ok;
      }();
      return value;
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

      // Run `SELECT json_extract(blob, <path>) FROM t LIMIT 1` and return the extracted text.
      // SQLite's json_extract returns JSON text for containers, or the raw value for scalars.
      std::string json_extract(const std::string& path)
      {
         sqlite3_stmt* s{};
         if (sqlite3_prepare_v2(db, "SELECT json_extract(blob, ?) FROM t LIMIT 1", -1, &s, nullptr) != SQLITE_OK) {
            die(sqlite3_errmsg(db));
         }
         sqlite3_bind_text(s, 1, path.c_str(), static_cast<int>(path.size()), SQLITE_TRANSIENT);
         std::string out;
         if (sqlite3_step(s) == SQLITE_ROW) {
            const int col_type = sqlite3_column_type(s, 0);
            if (col_type == SQLITE_NULL) {
               out = "<NULL>"; // distinguish from an empty string value
            }
            else {
               const unsigned char* txt = sqlite3_column_text(s, 0);
               if (txt) out.assign(reinterpret_cast<const char*>(txt));
            }
         }
         sqlite3_finalize(s);
         return out;
      }
   };
}

// Shared types with the jsonb_test target, redefined here to keep this translation unit
// self-contained. Field names must match for json_extract paths to resolve.
struct sqlite_address
{
   std::string street;
   std::string city;
   std::string country;
   std::optional<std::string> postal_code;
};

struct sqlite_user_profile
{
   uint64_t id{};
   std::string email;
   std::string display_name;
   std::optional<sqlite_address> home_address;
   std::vector<std::string> tags;
   std::map<std::string, std::string> preferences;
   bool active = true;
};

struct sqlite_line_item
{
   std::string product_id;
   std::string name;
   int quantity{};
   double unit_price{};
};

struct sqlite_cart
{
   std::string cart_id;
   std::string user_id;
   std::vector<sqlite_line_item> items;
   double total{};
};

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
   if (!sqlite_has_jsonb()) return;
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
   if (!sqlite_has_jsonb()) return;
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
   if (!sqlite_has_jsonb()) return;
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

suite d_path_queries = [] {
   if (!sqlite_has_jsonb()) return;
   // SQLite's json_extract runs JSON Path-style queries directly on our JSONB blob. This
   // validates both that Glaze's output is well-formed AND that SQLite can navigate its
   // nested structure down to specific fields and array indices.

   "extract scalar field from reflected struct"_test = [] {
      sqlite_db db;
      sqlite_user_profile u{};
      u.id = 12345;
      u.email = "alice@example.com";
      u.display_name = "Alice";
      u.tags = {"admin"};

      std::string blob;
      expect(not glz::write_jsonb(u, blob));
      db.insert_blob(blob.data(), blob.size());

      expect(db.json_extract("$.id") == "12345");
      expect(db.json_extract("$.email") == "alice@example.com");
      expect(db.json_extract("$.display_name") == "Alice");
      expect(db.json_extract("$.active") == "1"); // SQLite returns boolean true as 1
   };

   "extract from nested optional struct"_test = [] {
      sqlite_db db;
      sqlite_user_profile u{};
      u.id = 1;
      u.email = "x@y";
      u.display_name = "X";
      u.home_address = sqlite_address{"221B Baker St", "London", "UK", std::string{"NW1 6XE"}};

      std::string blob;
      expect(not glz::write_jsonb(u, blob));
      db.insert_blob(blob.data(), blob.size());

      expect(db.json_extract("$.home_address.street") == "221B Baker St");
      expect(db.json_extract("$.home_address.city") == "London");
      expect(db.json_extract("$.home_address.postal_code") == "NW1 6XE");
   };

   "extract array index"_test = [] {
      sqlite_db db;
      sqlite_user_profile u{};
      u.id = 2;
      u.email = "a@b";
      u.display_name = "A";
      u.tags = {"alpha", "beta", "gamma"};

      std::string blob;
      expect(not glz::write_jsonb(u, blob));
      db.insert_blob(blob.data(), blob.size());

      expect(db.json_extract("$.tags[0]") == "alpha");
      expect(db.json_extract("$.tags[1]") == "beta");
      expect(db.json_extract("$.tags[2]") == "gamma");
   };

   "extract array length via json_array_length"_test = [] {
      sqlite_db db;
      sqlite_user_profile u{};
      u.id = 3;
      u.email = "c@d";
      u.display_name = "C";
      u.tags = {"t1", "t2", "t3", "t4"};
      std::string blob;
      expect(not glz::write_jsonb(u, blob));
      db.insert_blob(blob.data(), blob.size());

      // Direct SQL.
      sqlite3_stmt* s{};
      expect(sqlite3_prepare_v2(db.db, "SELECT json_array_length(blob, '$.tags') FROM t LIMIT 1", -1, &s, nullptr) ==
             SQLITE_OK);
      expect(sqlite3_step(s) == SQLITE_ROW);
      expect(sqlite3_column_int(s, 0) == 4);
      sqlite3_finalize(s);
   };

   "extract from nested map value"_test = [] {
      sqlite_db db;
      sqlite_user_profile u{};
      u.id = 4;
      u.email = "e@f";
      u.display_name = "E";
      u.preferences = {{"theme", "dark"}, {"locale", "en-US"}};

      std::string blob;
      expect(not glz::write_jsonb(u, blob));
      db.insert_blob(blob.data(), blob.size());

      expect(db.json_extract("$.preferences.theme") == "dark");
      expect(db.json_extract("$.preferences.locale") == "en-US");
   };

   "extract from array of reflected structs"_test = [] {
      sqlite_db db;
      sqlite_cart c{};
      c.cart_id = "cart-1";
      c.user_id = "user-42";
      c.items = {{"SKU-A", "Widget", 2, 9.99}, {"SKU-B", "Gadget", 1, 29.95}};
      c.total = 49.93;

      std::string blob;
      expect(not glz::write_jsonb(c, blob));
      db.insert_blob(blob.data(), blob.size());

      expect(db.json_extract("$.cart_id") == "cart-1");
      expect(db.json_extract("$.items[0].product_id") == "SKU-A");
      expect(db.json_extract("$.items[0].quantity") == "2");
      expect(db.json_extract("$.items[1].name") == "Gadget");
      expect(db.json_extract("$.items[1].unit_price") == "29.95");
      expect(db.json_extract("$.total") == "49.93");
   };

   "json_type reports correct types"_test = [] {
      sqlite_db db;
      sqlite_user_profile u{};
      u.id = 7;
      u.email = "g@h";
      u.display_name = "G";
      u.tags = {"t"};
      u.preferences = {{"k", "v"}};

      std::string blob;
      expect(not glz::write_jsonb(u, blob));
      db.insert_blob(blob.data(), blob.size());

      auto type_of = [&](const char* path) {
         sqlite3_stmt* s{};
         sqlite3_prepare_v2(db.db, "SELECT json_type(blob, ?) FROM t LIMIT 1", -1, &s, nullptr);
         sqlite3_bind_text(s, 1, path, -1, SQLITE_TRANSIENT);
         std::string out;
         if (sqlite3_step(s) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(s, 0);
            if (txt) out.assign(reinterpret_cast<const char*>(txt));
         }
         sqlite3_finalize(s);
         return out;
      };

      expect(type_of("$") == "object");
      expect(type_of("$.id") == "integer");
      expect(type_of("$.email") == "text");
      expect(type_of("$.active") == "true");
      expect(type_of("$.tags") == "array");
      expect(type_of("$.preferences") == "object");
   };

   "json_valid accepts our blob"_test = [] {
      // Our writer emits spec-legal but non-canonical JSONB (always-9-byte container
      // headers per the documented one-pass strategy). SQLite's json_valid flag 8
      // requires canonical form; flag 4 accepts any parseable JSONB. We validate with
      // flag 4, which is the correct check for "SQLite can read this".
      sqlite_db db;
      sqlite_cart c{};
      c.cart_id = "c1";
      c.user_id = "u1";
      c.items = {{"P1", "Prod", 1, 1.00}};
      c.total = 1.00;

      std::string blob;
      expect(not glz::write_jsonb(c, blob));
      db.insert_blob(blob.data(), blob.size());

      sqlite3_stmt* s{};
      sqlite3_prepare_v2(db.db, "SELECT json_valid(blob, 4) FROM t LIMIT 1", -1, &s, nullptr);
      expect(sqlite3_step(s) == SQLITE_ROW);
      expect(sqlite3_column_int(s, 0) == 1);
      sqlite3_finalize(s);
   };

   "round-trip through SQLite preserves deep structure"_test = [] {
      sqlite_db db;
      sqlite_user_profile src{};
      src.id = 999;
      src.email = "deep@nested.test";
      src.display_name = "Nested";
      src.home_address = sqlite_address{"1 Way", "Spot", "ZZ", std::nullopt};
      src.tags = {"a", "b", "c"};
      src.preferences = {{"k1", "v1"}, {"k2", "v2"}};

      std::string blob;
      expect(not glz::write_jsonb(src, blob));
      db.insert_blob(blob.data(), blob.size());

      // Re-serialize: SELECT jsonb(json(blob)) — SQLite's canonical re-encoding.
      sqlite3_stmt* s{};
      sqlite3_prepare_v2(db.db, "SELECT jsonb(json(blob)) FROM t LIMIT 1", -1, &s, nullptr);
      expect(sqlite3_step(s) == SQLITE_ROW);
      const void* data = sqlite3_column_blob(s, 0);
      const int size = sqlite3_column_bytes(s, 0);
      std::string canonical(static_cast<const char*>(data), static_cast<size_t>(size));
      sqlite3_finalize(s);

      // Glaze reads SQLite's canonical form back into our struct — every field preserved.
      sqlite_user_profile out{};
      expect(not glz::read_jsonb(out, canonical));
      expect(out.id == src.id);
      expect(out.email == src.email);
      expect(out.display_name == src.display_name);
      expect(out.home_address.has_value());
      expect(out.home_address->street == "1 Way");
      expect(out.tags == src.tags);
      expect(out.preferences == src.preferences);
   };

   "schema-level query with real SQL"_test = [] {
      // A realistic scenario: many rows, each containing JSONB user profiles. Run a SQL
      // query that filters on a nested JSONB field.
      sqlite_db db;
      for (int i = 0; i < 20; ++i) {
         sqlite_user_profile u{};
         u.id = static_cast<uint64_t>(i);
         u.email = "u" + std::to_string(i) + "@test";
         u.display_name = "User" + std::to_string(i);
         u.active = (i % 2 == 0);
         u.tags = {i % 2 == 0 ? "even" : "odd"};
         std::string blob;
         expect(not glz::write_jsonb(u, blob));
         db.insert_blob(blob.data(), blob.size());
      }

      sqlite3_stmt* s{};
      sqlite3_prepare_v2(db.db, "SELECT COUNT(*) FROM t WHERE json_extract(blob, '$.active') = 1", -1, &s, nullptr);
      expect(sqlite3_step(s) == SQLITE_ROW);
      expect(sqlite3_column_int(s, 0) == 10);
      sqlite3_finalize(s);

      sqlite3_prepare_v2(db.db, "SELECT json_extract(blob, '$.display_name') FROM t WHERE json_extract(blob, '$.id') = 7",
                        -1, &s, nullptr);
      expect(sqlite3_step(s) == SQLITE_ROW);
      const unsigned char* name = sqlite3_column_text(s, 0);
      expect(name && std::string(reinterpret_cast<const char*>(name)) == "User7");
      sqlite3_finalize(s);
   };
};

int main() { return 0; }
