// Glaze Library
// For the license information refer to glaze.hpp

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#include "glaze/beve.hpp"
#include "glaze/core/ostream_buffer.hpp"
#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Custom streambuf that simulates slow data arrival by limiting bytes per read
// This mimics network streams or pipes where data arrives in chunks
class slow_stringbuf : public std::stringbuf
{
   size_t max_bytes_per_read_;

 public:
   slow_stringbuf(const std::string& s, size_t max_bytes_per_read)
      : std::stringbuf(s, std::ios_base::in), max_bytes_per_read_(max_bytes_per_read)
   {}

 protected:
   // Override xsgetn to limit how many bytes are returned per read() call
   std::streamsize xsgetn(char* s, std::streamsize n) override
   {
      // Only return up to max_bytes_per_read_ bytes, simulating chunked arrival
      return std::stringbuf::xsgetn(s, std::min(n, static_cast<std::streamsize>(max_bytes_per_read_)));
   }
};

// Thread-safe pipe-like buffer for testing truly async scenarios
class pipe_buffer : public std::streambuf
{
   std::vector<char> buffer_;
   size_t read_pos_ = 0;
   size_t write_pos_ = 0;
   bool closed_ = false;
   mutable std::mutex mutex_;
   std::condition_variable cv_;

 public:
   pipe_buffer() : buffer_(65536) {} // 64KB buffer

   // Writer side: append data to the buffer
   void write(const char* data, size_t len)
   {
      std::lock_guard<std::mutex> lock(mutex_);
      for (size_t i = 0; i < len && write_pos_ < buffer_.size(); ++i) {
         buffer_[write_pos_++] = data[i];
      }
      cv_.notify_one();
   }

   void write(const std::string& s) { write(s.data(), s.size()); }

   // Signal that no more data will be written
   void close()
   {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
      cv_.notify_all();
   }

 protected:
   // Called when the internal buffer needs more data
   int_type underflow() override
   {
      std::unique_lock<std::mutex> lock(mutex_);

      // Wait for data to be available
      cv_.wait(lock, [this] { return read_pos_ < write_pos_ || closed_; });

      if (read_pos_ >= write_pos_) {
         return traits_type::eof();
      }

      // Set up the get area with available data
      char* base = buffer_.data();
      setg(base + read_pos_, base + read_pos_, base + write_pos_);

      return traits_type::to_int_type(*gptr());
   }

   int_type uflow() override
   {
      int_type c = underflow();
      if (c != traits_type::eof()) {
         gbump(1);
         read_pos_++;
      }
      return c;
   }

   std::streamsize xsgetn(char* s, std::streamsize n) override
   {
      std::unique_lock<std::mutex> lock(mutex_);

      // Wait for at least some data
      cv_.wait(lock, [this] { return read_pos_ < write_pos_ || closed_; });

      size_t available = write_pos_ - read_pos_;
      if (available == 0) {
         return 0;
      }

      size_t to_read = std::min(static_cast<size_t>(n), available);
      std::memcpy(s, buffer_.data() + read_pos_, to_read);
      read_pos_ += to_read;

      return static_cast<std::streamsize>(to_read);
   }
};

// Custom streambuf that simulates slow/chunked writes by limiting bytes per write
// This mimics network streams, pipes, or slow storage where writes happen in chunks
class slow_ostringbuf : public std::stringbuf
{
   size_t max_bytes_per_write_;
   size_t total_write_calls_ = 0;

 public:
   explicit slow_ostringbuf(size_t max_bytes_per_write) : std::stringbuf(std::ios_base::out), max_bytes_per_write_(max_bytes_per_write) {}

   // Get the accumulated output
   std::string output() const { return str(); }

   // Get number of write calls made (for verification)
   size_t write_calls() const { return total_write_calls_; }

 protected:
   // Override xsputn to simulate slow writes by writing in chunks
   // but ensuring all data is eventually written (simulating a slow network that
   // accepts data in small pieces)
   std::streamsize xsputn(const char* s, std::streamsize n) override
   {
      std::streamsize total_written = 0;
      while (total_written < n) {
         ++total_write_calls_;
         std::streamsize to_write = std::min(n - total_written, static_cast<std::streamsize>(max_bytes_per_write_));
         auto written = std::stringbuf::xsputn(s + total_written, to_write);
         if (written <= 0) break; // Error case
         total_written += written;
      }
      return total_written;
   }
};

// Test structs
struct Record
{
   int id{};
   std::string name{};

   bool operator==(const Record&) const = default;
};

template <>
struct glz::meta<Record>
{
   using T = Record;
   static constexpr auto value = object("id", &T::id, "name", &T::name);
};

struct NestedObj
{
   int x{};
   std::vector<int> arr{};

   bool operator==(const NestedObj&) const = default;
};

template <>
struct glz::meta<NestedObj>
{
   using T = NestedObj;
   static constexpr auto value = object("x", &T::x, "arr", &T::arr);
};

struct ComplexObj
{
   int id{};
   std::string name{};
   double value{};
   std::vector<int> numbers{};
   std::map<std::string, int> mapping{};
   std::optional<std::string> optional_field{};

   bool operator==(const ComplexObj&) const = default;
};

template <>
struct glz::meta<ComplexObj>
{
   using T = ComplexObj;
   static constexpr auto value = object(
      "id", &T::id,
      "name", &T::name,
      "value", &T::value,
      "numbers", &T::numbers,
      "mapping", &T::mapping,
      "optional_field", &T::optional_field
   );
};

suite istream_buffer_concept_tests = [] {
   "byte_input_stream concept"_test = [] {
      static_assert(glz::byte_input_stream<std::istream>);
      static_assert(glz::byte_input_stream<std::ifstream>);
      static_assert(glz::byte_input_stream<std::istringstream>);
      static_assert(!glz::byte_input_stream<std::wistream>);
      static_assert(!glz::byte_input_stream<std::wifstream>);
   };

   "is_input_streaming concept"_test = [] {
      static_assert(!glz::is_input_streaming<std::string>);
      static_assert(!glz::is_input_streaming<std::vector<char>>);
      static_assert(glz::is_input_streaming<glz::istream_buffer<>>);
      static_assert(glz::is_input_streaming<glz::basic_istream_buffer<std::ifstream>>);
   };

   "buffer_traits for istream_buffer"_test = [] {
      using traits = glz::buffer_traits<glz::istream_buffer<>>;
      static_assert(!traits::is_resizable);
      static_assert(!traits::has_bounded_capacity);
      static_assert(!traits::is_output_streaming);
      static_assert(traits::is_input_streaming);
   };
};

suite istream_buffer_basic_tests = [] {
   "basic_istream_buffer construction"_test = [] {
      std::istringstream iss("test data");
      glz::istream_buffer<> buffer(iss);

      expect(buffer.size() > 0u);
      expect(!buffer.empty());
      expect(!buffer.eof());
   };

   "basic_istream_buffer with closed ifstream"_test = [] {
      std::ifstream file;
      glz::basic_istream_buffer<std::ifstream> buffer(file);
      expect(buffer.eof());
   };

   "istream_buffer consume and bytes_consumed"_test = [] {
      std::istringstream iss("hello world");
      glz::istream_buffer<> buffer(iss);

      expect(buffer.bytes_consumed() == 0u);
      buffer.consume(5);
      expect(buffer.bytes_consumed() == 5u);
      buffer.consume(6);
      expect(buffer.bytes_consumed() == 11u);
   };

   "istream_buffer refill"_test = [] {
      std::istringstream iss("abcdefghij");
      glz::basic_istream_buffer<std::istringstream, 5> buffer(iss);

      expect(buffer.size() == 5u);
      buffer.consume(3);
      expect(buffer.size() == 2u);

      bool refilled = buffer.refill();
      expect(refilled);
      expect(buffer.size() == 5u);
   };
};

suite json_read_streaming_tests = [] {
   "read_json with istream_buffer - simple object"_test = [] {
      std::istringstream iss(R"({"id":42,"name":"test"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 42);
      expect(r.name == "test");
   };

   "read_json with istream_buffer - array of integers"_test = [] {
      std::istringstream iss(R"([1,2,3,4,5,6,7,8,9,10])");
      glz::istream_buffer<> buffer(iss);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec);
      expect(arr.size() == 10u);
      expect(arr[0] == 1);
      expect(arr[9] == 10);
   };

   "read_json with istream_buffer - array of objects"_test = [] {
      std::istringstream iss(R"([{"id":1,"name":"one"},{"id":2,"name":"two"},{"id":3,"name":"three"}])");
      glz::istream_buffer<> buffer(iss);

      std::vector<Record> records;
      auto ec = glz::read_json(records, buffer);

      expect(!ec);
      expect(records.size() == 3u);
      expect(records[0].id == 1);
      expect(records[0].name == "one");
      expect(records[2].id == 3);
      expect(records[2].name == "three");
   };

   "read_json with istream_buffer - nested object"_test = [] {
      std::istringstream iss(R"({"x":10,"arr":[1,2,3,4,5]})");
      glz::istream_buffer<> buffer(iss);

      NestedObj obj;
      auto ec = glz::read_json(obj, buffer);

      expect(!ec);
      expect(obj.x == 10);
      expect(obj.arr.size() == 5u);
      expect(obj.arr[4] == 5);
   };

   "read_json with istream_buffer - complex object"_test = [] {
      std::istringstream iss(R"({
         "id": 123,
         "name": "complex test",
         "value": 3.14159,
         "numbers": [10, 20, 30, 40, 50],
         "mapping": {"a": 1, "b": 2, "c": 3},
         "optional_field": "present"
      })");
      glz::istream_buffer<> buffer(iss);

      ComplexObj obj;
      auto ec = glz::read_json(obj, buffer);

      expect(!ec);
      expect(obj.id == 123);
      expect(obj.name == "complex test");
      expect(obj.value > 3.14 && obj.value < 3.15);
      expect(obj.numbers.size() == 5u);
      expect(obj.mapping.size() == 3u);
      expect(obj.mapping["b"] == 2);
      expect(obj.optional_field.has_value());
      expect(*obj.optional_field == "present");
   };

   "read_json with istream_buffer - deeply nested"_test = [] {
      std::istringstream iss(R"({"x":1,"arr":[{"x":2,"arr":[{"x":3,"arr":[]}]}]})");
      glz::istream_buffer<> buffer(iss);

      // Using glz::generic for arbitrary JSON
      glz::generic json;
      auto ec = glz::read_json(json, buffer);

      expect(!ec);
   };

   "read_json with istream_buffer - string with escapes"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"hello\nworld\t\"quoted\""})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec);
      expect(r.name == "hello\nworld\t\"quoted\"");
   };

   "read_json with istream_buffer - unicode"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"日本語テスト"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec);
      expect(r.name == "日本語テスト");
   };

   "read_json with istream_buffer - whitespace variations"_test = [] {
      std::istringstream iss("  \n\t  {  \"id\"  :  42  ,  \"name\"  :  \"test\"  }  \n\t  ");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec);
      expect(r.id == 42);
      expect(r.name == "test");
   };

   "read_json with istream_buffer - empty array"_test = [] {
      std::istringstream iss(R"([])");
      glz::istream_buffer<> buffer(iss);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec);
      expect(arr.empty());
   };

   "read_json with istream_buffer - empty object"_test = [] {
      std::istringstream iss(R"({})");
      glz::istream_buffer<> buffer(iss);

      std::map<std::string, int> map;
      auto ec = glz::read_json(map, buffer);

      expect(!ec);
      expect(map.empty());
   };

   "read_json with istream_buffer - primitives"_test = [] {
      {
         std::istringstream iss("42");
         glz::istream_buffer<> buffer(iss);
         int val;
         expect(!glz::read_json(val, buffer));
         expect(val == 42);
      }
      {
         std::istringstream iss("3.14159");
         glz::istream_buffer<> buffer(iss);
         double val;
         expect(!glz::read_json(val, buffer));
         expect(val > 3.14 && val < 3.15);
      }
      {
         std::istringstream iss("true");
         glz::istream_buffer<> buffer(iss);
         bool val;
         expect(!glz::read_json(val, buffer));
         expect(val == true);
      }
      {
         std::istringstream iss(R"("hello")");
         glz::istream_buffer<> buffer(iss);
         std::string val;
         expect(!glz::read_json(val, buffer));
         expect(val == "hello");
      }
   };
};

suite json_roundtrip_tests = [] {
   "JSON roundtrip - write to ostream, read from istream"_test = [] {
      ComplexObj original{
         .id = 42,
         .name = "roundtrip test",
         .value = 2.71828,
         .numbers = {1, 2, 3, 4, 5},
         .mapping = {{"key1", 100}, {"key2", 200}},
         .optional_field = "optional value"
      };

      // Write to stringstream using ostream_buffer
      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostringstream> write_buffer(oss);
      auto write_ec = glz::write_json(original, write_buffer);
      expect(!write_ec);

      // Read back using istream_buffer
      std::istringstream iss(oss.str());
      glz::istream_buffer<> read_buffer(iss);
      ComplexObj parsed;
      auto read_ec = glz::read_json(parsed, read_buffer);

      expect(!read_ec);
      expect(parsed == original);
   };

   "JSON roundtrip - array of objects"_test = [] {
      std::vector<Record> original = {
         {1, "first"},
         {2, "second"},
         {3, "third"}
      };

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostringstream> write_buffer(oss);
      auto write_ec = glz::write_json(original, write_buffer);
      expect(!write_ec);

      std::istringstream iss(oss.str());
      glz::istream_buffer<> read_buffer(iss);
      std::vector<Record> parsed;
      auto read_ec = glz::read_json(parsed, read_buffer);

      expect(!read_ec);
      expect(parsed == original);
   };
};

suite json_stream_reader_tests = [] {
   "json_stream_reader NDJSON"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"first"}
{"id":2,"name":"second"}
{"id":3,"name":"third"})");

      glz::json_stream_reader<Record> reader(iss);
      std::vector<Record> records;

      Record r;
      glz::error_ctx ec;
      while (reader.read_next(r, ec)) {
         records.push_back(r);
      }

      expect(!ec);
      expect(records.size() == 3u);
      expect(records[0].id == 1);
      expect(records[2].id == 3);
   };

   "json_stream_reader range-based for"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"a"}
{"id":2,"name":"b"})");

      int count = 0;
      for (auto&& r : glz::json_stream_reader<Record>(iss)) {
         count++;
         expect(r.id == count);
      }
      expect(count == 2);
   };

   "json_stream_reader empty stream"_test = [] {
      std::istringstream iss("");
      glz::json_stream_reader<Record> reader(iss);

      Record r;
      glz::error_ctx ec;
      expect(!reader.read_next(r, ec));
      expect(reader.eof());
   };

   "ndjson_stream alias"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"test"})");

      int count = 0;
      for (auto&& r : glz::ndjson_stream<Record>(iss)) {
         count++;
         expect(r.id == 1);
      }
      expect(count == 1);
   };

   "read_json_stream convenience function"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"a"}
{"id":2,"name":"b"}
{"id":3,"name":"c"})");

      std::vector<Record> records;
      auto ec = glz::read_json_stream(records, iss);

      expect(!ec);
      expect(records.size() == 3u);
   };

   "json_stream_reader error handling"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"valid"}
{"id":invalid})");
      glz::json_stream_reader<Record> reader(iss);

      Record r;
      glz::error_ctx ec;

      expect(reader.read_next(r, ec));
      expect(!ec);
      expect(r.id == 1);

      expect(!reader.read_next(r, ec));
      expect(ec.ec != glz::error_code::none);
   };
};

suite beve_streaming_tests = [] {
   "BEVE roundtrip with streaming buffers"_test = [] {
      ComplexObj original{
         .id = 999,
         .name = "beve test",
         .value = 1.41421,
         .numbers = {10, 20, 30},
         .mapping = {{"x", 1}, {"y", 2}},
         .optional_field = std::nullopt
      };

      // Write BEVE to stringstream
      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostringstream> write_buffer(oss);
      auto write_ec = glz::write_beve(original, write_buffer);
      expect(!write_ec);

      // Read BEVE back - note: BEVE read doesn't have streaming overloads yet
      // so we read from the string directly
      std::string beve_data = oss.str();
      ComplexObj parsed;
      auto read_ec = glz::read_beve(parsed, beve_data);

      expect(!read_ec);
      expect(parsed == original);
   };
};

suite small_buffer_tests = [] {
   "read_json with 256-byte istream_buffer"_test = [] {
      // Use a smaller buffer (256 bytes) - still reasonable for most JSON values
      std::istringstream iss(R"({"id":12345,"name":"this is a longer name to test smaller buffer"})");
      glz::basic_istream_buffer<std::istringstream, 256> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 12345);
      expect(r.name == "this is a longer name to test smaller buffer");
   };

   "read_json with 512-byte buffer - array"_test = [] {
      std::istringstream iss(R"([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20])");
      glz::basic_istream_buffer<std::istringstream, 512> buffer(iss);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec);
      expect(arr.size() == 20u);
      expect(arr[19] == 20);
   };

   "read_json with 2KB buffer - larger data"_test = [] {
      // Generate a larger JSON string (about 1.5KB)
      std::string json = R"([)";
      for (int i = 0; i < 50; ++i) {
         if (i > 0) json += ",";
         json += R"({"id":)" + std::to_string(i) + R"(,"name":"item)" + std::to_string(i) + R"("})";
      }
      json += "]";

      std::istringstream iss(json);
      glz::basic_istream_buffer<std::istringstream, 2048> buffer(iss);

      std::vector<Record> records;
      auto ec = glz::read_json(records, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(records.size() == 50u);
      expect(records[49].id == 49);
   };
};

// Complex nested structures for comprehensive testing
struct Address
{
   std::string street{};
   std::string city{};
   int zip{};

   bool operator==(const Address&) const = default;
};

template <>
struct glz::meta<Address>
{
   using T = Address;
   static constexpr auto value = object("street", &T::street, "city", &T::city, "zip", &T::zip);
};

struct Person
{
   std::string name{};
   int age{};
   Address address{};
   std::vector<std::string> emails{};
   std::map<std::string, std::string> metadata{};

   bool operator==(const Person&) const = default;
};

template <>
struct glz::meta<Person>
{
   using T = Person;
   static constexpr auto value =
      object("name", &T::name, "age", &T::age, "address", &T::address, "emails", &T::emails, "metadata", &T::metadata);
};

struct Department
{
   std::string name{};
   std::vector<Person> employees{};
   std::map<std::string, Person> managers{};
   std::optional<Person> head{};

   bool operator==(const Department&) const = default;
};

template <>
struct glz::meta<Department>
{
   using T = Department;
   static constexpr auto value =
      object("name", &T::name, "employees", &T::employees, "managers", &T::managers, "head", &T::head);
};

struct Company
{
   std::string name{};
   std::vector<Department> departments{};
   std::map<std::string, std::vector<Person>> teams{};
   std::map<std::string, std::map<std::string, int>> nested_maps{};

   bool operator==(const Company&) const = default;
};

template <>
struct glz::meta<Company>
{
   using T = Company;
   static constexpr auto value =
      object("name", &T::name, "departments", &T::departments, "teams", &T::teams, "nested_maps", &T::nested_maps);
};

suite complex_nested_structure_tests = [] {
   "nested struct with address"_test = [] {
      Person original{.name = "Alice",
                      .age = 30,
                      .address = {.street = "123 Main St", .city = "Boston", .zip = 12345},
                      .emails = {"alice@example.com", "alice.work@company.com"},
                      .metadata = {{"role", "engineer"}, {"level", "senior"}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      Person parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed == original);
   };

   "deeply nested - department with employees"_test = [] {
      Department original{
         .name = "Engineering",
         .employees =
            {Person{.name = "Bob",
                    .age = 25,
                    .address = {.street = "456 Oak Ave", .city = "Seattle", .zip = 98101},
                    .emails = {"bob@company.com"},
                    .metadata = {{"team", "backend"}}},
             Person{.name = "Carol",
                    .age = 28,
                    .address = {.street = "789 Pine Rd", .city = "Portland", .zip = 97201},
                    .emails = {"carol@company.com", "carol.personal@email.com"},
                    .metadata = {{"team", "frontend"}, {"remote", "true"}}}},
         .managers = {{"tech_lead",
                       Person{.name = "Dave",
                              .age = 35,
                              .address = {.street = "321 Elm St", .city = "Denver", .zip = 80201},
                              .emails = {"dave@company.com"},
                              .metadata = {{"reports", "5"}}}}},
         .head = Person{.name = "Eve",
                        .age = 45,
                        .address = {.street = "555 Cedar Ln", .city = "Austin", .zip = 78701},
                        .emails = {"eve@company.com"},
                        .metadata = {{"title", "VP Engineering"}}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      Department parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed == original);
   };

   "very deep nesting - company structure"_test = [] {
      Company original{
         .name = "TechCorp",
         .departments = {Department{.name = "Engineering",
                                    .employees = {Person{.name = "Alice",
                                                         .age = 30,
                                                         .address = {.street = "123 Main", .city = "NYC", .zip = 10001},
                                                         .emails = {"alice@tech.com"},
                                                         .metadata = {{"level", "senior"}}}},
                                    .managers = {},
                                    .head = std::nullopt},
                         Department{.name = "Sales",
                                    .employees = {},
                                    .managers = {{"regional",
                                                  Person{.name = "Bob",
                                                         .age = 40,
                                                         .address = {.street = "456 Oak", .city = "LA", .zip = 90001},
                                                         .emails = {"bob@tech.com"},
                                                         .metadata = {}}}},
                                    .head = std::nullopt}},
         .teams = {{"alpha",
                    {Person{.name = "Charlie",
                            .age = 25,
                            .address = {.street = "789 Pine", .city = "SF", .zip = 94102},
                            .emails = {"charlie@tech.com"},
                            .metadata = {}}}},
                   {"beta",
                    {Person{.name = "Diana",
                            .age = 28,
                            .address = {.street = "321 Elm", .city = "Chicago", .zip = 60601},
                            .emails = {"diana@tech.com", "d.smith@email.com"},
                            .metadata = {{"specialty", "ML"}}}}}},
         .nested_maps = {{"budget", {{"q1", 100000}, {"q2", 150000}, {"q3", 120000}}},
                         {"headcount", {{"engineering", 50}, {"sales", 20}, {"hr", 5}}}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      Company parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed == original);
   };

   "vector of maps"_test = [] {
      std::vector<std::map<std::string, int>> original = {
         {{"a", 1}, {"b", 2}}, {{"c", 3}, {"d", 4}, {"e", 5}}, {{"f", 6}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::vector<std::map<std::string, int>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "map of vectors"_test = [] {
      std::map<std::string, std::vector<int>> original = {
         {"primes", {2, 3, 5, 7, 11, 13}}, {"fibonacci", {1, 1, 2, 3, 5, 8, 13}}, {"squares", {1, 4, 9, 16, 25}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::map<std::string, std::vector<int>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "nested maps three levels deep"_test = [] {
      std::map<std::string, std::map<std::string, std::map<std::string, int>>> original = {
         {"level1a", {{"level2a", {{"level3a", 1}, {"level3b", 2}}}, {"level2b", {{"level3c", 3}}}}},
         {"level1b", {{"level2c", {{"level3d", 4}, {"level3e", 5}, {"level3f", 6}}}}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::map<std::string, std::map<std::string, std::map<std::string, int>>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "vector of vectors"_test = [] {
      std::vector<std::vector<std::vector<int>>> original = {
         {{1, 2, 3}, {4, 5}}, {{6}, {7, 8, 9, 10}}, {{11, 12}, {13}, {14, 15, 16}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::vector<std::vector<std::vector<int>>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "array of complex objects"_test = [] {
      std::vector<ComplexObj> original = {
         ComplexObj{.id = 1,
                    .name = "first",
                    .value = 1.5,
                    .numbers = {1, 2, 3},
                    .mapping = {{"a", 10}},
                    .optional_field = "present"},
         ComplexObj{
            .id = 2, .name = "second", .value = 2.5, .numbers = {4, 5}, .mapping = {{"b", 20}, {"c", 30}}, .optional_field = std::nullopt},
         ComplexObj{.id = 3,
                    .name = "third",
                    .value = 3.5,
                    .numbers = {},
                    .mapping = {},
                    .optional_field = "also present"}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::vector<ComplexObj> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "map with complex object values"_test = [] {
      std::map<std::string, ComplexObj> original = {
         {"item1",
          ComplexObj{.id = 100,
                     .name = "complex one",
                     .value = 99.9,
                     .numbers = {10, 20, 30, 40},
                     .mapping = {{"key1", 1}, {"key2", 2}},
                     .optional_field = "has value"}},
         {"item2",
          ComplexObj{.id = 200,
                     .name = "complex two",
                     .value = 88.8,
                     .numbers = {50},
                     .mapping = {},
                     .optional_field = std::nullopt}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::map<std::string, ComplexObj> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "tuple types"_test = [] {
      std::tuple<int, std::string, std::vector<double>> original = {42, "hello", {1.1, 2.2, 3.3}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::tuple<int, std::string, std::vector<double>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "pair type"_test = [] {
      std::pair<std::string, std::map<std::string, int>> original = {"config", {{"width", 800}, {"height", 600}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);
      std::pair<std::string, std::map<std::string, int>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec);
      expect(parsed == original);
   };

   "streaming roundtrip with complex nested data"_test = [] {
      Company original{
         .name = "StreamTest Corp",
         .departments =
            {Department{
                .name = "R&D",
                .employees = {Person{.name = "Researcher",
                                     .age = 35,
                                     .address = {.street = "Lab Lane 1", .city = "Cambridge", .zip = 2139},
                                     .emails = {"research@streamtest.com"},
                                     .metadata = {{"publications", "15"}, {"patents", "3"}}}},
                .managers = {},
                .head = Person{.name = "Chief Scientist",
                               .age = 50,
                               .address = {.street = "Innovation Blvd", .city = "Boston", .zip = 2101},
                               .emails = {"chief@streamtest.com"},
                               .metadata = {}}}},
         .teams = {},
         .nested_maps = {{"metrics", {{"accuracy", 95}, {"precision", 92}}}}};

      // Write using ostream_buffer
      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostringstream> write_buffer(oss);
      auto wec = glz::write_json(original, write_buffer);
      expect(!wec);

      // Read using istream_buffer
      std::istringstream iss(oss.str());
      glz::istream_buffer<> read_buffer(iss);
      Company parsed;
      auto rec = glz::read_json(parsed, read_buffer);

      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed == original);
   };
};

// Tests for slow/chunked data arrival (simulating network streams, pipes, etc.)
suite slow_streaming_tests = [] {
   "slow stream - simple object with 8 bytes per read"_test = [] {
      std::string json = R"({"id":42,"name":"slow streaming test"})";
      slow_stringbuf sbuf(json, 8); // Only 8 bytes per read() call
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 42);
      expect(r.name == "slow streaming test");
   };

   "slow stream - simple object with 4 bytes per read"_test = [] {
      std::string json = R"({"id":123,"name":"very slow"})";
      slow_stringbuf sbuf(json, 4); // Only 4 bytes per read() call
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 32> buffer(slow_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 123);
      expect(r.name == "very slow");
   };

   "slow stream - array with 16 bytes per read"_test = [] {
      std::string json = R"([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15])";
      slow_stringbuf sbuf(json, 16);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);
      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(arr.size() == 15u);
      expect(arr[14] == 15);
   };

   "slow stream - nested object with 10 bytes per read"_test = [] {
      std::string json = R"({"x":100,"arr":[1,2,3,4,5,6,7,8,9,10]})";
      slow_stringbuf sbuf(json, 10);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);
      NestedObj obj;
      auto ec = glz::read_json(obj, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(obj.x == 100);
      expect(obj.arr.size() == 10u);
   };

   "slow stream - complex object with 12 bytes per read"_test = [] {
      std::string json = R"({
         "id": 999,
         "name": "complex slow test",
         "value": 3.14159,
         "numbers": [10, 20, 30, 40, 50],
         "mapping": {"alpha": 1, "beta": 2, "gamma": 3},
         "optional_field": "present"
      })";
      slow_stringbuf sbuf(json, 12);
      std::istream slow_stream(&sbuf);

      // Buffer must be larger than JSON (~210 bytes with whitespace)
      glz::basic_istream_buffer<std::istream, 256> buffer(slow_stream);
      ComplexObj obj;
      auto ec = glz::read_json(obj, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(obj.id == 999);
      expect(obj.name == "complex slow test");
      expect(obj.numbers.size() == 5u);
      expect(obj.mapping.size() == 3u);
      expect(obj.optional_field.has_value());
   };

   "slow stream - array of objects with 20 bytes per read"_test = [] {
      std::string json = R"([
         {"id":1,"name":"first"},
         {"id":2,"name":"second"},
         {"id":3,"name":"third"},
         {"id":4,"name":"fourth"},
         {"id":5,"name":"fifth"}
      ])";
      slow_stringbuf sbuf(json, 20);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 256> buffer(slow_stream);
      std::vector<Record> records;
      auto ec = glz::read_json(records, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(records.size() == 5u);
      expect(records[4].id == 5);
      expect(records[4].name == "fifth");
   };

   "slow stream - deeply nested with 15 bytes per read"_test = [] {
      Department original{
         .name = "SlowDept",
         .employees = {Person{.name = "SlowWorker",
                              .age = 30,
                              .address = {.street = "123 Slow St", .city = "SlowCity", .zip = 12345},
                              .emails = {"slow@test.com"},
                              .metadata = {{"speed", "slow"}}}},
         .managers = {},
         .head = std::nullopt};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      slow_stringbuf sbuf(json, 15);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 256> buffer(slow_stream);
      Department parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed == original);
   };

   "slow stream - map of vectors with 25 bytes per read"_test = [] {
      std::map<std::string, std::vector<int>> original = {
         {"primes", {2, 3, 5, 7, 11, 13, 17, 19}},
         {"evens", {2, 4, 6, 8, 10, 12, 14, 16}},
         {"odds", {1, 3, 5, 7, 9, 11, 13, 15}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      slow_stringbuf sbuf(json, 25);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 256> buffer(slow_stream);
      std::map<std::string, std::vector<int>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed == original);
   };

   "slow stream - extremely slow (1 byte per read)"_test = [] {
      std::string json = R"({"id":7,"name":"byte by byte"})";
      slow_stringbuf sbuf(json, 1); // 1 byte at a time!
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 7);
      expect(r.name == "byte by byte");
   };

   "slow stream - unicode with 6 bytes per read"_test = [] {
      std::string json = R"({"id":1,"name":"日本語テスト"})";
      slow_stringbuf sbuf(json, 6); // May split UTF-8 sequences
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.name == "日本語テスト");
   };

   "slow stream - string with escapes and 5 bytes per read"_test = [] {
      std::string json = R"({"id":1,"name":"hello\nworld\t\"quoted\""})";
      slow_stringbuf sbuf(json, 5);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.name == "hello\nworld\t\"quoted\"");
   };
};

// Tests for async/threaded streaming where data arrives with delays
suite async_streaming_tests = [] {
   "async pipe - data arrives in chunks with delays"_test = [] {
      pipe_buffer pbuf;
      std::istream pipe_stream(&pbuf);

      // Writer thread: sends JSON in chunks with delays
      std::thread writer([&pbuf] {
         std::string json = R"({"id":42,"name":"async test"})";

         // Send in small chunks with delays
         size_t pos = 0;
         size_t chunk_size = 5;
         while (pos < json.size()) {
            size_t len = std::min(chunk_size, json.size() - pos);
            pbuf.write(json.data() + pos, len);
            pos += len;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         pbuf.close();
      });

      // Reader: parse using istream_buffer
      glz::basic_istream_buffer<std::istream, 64> buffer(pipe_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      writer.join();

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 42);
      expect(r.name == "async test");
   };

   "async pipe - complex object with slow arrival"_test = [] {
      pipe_buffer pbuf;
      std::istream pipe_stream(&pbuf);

      std::thread writer([&pbuf] {
         ComplexObj original{.id = 777,
                             .name = "async complex",
                             .value = 2.71828,
                             .numbers = {100, 200, 300},
                             .mapping = {{"key1", 10}, {"key2", 20}},
                             .optional_field = "async value"};

         std::string json;
         (void)glz::write_json(original, json);

         // Send in variable-sized chunks
         size_t pos = 0;
         while (pos < json.size()) {
            size_t len = std::min(size_t(7 + (pos % 5)), json.size() - pos); // Varying chunk sizes
            pbuf.write(json.data() + pos, len);
            pos += len;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
         }
         pbuf.close();
      });

      glz::basic_istream_buffer<std::istream, 128> buffer(pipe_stream);
      ComplexObj parsed;
      auto ec = glz::read_json(parsed, buffer);

      writer.join();

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed.id == 777);
      expect(parsed.name == "async complex");
      expect(parsed.numbers.size() == 3u);
   };

   "async pipe - array with staggered arrival"_test = [] {
      pipe_buffer pbuf;
      std::istream pipe_stream(&pbuf);

      std::thread writer([&pbuf] {
         std::vector<Record> original = {
            {1, "first"}, {2, "second"}, {3, "third"}, {4, "fourth"}, {5, "fifth"}};

         std::string json;
         (void)glz::write_json(original, json);

         // Send with longer delays between chunks
         size_t pos = 0;
         while (pos < json.size()) {
            size_t len = std::min(size_t(10), json.size() - pos);
            pbuf.write(json.data() + pos, len);
            pos += len;
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
         }
         pbuf.close();
      });

      glz::basic_istream_buffer<std::istream, 256> buffer(pipe_stream);
      std::vector<Record> parsed;
      auto ec = glz::read_json(parsed, buffer);

      writer.join();

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed.size() == 5u);
      expect(parsed[4].name == "fifth");
   };

   "async pipe - deeply nested with slow arrival"_test = [] {
      pipe_buffer pbuf;
      std::istream pipe_stream(&pbuf);

      std::thread writer([&pbuf] {
         Company original{.name = "AsyncCorp",
                          .departments = {Department{.name = "AsyncDept",
                                                     .employees = {Person{.name = "AsyncWorker",
                                                                          .age = 25,
                                                                          .address = {.street = "Async St", .city = "AsyncCity", .zip = 11111},
                                                                          .emails = {"async@corp.com"},
                                                                          .metadata = {{"async", "true"}}}},
                                                     .managers = {},
                                                     .head = std::nullopt}},
                          .teams = {},
                          .nested_maps = {{"metrics", {{"latency", 50}, {"throughput", 1000}}}}};

         std::string json;
         (void)glz::write_json(original, json);

         // Send byte by byte with tiny delays (stress test)
         for (size_t i = 0; i < json.size(); ++i) {
            pbuf.write(json.data() + i, 1);
            if (i % 10 == 0) {
               std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
         }
         pbuf.close();
      });

      glz::basic_istream_buffer<std::istream, 512> buffer(pipe_stream);
      Company parsed;
      auto ec = glz::read_json(parsed, buffer);

      writer.join();

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed.name == "AsyncCorp");
      expect(parsed.departments.size() == 1u);
      expect(parsed.departments[0].employees[0].name == "AsyncWorker");
   };
};

suite concrete_stream_type_tests = [] {
   "istream_buffer with concrete ifstream type"_test = [] {
      std::ifstream file;
      using BufferType = glz::basic_istream_buffer<std::ifstream>;

      static_assert(glz::is_input_streaming<BufferType>);
      static_assert(glz::byte_input_stream<std::ifstream>);
   };

   "istream_buffer with concrete istringstream type"_test = [] {
      using BufferType = glz::basic_istream_buffer<std::istringstream>;

      static_assert(glz::is_input_streaming<BufferType>);
      static_assert(glz::byte_input_stream<std::istringstream>);
   };
};

// Tests for incremental streaming where buffer is smaller than JSON input
// This exercises the refill points added inside array/object parsing loops
suite incremental_streaming_tests = [] {
   "array larger than buffer - 100 ints with 64-byte buffer"_test = [] {
      // Generate ~290 bytes of JSON, use 64-byte buffer
      std::string json = "[";
      for (int i = 0; i < 100; ++i) {
         if (i > 0) json += ",";
         json += std::to_string(i);
      }
      json += "]";
      expect(json.size() > 64u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 16);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(arr.size() == 100u);
      expect(arr[99] == 99);
   };

   "array larger than buffer - 50 ints with 32-byte buffer (stress)"_test = [] {
      std::string json = "[";
      for (int i = 0; i < 50; ++i) {
         if (i > 0) json += ",";
         json += std::to_string(i * 10);
      }
      json += "]";
      expect(json.size() > 32u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 8);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 32> buffer(slow_stream);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(arr.size() == 50u);
      expect(arr[49] == 490);
   };

   "object larger than buffer - map with 64-byte buffer"_test = [] {
      std::string json = R"({"alpha":1,"beta":2,"gamma":3,"delta":4,"epsilon":5,"zeta":6,"eta":7,"theta":8})";
      expect(json.size() > 64u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 12);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);

      std::map<std::string, int> m;
      auto ec = glz::read_json(m, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(m.size() == 8u);
      expect(m["alpha"] == 1);
      expect(m["theta"] == 8);
   };

   "object larger than buffer - map<string,string> with 64-byte buffer"_test = [] {
      std::map<std::string, std::string> original = {
         {"first_key_here", "first_value_here"},
         {"second_key_here", "second_value_here"},
         {"third_key_here", "third_value_here"}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);
      expect(json.size() > 64u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 16);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);

      std::map<std::string, std::string> parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "array of objects larger than buffer"_test = [] {
      std::string json = "[";
      for (int i = 0; i < 20; ++i) {
         if (i > 0) json += ",";
         json += R"({"id":)" + std::to_string(i) + R"(,"name":"item)" + std::to_string(i) + R"("})";
      }
      json += "]";
      expect(json.size() > 128u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 32);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 128> buffer(slow_stream);

      std::vector<Record> records;
      auto ec = glz::read_json(records, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(records.size() == 20u);
      expect(records[19].id == 19);
   };

   "nested vectors larger than buffer"_test = [] {
      std::vector<std::vector<int>> original = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}, {13, 14, 15}, {16, 17, 18}, {19, 20, 21}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      slow_stringbuf sbuf(json, 10);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);

      std::vector<std::vector<int>> parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "vector of maps larger than buffer"_test = [] {
      std::vector<std::map<std::string, int>> original = {
         {{"a", 1}, {"b", 2}}, {{"c", 3}, {"d", 4}}, {{"e", 5}, {"f", 6}}, {{"g", 7}, {"h", 8}}, {{"i", 9}, {"j", 10}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);
      expect(json.size() > 64u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 16);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);

      std::vector<std::map<std::string, int>> parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "map of vectors larger than buffer"_test = [] {
      std::map<std::string, std::vector<int>> original = {{"first", {1, 2, 3}}, {"second", {4, 5, 6}}, {"third", {7, 8, 9}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      slow_stringbuf sbuf(json, 16);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 64> buffer(slow_stream);

      std::map<std::string, std::vector<int>> parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "complex object larger than buffer"_test = [] {
      ComplexObj original{.id = 12345,
                          .name = "this is a longer name for testing streaming",
                          .value = 3.14159265359,
                          .numbers = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
                          .mapping = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}, {"delta", 4}},
                          .optional_field = "optional value that adds more bytes"};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);
      expect(json.size() > 128u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 20);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 128> buffer(slow_stream);

      ComplexObj parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "deeply nested structure larger than buffer"_test = [] {
      Department original{
         .name = "Engineering Department with Long Name",
         .employees = {Person{.name = "Employee One",
                              .age = 30,
                              .address = {.street = "123 Long Street Name Ave", .city = "San Francisco", .zip = 94102},
                              .emails = {"employee1@company.com", "personal1@email.com"},
                              .metadata = {{"role", "engineer"}, {"level", "senior"}}},
                       Person{.name = "Employee Two",
                              .age = 35,
                              .address = {.street = "456 Another Street Blvd", .city = "Los Angeles", .zip = 90001},
                              .emails = {"employee2@company.com"},
                              .metadata = {{"role", "manager"}}}},
         .managers = {{"lead",
                       Person{.name = "Tech Lead",
                              .age = 40,
                              .address = {.street = "789 Manager Lane", .city = "Seattle", .zip = 98101},
                              .emails = {"lead@company.com"},
                              .metadata = {}}}},
         .head = Person{.name = "Department Head",
                        .age = 50,
                        .address = {.street = "Executive Suite", .city = "New York", .zip = 10001},
                        .emails = {"head@company.com"},
                        .metadata = {{"title", "VP"}}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);
      expect(json.size() > 512u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 50);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 256> buffer(slow_stream);

      Department parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "very small buffer (16 bytes) with array"_test = [] {
      std::string json = "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]";
      expect(json.size() > 16u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 4);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 16> buffer(slow_stream);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(arr.size() == 20u);
      expect(arr[19] == 20);
   };

   "very small buffer (16 bytes) with map"_test = [] {
      std::string json = R"({"a":1,"b":2,"c":3})";
      expect(json.size() > 16u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 4);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 16> buffer(slow_stream);

      std::map<std::string, int> m;
      auto ec = glz::read_json(m, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(m.size() == 3u);
      expect(m["a"] == 1);
      expect(m["c"] == 3);
   };

   "array of strings larger than buffer"_test = [] {
      std::vector<std::string> original = {"this is a fairly long string that takes up space",
                                           "another long string with lots of characters in it",
                                           "yet another string to make the array larger than buffer",
                                           "and one more for good measure with extra padding"};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);
      expect(json.size() > 128u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 20);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 128> buffer(slow_stream);

      std::vector<std::string> parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "incremental streaming roundtrip - large company structure"_test = [] {
      Company original{
         .name = "Large Corporation Inc",
         .departments =
            {Department{.name = "Engineering",
                        .employees = {Person{.name = "Engineer 1",
                                             .age = 28,
                                             .address = {.street = "100 Tech Blvd", .city = "Palo Alto", .zip = 94301},
                                             .emails = {"eng1@corp.com"},
                                             .metadata = {{"team", "backend"}}},
                                      Person{.name = "Engineer 2",
                                             .age = 32,
                                             .address = {.street = "200 Code Ave", .city = "Mountain View", .zip = 94043},
                                             .emails = {"eng2@corp.com", "eng2.personal@gmail.com"},
                                             .metadata = {{"team", "frontend"}, {"remote", "yes"}}}},
                        .managers = {},
                        .head = std::nullopt},
             Department{.name = "Sales",
                        .employees = {},
                        .managers = {{"regional",
                                      Person{.name = "Sales Manager",
                                             .age = 45,
                                             .address = {.street = "300 Commerce St", .city = "Chicago", .zip = 60601},
                                             .emails = {"sales@corp.com"},
                                             .metadata = {}}}},
                        .head = std::nullopt}},
         .teams = {{"alpha", {Person{.name = "Alpha Lead", .age = 35, .address = {.street = "A St", .city = "Austin", .zip = 78701}, .emails = {"alpha@corp.com"}, .metadata = {}}}},
                   {"beta", {Person{.name = "Beta Lead", .age = 33, .address = {.street = "B St", .city = "Denver", .zip = 80201}, .emails = {"beta@corp.com"}, .metadata = {}}}}},
         .nested_maps = {{"budget", {{"q1", 1000000}, {"q2", 1500000}, {"q3", 1200000}, {"q4", 1800000}}},
                         {"headcount", {{"engineering", 150}, {"sales", 75}, {"marketing", 50}, {"hr", 20}}}}};

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);
      expect(json.size() > 512u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 64);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 256> buffer(slow_stream);

      Company parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };
};

// Tests for slow streaming buffer writes
// These test the ostream_buffer with a slow/chunked output stream
suite slow_streaming_write_tests = [] {
   "slow write - simple object with 8 bytes per write"_test = [] {
      Record original{.id = 42, .name = "slow write test"};

      slow_ostringbuf sbuf(8);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(slow_stream.good());

      // Verify by parsing back
      Record parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - simple object with 4 bytes per write"_test = [] {
      Record original{.id = 123, .name = "very slow write"};

      slow_ostringbuf sbuf(4);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 32> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      Record parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - array of integers with 16 bytes per write"_test = [] {
      std::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

      slow_ostringbuf sbuf(16);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::vector<int> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - nested object with 10 bytes per write"_test = [] {
      NestedObj original{.x = 100, .arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};

      slow_ostringbuf sbuf(10);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      NestedObj parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - complex object with 12 bytes per write"_test = [] {
      ComplexObj original{.id = 999,
                          .name = "complex slow write test",
                          .value = 3.14159,
                          .numbers = {10, 20, 30, 40, 50},
                          .mapping = {{"alpha", 1}, {"beta", 2}, {"gamma", 3}},
                          .optional_field = "present"};

      slow_ostringbuf sbuf(12);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 128> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      ComplexObj parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - array of objects with 20 bytes per write"_test = [] {
      std::vector<Record> original = {{1, "first"}, {2, "second"}, {3, "third"}, {4, "fourth"}, {5, "fifth"}};

      slow_ostringbuf sbuf(20);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 128> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::vector<Record> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - deeply nested with 15 bytes per write"_test = [] {
      Department original{.name = "SlowWriteDept",
                          .employees = {Person{.name = "SlowWriter",
                                               .age = 30,
                                               .address = {.street = "123 Slow St", .city = "SlowCity", .zip = 12345},
                                               .emails = {"slow@test.com"},
                                               .metadata = {{"speed", "slow"}}}},
                          .managers = {},
                          .head = std::nullopt};

      slow_ostringbuf sbuf(15);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 256> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      Department parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - map of vectors with 25 bytes per write"_test = [] {
      std::map<std::string, std::vector<int>> original = {
         {"primes", {2, 3, 5, 7, 11, 13, 17, 19}}, {"evens", {2, 4, 6, 8, 10, 12, 14, 16}}, {"odds", {1, 3, 5, 7, 9, 11, 13, 15}}};

      slow_ostringbuf sbuf(25);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 256> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::map<std::string, std::vector<int>> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - extremely slow (1 byte per write)"_test = [] {
      Record original{.id = 7, .name = "byte by byte write"};

      slow_ostringbuf sbuf(1);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      Record parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - unicode with 6 bytes per write"_test = [] {
      Record original{.id = 1, .name = "日本語テスト"};

      slow_ostringbuf sbuf(6);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      Record parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - string with escapes with 5 bytes per write"_test = [] {
      Record original{.id = 1, .name = "hello\nworld\t\"quoted\""};

      slow_ostringbuf sbuf(5);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      Record parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - large array with small buffer and slow writes"_test = [] {
      std::vector<int> original(100);
      for (int i = 0; i < 100; ++i)
         original[i] = i;

      slow_ostringbuf sbuf(8);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 32> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::vector<int> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - large map with small buffer and slow writes"_test = [] {
      std::map<std::string, int> original;
      for (int i = 0; i < 20; ++i) {
         original["key_" + std::to_string(i)] = i * 10;
      }

      slow_ostringbuf sbuf(10);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::map<std::string, int> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - vector of maps with 16 bytes per write"_test = [] {
      std::vector<std::map<std::string, int>> original = {
         {{"a", 1}, {"b", 2}}, {{"c", 3}, {"d", 4}}, {{"e", 5}, {"f", 6}}, {{"g", 7}, {"h", 8}}, {{"i", 9}, {"j", 10}}};

      slow_ostringbuf sbuf(16);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 64> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::vector<std::map<std::string, int>> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - nested vectors with 12 bytes per write"_test = [] {
      std::vector<std::vector<std::vector<int>>> original = {
         {{1, 2, 3}, {4, 5}}, {{6}, {7, 8, 9, 10}}, {{11, 12}, {13}, {14, 15, 16}}};

      slow_ostringbuf sbuf(12);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 128> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::vector<std::vector<std::vector<int>>> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - company structure with 50 bytes per write"_test = [] {
      Company original{.name = "SlowWriteCorp",
                       .departments = {Department{.name = "Engineering",
                                                  .employees = {Person{.name = "Engineer",
                                                                       .age = 30,
                                                                       .address = {.street = "123 Tech St", .city = "TechCity", .zip = 12345},
                                                                       .emails = {"eng@corp.com"},
                                                                       .metadata = {{"role", "dev"}}}},
                                                  .managers = {},
                                                  .head = std::nullopt}},
                       .teams = {{"alpha", {Person{.name = "Lead", .age = 35, .address = {.street = "A St", .city = "Austin", .zip = 78701}, .emails = {"lead@corp.com"}, .metadata = {}}}}},
                       .nested_maps = {{"budget", {{"q1", 100000}, {"q2", 150000}}}}};

      slow_ostringbuf sbuf(50);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 256> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      Company parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - verify multiple write calls happen"_test = [] {
      // Generate enough data to require multiple flushes
      std::vector<int> original(500);
      for (int i = 0; i < 500; ++i)
         original[i] = i;

      slow_ostringbuf sbuf(64);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 128> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(sbuf.write_calls() > 1u) << "Expected multiple write calls, got " << sbuf.write_calls();

      std::vector<int> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write roundtrip - write slow, read slow"_test = [] {
      ComplexObj original{.id = 12345,
                          .name = "full roundtrip test with slow streaming",
                          .value = 2.71828,
                          .numbers = {100, 200, 300, 400, 500},
                          .mapping = {{"key1", 111}, {"key2", 222}, {"key3", 333}},
                          .optional_field = "roundtrip value"};

      // Slow write
      slow_ostringbuf wbuf(10);
      std::ostream slow_ostream(&wbuf);
      glz::basic_ostream_buffer<std::ostream, 64> write_buffer(slow_ostream);

      auto wec = glz::write_json(original, write_buffer);
      expect(!wec) << "Write error: " << int(wec.ec);

      // Slow read
      std::string json = wbuf.output();
      slow_stringbuf rbuf(json, 10);
      std::istream slow_istream(&rbuf);
      glz::basic_istream_buffer<std::istream, 64> read_buffer(slow_istream);

      ComplexObj parsed;
      auto rec = glz::read_json(parsed, read_buffer);

      expect(!rec) << "Read error: " << int(rec.ec);
      expect(parsed == original);
   };

   "slow write BEVE format with 20 bytes per write"_test = [] {
      ComplexObj original{.id = 555,
                          .name = "beve slow write",
                          .value = 1.41421,
                          .numbers = {10, 20, 30},
                          .mapping = {{"x", 1}, {"y", 2}},
                          .optional_field = std::nullopt};

      slow_ostringbuf sbuf(20);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 128> buffer(slow_stream);

      auto ec = glz::write_beve(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      // Read back (BEVE read doesn't have streaming overloads yet)
      ComplexObj parsed;
      auto rec = glz::read_beve(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };
};

int main() { return 0; }
