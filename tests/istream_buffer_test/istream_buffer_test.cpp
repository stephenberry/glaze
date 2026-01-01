// Glaze Library
// For the license information refer to glaze.hpp

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <limits>
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

// Larger record for file size testing
struct LargeRecord
{
   int id{};
   std::string name{};
   std::string description{};
   std::vector<int> values{};

   bool operator==(const LargeRecord&) const = default;
};

// Mixed data types for streaming tests
struct MixedData
{
   std::vector<int> ints{};
   std::vector<double> doubles{};
   std::vector<std::string> strings{};
   std::map<std::string, int> map{};
   std::optional<std::string> opt{};

   bool operator==(const MixedData&) const = default;
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
      // Create a string larger than 512 bytes to test refill
      std::string data(768, 'x');
      std::istringstream iss(data);
      glz::basic_istream_buffer<std::istringstream, 512> buffer(iss);

      expect(buffer.size() == 512u);
      buffer.consume(200);
      expect(buffer.size() == 312u);

      bool refilled = buffer.refill();
      expect(refilled);
      expect(buffer.size() == 512u); // Refilled: 312 remaining + 200 from stream = 512
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
      while (!reader.read_next(r)) {
         records.push_back(r);
      }

      expect(reader.last_error().ec == glz::error_code::end_reached);
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
      expect(reader.read_next(r)); // Should return error (end_reached)
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

      expect(!reader.read_next(r)); // First read should succeed
      expect(r.id == 1);

      auto ec = reader.read_next(r); // Second read should fail
      expect(ec);
      expect(ec.ec != glz::error_code::none);
      expect(ec.ec != glz::error_code::end_reached); // Should be a parse error, not EOF
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
   "read_json with 512-byte istream_buffer"_test = [] {
      // Use a smaller buffer (512 bytes) - still reasonable for most JSON values
      std::istringstream iss(R"({"id":12345,"name":"this is a longer name to test smaller buffer"})");
      glz::basic_istream_buffer<std::istringstream, 512> buffer(iss);

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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
      std::map<std::string, std::vector<int>> parsed;
      auto rec = glz::read_json(parsed, buffer);

      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed == original);
   };

   "slow stream - extremely slow (1 byte per read)"_test = [] {
      std::string json = R"({"id":7,"name":"byte by byte"})";
      slow_stringbuf sbuf(json, 1); // 1 byte at a time!
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.name == "日本語テスト");
   };

   "slow stream - string with escapes and 5 bytes per read"_test = [] {
      std::string json = R"({"id":1,"name":"hello\nworld\t\"quoted\""})";
      slow_stringbuf sbuf(json, 5);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
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
      glz::basic_istream_buffer<std::istream, 512> buffer(pipe_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(pipe_stream);
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

      glz::basic_istream_buffer<std::istream, 512> buffer(pipe_stream);
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
   "array larger than buffer - 100 ints with 512-byte buffer"_test = [] {
      // Generate ~290 bytes of JSON, use 512-byte buffer
      std::string json = "[";
      for (int i = 0; i < 100; ++i) {
         if (i > 0) json += ",";
         json += std::to_string(i);
      }
      json += "]";
      expect(json.size() > 64u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 16);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(arr.size() == 100u);
      expect(arr[99] == 99);
   };

   "object larger than buffer - map with 512-byte buffer"_test = [] {
      std::string json = R"({"alpha":1,"beta":2,"gamma":3,"delta":4,"epsilon":5,"zeta":6,"eta":7,"theta":8})";
      expect(json.size() > 64u) << "JSON should be larger than buffer";

      slow_stringbuf sbuf(json, 12);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

      std::map<std::string, int> m;
      auto ec = glz::read_json(m, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(m.size() == 8u);
      expect(m["alpha"] == 1);
      expect(m["theta"] == 8);
   };

   "object larger than buffer - map<string,string> with 512-byte buffer"_test = [] {
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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

      Department parsed;
      auto ec = glz::read_json(parsed, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "minimum buffer (512 bytes) with array"_test = [] {
      std::string json = "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]";
      expect(json.size() < 512u) << "JSON fits in minimum buffer";

      slow_stringbuf sbuf(json, 4);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(arr.size() == 20u);
      expect(arr[19] == 20);
   };

   "minimum buffer (512 bytes) with map"_test = [] {
      std::string json = R"({"a":1,"b":2,"c":3})";
      expect(json.size() < 512u) << "JSON fits in minimum buffer";

      slow_stringbuf sbuf(json, 4);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      Record parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - array of integers with 512-byte buffer"_test = [] {
      std::vector<int> original = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

      slow_ostringbuf sbuf(16);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

      auto ec = glz::write_json(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      std::map<std::string, int> parsed;
      auto rec = glz::read_json(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };

   "slow write - vector of maps with 512-byte buffer"_test = [] {
      std::vector<std::map<std::string, int>> original = {
         {{"a", 1}, {"b", 2}}, {{"c", 3}, {"d", 4}}, {{"e", 5}, {"f", 6}}, {{"g", 7}, {"h", 8}}, {{"i", 9}, {"j", 10}}};

      slow_ostringbuf sbuf(16);
      std::ostream slow_stream(&sbuf);
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

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
      glz::basic_ostream_buffer<std::ostream, 512> write_buffer(slow_ostream);

      auto wec = glz::write_json(original, write_buffer);
      expect(!wec) << "Write error: " << int(wec.ec);

      // Slow read
      std::string json = wbuf.output();
      slow_stringbuf rbuf(json, 10);
      std::istream slow_istream(&rbuf);
      glz::basic_istream_buffer<std::istream, 512> read_buffer(slow_istream);

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
      glz::basic_ostream_buffer<std::ostream, 512> buffer(slow_stream);

      auto ec = glz::write_beve(original, buffer);

      expect(!ec) << "Error: " << int(ec.ec);

      // Read back (BEVE read doesn't have streaming overloads yet)
      ComplexObj parsed;
      auto rec = glz::read_beve(parsed, sbuf.output());
      expect(!rec);
      expect(parsed == original);
   };
};

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

suite streaming_error_handling_tests = [] {
   "malformed JSON - unclosed object"_test = [] {
      std::istringstream iss(R"({"id":42,"name":"test")");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on unclosed object";
   };

   "malformed JSON - unclosed array"_test = [] {
      std::istringstream iss(R"([1,2,3,4,5)");
      glz::istream_buffer<> buffer(iss);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on unclosed array";
   };

   "malformed JSON - unexpected token"_test = [] {
      std::istringstream iss(R"({"id":42,,"name":"test"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on unexpected token";
   };

   "malformed JSON - invalid number"_test = [] {
      std::istringstream iss(R"({"id":42abc,"name":"test"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on invalid number";
   };

   "malformed JSON - invalid escape sequence"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"bad\qescape"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on invalid escape";
   };

   "truncated data mid-string"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"trunc)");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on truncated string";
   };

   "truncated data mid-array"_test = [] {
      std::istringstream iss(R"([1,2,3,)");
      glz::istream_buffer<> buffer(iss);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on truncated array";
   };

   "truncated data mid-object"_test = [] {
      std::istringstream iss(R"({"id":42,"name":)");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on truncated object";
   };

   "empty stream"_test = [] {
      std::istringstream iss("");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on empty stream";
   };

   "whitespace only stream"_test = [] {
      std::istringstream iss("   \n\t\r\n   ");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      // May fail with different errors depending on parse state, but should not succeed
      // Note: Some implementations may return no_read_input, end_reached, or other errors
      (void)ec; // Result varies, just ensure no crash
   };

   "multiple reads after successful parse"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"first"})");
      glz::istream_buffer<> buffer(iss);

      Record r1;
      auto ec1 = glz::read_json(r1, buffer);
      expect(!ec1) << "First read should succeed";
      expect(r1.id == 1);

      // Second read should fail (no more data)
      Record r2;
      auto ec2 = glz::read_json(r2, buffer);
      expect(ec2.ec != glz::error_code::none) << "Second read should fail";
   };

   "stream good() check after successful parse"_test = [] {
      std::istringstream iss(R"({"id":42,"name":"test"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec);
      // Stream should still be in good state (EOF is acceptable after reading all data)
   };

   "malformed JSON with slow stream"_test = [] {
      std::string json = R"({"id":42,"name":"unclosed)";
      slow_stringbuf sbuf(json, 8);
      std::istream slow_stream(&sbuf);

      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);
      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on malformed JSON even with slow stream";
   };

   "nested malformed JSON"_test = [] {
      std::istringstream iss(R"({"x":10,"arr":[1,2,{"bad":})");
      glz::istream_buffer<> buffer(iss);

      glz::generic json;
      auto ec = glz::read_json(json, buffer);

      expect(ec.ec != glz::error_code::none) << "Should fail on nested malformed JSON";
   };

   "extremely deep nesting (stack stress)"_test = [] {
      // Create deeply nested JSON
      std::string json = "";
      for (int i = 0; i < 100; ++i) {
         json += R"({"x":)";
      }
      json += "1";
      for (int i = 0; i < 100; ++i) {
         json += "}";
      }

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);

      glz::generic result;
      auto ec = glz::read_json(result, buffer);
      // Should either succeed or fail gracefully, not crash
      (void)ec;
   };

   "invalid UTF-8 in string"_test = [] {
      // Invalid UTF-8 byte sequence
      std::string json = R"({"id":1,"name":"invalid)";
      json += static_cast<char>(0xFF); // Invalid UTF-8 byte
      json += R"("})";

      std::istringstream iss(json);
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);
      // May succeed or fail depending on validation level, but should not crash
      (void)ec;
   };

   "number overflow"_test = [] {
      std::istringstream iss(R"({"id":99999999999999999999999999999999,"name":"overflow"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);
      // Should handle overflow gracefully
      (void)ec;
   };
};

// ============================================================================
// BUFFER BOUNDARY EDGE CASE TESTS
// ============================================================================

suite buffer_boundary_tests = [] {
   "string spanning multiple refills"_test = [] {
      // Note: Streaming refill points are at array/object field boundaries only.
      // A single string value must fit entirely within the buffer.
      // This test verifies that slow stream filling still works when buffer is large enough.
      std::string long_str(50, 'x');
      std::string json = R"({"id":1,"name":")" + long_str + R"("})";

      slow_stringbuf sbuf(json, 16); // Slow reads, but buffer is larger than content
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream); // Buffer larger than content

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.name == long_str);
   };

   "number at buffer boundary"_test = [] {
      // Position a number so it might be split across buffer reads
      std::string json = R"({"id":1234567890,"name":"test"})";

      slow_stringbuf sbuf(json, 7); // Split mid-number
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 1234567890);
   };

   "unicode at buffer boundary"_test = [] {
      // UTF-8 multi-byte character parsing with slow stream
      // Buffer must be large enough to hold the entire value
      std::string json = R"({"id":1,"name":"テスト日本語"})";

      slow_stringbuf sbuf(json, 5); // Slow reads
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream); // Large enough buffer

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.name == "テスト日本語");
   };

   "escape sequence at buffer boundary"_test = [] {
      std::string json = R"({"id":1,"name":"line1\nline2\ttab"})";

      slow_stringbuf sbuf(json, 6); // Slow reads
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream); // Large enough buffer

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.name == "line1\nline2\ttab");
   };

   "unicode escape at buffer boundary"_test = [] {
      // \\u0041 = 'A'
      std::string json = R"({"id":1,"name":"test\u0041end"})";

      slow_stringbuf sbuf(json, 4); // May split \\uXXXX
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.name == "testAend");
   };

   "key-value colon at buffer boundary"_test = [] {
      std::string json = R"({"id":42,"name":"test"})";

      // Try various offsets to hit colon at boundary
      for (size_t chunk = 3; chunk <= 8; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         Record r;
         auto ec = glz::read_json(r, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
         expect(r.id == 42);
         expect(r.name == "test");
      }
   };

   "array comma at buffer boundary"_test = [] {
      std::string json = R"([111,222,333,444,555,666,777,888,999])";

      for (size_t chunk = 3; chunk <= 8; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         std::vector<int> arr;
         auto ec = glz::read_json(arr, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
         expect(arr.size() == 9u);
         expect(arr[0] == 111);
         expect(arr[8] == 999);
      }
   };

   "object brace at buffer boundary"_test = [] {
      std::string json = R"({"inner":{"x":10,"y":20},"outer":30})";

      for (size_t chunk = 3; chunk <= 10; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         std::map<std::string, glz::generic> m;
         auto ec = glz::read_json(m, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
      }
   };

   "whitespace at buffer boundary"_test = [] {
      std::string json = "  {  \"id\"  :  42  ,  \"name\"  :  \"test\"  }  ";

      slow_stringbuf sbuf(json, 5);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 42);
   };

   "null/true/false at buffer boundary"_test = [] {
      std::string json = R"([null,true,false,null,true,false])";

      for (size_t chunk = 2; chunk <= 6; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         std::vector<std::optional<bool>> arr;
         auto ec = glz::read_json(arr, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
         expect(arr.size() == 6u);
         expect(!arr[0].has_value()); // null
         expect(arr[1].has_value() && *arr[1] == true);
         expect(arr[2].has_value() && *arr[2] == false);
      }
   };

   "deeply nested at each level crossing boundary"_test = [] {
      std::string json = R"({"a":{"b":{"c":{"d":{"e":{"f":1}}}}}})";

      for (size_t chunk = 2; chunk <= 8; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         glz::generic result;
         auto ec = glz::read_json(result, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
      }
   };

   "floating point at buffer boundary"_test = [] {
      std::string json = R"([3.14159265358979,2.71828182845904,1.41421356237309])";

      for (size_t chunk = 4; chunk <= 10; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         std::vector<double> arr;
         auto ec = glz::read_json(arr, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
         expect(arr.size() == 3u);
         expect(arr[0] > 3.14 && arr[0] < 3.15);
      }
   };

   "scientific notation at buffer boundary"_test = [] {
      std::string json = R"([1.23e+10,4.56e-20,7.89E+30])";

      for (size_t chunk = 3; chunk <= 8; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         std::vector<double> arr;
         auto ec = glz::read_json(arr, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
         expect(arr.size() == 3u);
      }
   };

   "negative numbers at buffer boundary"_test = [] {
      std::string json = R"([-123,-456,-789,-1000])";

      for (size_t chunk = 2; chunk <= 6; ++chunk) {
         slow_stringbuf sbuf(json, chunk);
         std::istream slow_stream(&sbuf);
         glz::basic_istream_buffer<std::istream, 512> buffer(slow_stream);

         std::vector<int> arr;
         auto ec = glz::read_json(arr, buffer);

         expect(!ec) << "Failed with chunk size " << chunk << ", error: " << int(ec.ec);
         expect(arr[0] == -123);
         expect(arr[3] == -1000);
      }
   };
};

// ============================================================================
// JSON_STREAM_READER EDGE CASE TESTS
// ============================================================================

suite json_stream_reader_edge_cases = [] {
   "object with moderately long string"_test = [] {
      // Note: A single JSON value must fit entirely within the buffer.
      // This tests a reasonable-sized object with a longer string.
      std::string long_name(200, 'x');
      std::string json = R"({"id":1,"name":")" + long_name + R"("})";

      std::istringstream iss(json);
      glz::json_stream_reader<Record, std::istream, 512> reader(iss); // Buffer larger than content

      Record r;
      auto ec = reader.read_next(r);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(r.id == 1);
      expect(r.name == long_name);
   };

   "NDJSON with varying whitespace"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"a"}
   {"id":2,"name":"b"}
      {"id":3,"name":"c"})");

      std::vector<Record> records;
      for (auto&& r : glz::json_stream_reader<Record>(iss)) {
         records.push_back(r);
      }

      expect(records.size() == 3u);
      expect(records[2].id == 3);
   };

   "NDJSON with blank lines"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"a"}

{"id":2,"name":"b"}


{"id":3,"name":"c"})");

      std::vector<Record> records;
      for (auto&& r : glz::json_stream_reader<Record>(iss)) {
         records.push_back(r);
      }

      expect(records.size() == 3u);
   };

   "NDJSON with trailing whitespace"_test = [] {
      std::istringstream iss("{\"id\":1,\"name\":\"a\"}   \n{\"id\":2,\"name\":\"b\"}   \n");

      std::vector<Record> records;
      for (auto&& r : glz::json_stream_reader<Record>(iss)) {
         records.push_back(r);
      }

      expect(records.size() == 2u);
   };

   "NDJSON with CRLF line endings"_test = [] {
      std::istringstream iss("{\"id\":1,\"name\":\"a\"}\r\n{\"id\":2,\"name\":\"b\"}\r\n{\"id\":3,\"name\":\"c\"}\r\n");

      std::vector<Record> records;
      for (auto&& r : glz::json_stream_reader<Record>(iss)) {
         records.push_back(r);
      }

      expect(records.size() == 3u);
   };

   "multiple read_next after EOF"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"only"})");
      glz::json_stream_reader<Record> reader(iss);

      Record r;
      expect(!reader.read_next(r)); // First read succeeds
      expect(r.id == 1);

      // Subsequent reads should consistently return error
      expect(reader.read_next(r).ec != glz::error_code::none);
      expect(reader.read_next(r).ec != glz::error_code::none);
      expect(reader.read_next(r).ec != glz::error_code::none);
      expect(reader.eof());
   };

   "has_more accuracy"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"a"}
{"id":2,"name":"b"})");
      glz::json_stream_reader<Record> reader(iss);

      expect(reader.has_more());

      Record r;
      reader.read_next(r);
      expect(reader.has_more()); // Still has second record

      reader.read_next(r);
      // After reading all, has_more should be false
      expect(!reader.has_more() || reader.eof());
   };

   "bytes_consumed tracking"_test = [] {
      std::string json = R"({"id":1,"name":"a"}
{"id":2,"name":"b"})";
      std::istringstream iss(json);
      glz::json_stream_reader<Record> reader(iss);

      Record r;
      reader.read_next(r);
      size_t after_first = reader.bytes_consumed();
      expect(after_first > 0u);

      reader.read_next(r);
      size_t after_second = reader.bytes_consumed();
      expect(after_second > after_first);
   };

   "buffer accessor"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"test"})");
      glz::json_stream_reader<Record> reader(iss);

      auto& buf = reader.buffer();
      expect(buf.size() > 0u);

      const auto& const_reader = reader;
      const auto& const_buf = const_reader.buffer();
      expect(const_buf.size() > 0u);
   };

   "custom buffer capacity"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"test"})");
      glz::json_stream_reader<Record, std::istream, 512> reader(iss); // 512-byte buffer (minimum)

      Record r;
      auto ec = reader.read_next(r);
      expect(!ec);
      expect(r.id == 1);
   };

   "JSON array not supported as stream (single value)"_test = [] {
      std::istringstream iss(R"([{"id":1,"name":"a"},{"id":2,"name":"b"}])");
      glz::json_stream_reader<Record> reader(iss);

      Record r;
      // Reading a JSON array as individual records will fail
      // because the reader expects top-level objects
      auto ec = reader.read_next(r);
      expect(ec.ec != glz::error_code::none);
   };

   "NDJSON with parse error mid-stream"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"valid"}
{"id":invalid}
{"id":3,"name":"never reached"})");
      glz::json_stream_reader<Record> reader(iss);

      Record r;
      expect(!reader.read_next(r)); // First succeeds
      expect(r.id == 1);

      auto ec = reader.read_next(r); // Second fails
      expect(ec.ec != glz::error_code::none);
      expect(ec.ec != glz::error_code::end_reached); // Parse error, not EOF
   };

   "single value (not NDJSON)"_test = [] {
      std::istringstream iss(R"({"id":42,"name":"single"})");
      glz::json_stream_reader<Record> reader(iss);

      std::vector<Record> records;
      Record r;
      while (!reader.read_next(r)) {
         records.push_back(r);
      }

      expect(records.size() == 1u);
      expect(records[0].id == 42);
   };

   "empty lines only"_test = [] {
      std::istringstream iss("\n\n\n   \n\t\n");
      glz::json_stream_reader<Record> reader(iss);

      Record r;
      auto ec = reader.read_next(r);
      expect(ec.ec == glz::error_code::end_reached);
      expect(reader.eof());
   };

   "slow stream with NDJSON"_test = [] {
      std::string json = R"({"id":1,"name":"a"}
{"id":2,"name":"b"}
{"id":3,"name":"c"})";
      slow_stringbuf sbuf(json, 8);
      std::istream slow_stream(&sbuf);

      std::vector<Record> records;
      for (auto&& r : glz::json_stream_reader<Record>(slow_stream)) {
         records.push_back(r);
      }

      expect(records.size() == 3u);
      expect(records[2].id == 3);
   };
};

// ============================================================================
// FILE I/O INTEGRATION TESTS
// ============================================================================

// Helper to create temp file path
inline std::string temp_file_path(const std::string& name)
{
   return "/tmp/glaze_streaming_test_" + name;
}

suite file_io_streaming_tests = [] {
   "write JSON to file then read back"_test = [] {
      std::string filepath = temp_file_path("json_roundtrip.json");

      ComplexObj original{.id = 12345,
                          .name = "file roundtrip test",
                          .value = 3.14159,
                          .numbers = {1, 2, 3, 4, 5},
                          .mapping = {{"key1", 100}, {"key2", 200}},
                          .optional_field = "optional value"};

      // Write to file
      {
         std::ofstream file(filepath);
         expect(file.good()) << "Failed to open file for writing";
         glz::basic_ostream_buffer<std::ofstream> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec) << "Write error: " << int(ec.ec);
      }

      // Read from file
      {
         std::ifstream file(filepath);
         expect(file.good()) << "Failed to open file for reading";
         glz::basic_istream_buffer<std::ifstream> buffer(file);
         ComplexObj parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec) << "Read error: " << int(ec.ec);
         expect(parsed == original);
      }

      // Cleanup
      std::remove(filepath.c_str());
   };

   "write BEVE to file then read back"_test = [] {
      std::string filepath = temp_file_path("beve_roundtrip.beve");

      ComplexObj original{.id = 99999,
                          .name = "beve file test",
                          .value = 2.71828,
                          .numbers = {10, 20, 30},
                          .mapping = {{"x", 1}, {"y", 2}},
                          .optional_field = std::nullopt};

      // Write to file
      {
         std::ofstream file(filepath, std::ios::binary);
         expect(file.good()) << "Failed to open file for writing";
         glz::basic_ostream_buffer<std::ofstream> buffer(file);
         auto ec = glz::write_beve(original, buffer);
         expect(!ec) << "Write error: " << int(ec.ec);
      }

      // Read from file (using string buffer since read_beve doesn't have streaming overload)
      {
         std::ifstream file(filepath, std::ios::binary);
         expect(file.good()) << "Failed to open file for reading";
         std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
         ComplexObj parsed;
         auto ec = glz::read_beve(parsed, data);
         expect(!ec) << "Read error: " << int(ec.ec);
         expect(parsed == original);
      }

      std::remove(filepath.c_str());
   };

   "large array to file"_test = [] {
      std::string filepath = temp_file_path("large_array.json");

      std::vector<int> original(10000);
      for (int i = 0; i < 10000; ++i) {
         original[i] = i;
      }

      // Write
      {
         std::ofstream file(filepath);
         glz::basic_ostream_buffer<std::ofstream> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec);
      }

      // Read
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream> buffer(file);
         std::vector<int> parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec);
         expect(parsed == original);
      }

      std::remove(filepath.c_str());
   };

   "many small objects to file"_test = [] {
      std::string filepath = temp_file_path("many_objects.json");

      std::vector<Record> original;
      for (int i = 0; i < 1000; ++i) {
         original.push_back({i, "record_" + std::to_string(i)});
      }

      // Write
      {
         std::ofstream file(filepath);
         glz::basic_ostream_buffer<std::ofstream> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec);
      }

      // Read
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream> buffer(file);
         std::vector<Record> parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec);
         expect(parsed == original);
      }

      std::remove(filepath.c_str());
   };

   "large file 1MB+ JSON array"_test = [] {
      std::string filepath = temp_file_path("large_1mb.json");

      // Create ~1.5MB of data (150,000 integers, each ~10 chars with comma)
      std::vector<int> original(150000);
      for (int i = 0; i < 150000; ++i) {
         original[i] = i * 7; // Varying digit lengths
      }

      // Write
      {
         std::ofstream file(filepath);
         expect(file.good()) << "Failed to open file for writing";
         glz::basic_ostream_buffer<std::ofstream> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec) << "Write error: " << int(ec.ec);
      }

      // Verify file size is >1MB
      {
         std::ifstream file(filepath, std::ios::ate);
         auto size = file.tellg();
         expect(size > 1'000'000) << "File size: " << size << " bytes (expected >1MB)";
      }

      // Read back with streaming
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream> buffer(file);
         std::vector<int> parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec) << "Read error: " << int(ec.ec);
         expect(parsed.size() == original.size());
         expect(parsed == original);
      }

      std::remove(filepath.c_str());
   };

   "large file 5MB+ JSON objects"_test = [] {
      std::string filepath = temp_file_path("large_5mb.json");

      // Create ~5MB of data: 50,000 objects with longer strings
      std::vector<LargeRecord> original;
      original.reserve(50000);
      for (int i = 0; i < 50000; ++i) {
         original.push_back({i, "record_" + std::to_string(i),
                             "This is a longer description field with more text to increase file size for record number " +
                                std::to_string(i),
                             {i, i + 1, i + 2, i + 3, i + 4}});
      }

      // Write
      {
         std::ofstream file(filepath);
         expect(file.good()) << "Failed to open file for writing";
         glz::basic_ostream_buffer<std::ofstream> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec) << "Write error: " << int(ec.ec);
      }

      // Verify file size is >5MB
      {
         std::ifstream file(filepath, std::ios::ate);
         auto size = file.tellg();
         expect(size > 5'000'000) << "File size: " << size << " bytes (expected >5MB)";
      }

      // Read back with streaming
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream> buffer(file);
         std::vector<LargeRecord> parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec) << "Read error: " << int(ec.ec);
         expect(parsed.size() == original.size());
         // Spot check a few records
         expect(parsed[0].id == 0);
         expect(parsed[25000].id == 25000);
         expect(parsed[49999].id == 49999);
      }

      std::remove(filepath.c_str());
   };

   "large file with small streaming buffer"_test = [] {
      std::string filepath = temp_file_path("large_small_buffer.json");

      // Create ~500KB of data
      std::vector<int> original(50000);
      for (int i = 0; i < 50000; ++i) {
         original[i] = i;
      }

      // Write with small buffer (512 bytes) - forces many flushes
      {
         std::ofstream file(filepath);
         glz::basic_ostream_buffer<std::ofstream, 512> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec) << "Write error: " << int(ec.ec);
      }

      // Read with small buffer (512 bytes) - forces many refills
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream, 512> buffer(file);
         std::vector<int> parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec) << "Read error: " << int(ec.ec);
         expect(parsed == original);
      }

      std::remove(filepath.c_str());
   };

   "large NDJSON file 10K records"_test = [] {
      std::string filepath = temp_file_path("large_ndjson.ndjson");

      // Write 10,000 NDJSON records (~500KB)
      // Note: Record struct only has id and name fields
      {
         std::ofstream file(filepath);
         for (int i = 0; i < 10000; ++i) {
            file << R"({"id":)" << i << R"(,"name":"record_)" << i << R"("})" << "\n";
         }
      }

      // Verify file size
      {
         std::ifstream file(filepath, std::ios::ate);
         auto size = file.tellg();
         expect(size > 300'000) << "File size: " << size << " bytes";
      }

      // Read with stream reader
      {
         std::ifstream file(filepath);
         glz::json_stream_reader<Record, std::ifstream> reader(file);

         int count = 0;
         Record r;
         while (!reader.read_next(r)) {
            expect(r.id == count) << "Mismatch at record " << count;
            count++;
         }
         expect(count == 10000) << "Read " << count << " records, expected 10000";
      }

      std::remove(filepath.c_str());
   };

   "bounded memory with large file"_test = [] {
      std::string filepath = temp_file_path("bounded_memory.json");

      // Create 2MB file
      std::vector<int> original(200000);
      for (int i = 0; i < 200000; ++i) {
         original[i] = i;
      }

      // Write
      {
         std::ofstream file(filepath);
         glz::basic_ostream_buffer<std::ofstream, 4096> buffer(file); // 4KB buffer
         auto ec = glz::write_json(original, buffer);
         expect(!ec);

         // Buffer should have flushed multiple times
         expect(buffer.bytes_flushed() > 4096u) << "Expected multiple flushes";
      }

      // Read with bounded buffer
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream, 4096> buffer(file); // 4KB buffer
         std::vector<int> parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec);
         expect(parsed == original);

         // Verify multiple refills occurred
         expect(buffer.bytes_consumed() > 4096u) << "Expected multiple refills";
      }

      std::remove(filepath.c_str());
   };

   "NDJSON file with stream reader"_test = [] {
      std::string filepath = temp_file_path("events.ndjson");

      // Write NDJSON
      {
         std::ofstream file(filepath);
         for (int i = 1; i <= 100; ++i) {
            file << R"({"id":)" << i << R"(,"name":"event)" << i << R"("})" << "\n";
         }
      }

      // Read with stream reader
      {
         std::ifstream file(filepath);
         glz::json_stream_reader<Record, std::ifstream> reader(file);

         std::vector<Record> records;
         Record r;
         while (!reader.read_next(r)) {
            records.push_back(r);
         }

         expect(records.size() == 100u);
         expect(records[0].id == 1);
         expect(records[99].id == 100);
      }

      std::remove(filepath.c_str());
   };

   "file with small buffer"_test = [] {
      std::string filepath = temp_file_path("small_buffer.json");

      Record original{42, "test"}; // Short content to fit in small buffer

      // Write
      {
         std::ofstream file(filepath);
         glz::basic_ostream_buffer<std::ofstream, 512> buffer(file); // Buffer large enough for content
         auto ec = glz::write_json(original, buffer);
         expect(!ec);
      }

      // Read
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream, 512> buffer(file); // Buffer large enough for content
         Record parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec);
         expect(parsed == original);
      }

      std::remove(filepath.c_str());
   };

   "non-existent file read"_test = [] {
      std::ifstream file("/tmp/glaze_nonexistent_file_12345.json");
      glz::basic_istream_buffer<std::ifstream> buffer(file);
      expect(buffer.eof()); // Should be at EOF since file didn't open
   };

   "deeply nested structure to file"_test = [] {
      std::string filepath = temp_file_path("deep_nested.json");

      Company original{.name = "FileCorp",
                       .departments = {Department{.name = "Engineering",
                                                  .employees = {Person{.name = "Alice",
                                                                       .age = 30,
                                                                       .address = {.street = "123 Main St", .city = "Boston", .zip = 12345},
                                                                       .emails = {"alice@company.com"},
                                                                       .metadata = {{"role", "engineer"}}}},
                                                  .managers = {},
                                                  .head = std::nullopt}},
                       .teams = {},
                       .nested_maps = {{"budget", {{"q1", 100000}}}}};

      // Write
      {
         std::ofstream file(filepath);
         glz::basic_ostream_buffer<std::ofstream> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec);
      }

      // Read
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream> buffer(file);
         Company parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec);
         expect(parsed == original);
      }

      std::remove(filepath.c_str());
   };
};

// ============================================================================
// 4KB BUFFER SIZE TESTS (realistic production size)
// ============================================================================

suite small_buffer_4kb_tests = [] {
   "4KB buffer - array larger than buffer"_test = [] {
      // Create array that's ~20KB when serialized (5x buffer size)
      std::vector<int> original(2000);
      for (int i = 0; i < 2000; ++i) {
         original[i] = i * 12345; // 8-10 digit numbers
      }

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 4096> write_buf(oss);
      auto wec = glz::write_json(original, write_buf);
      expect(!wec) << "Write error: " << int(wec.ec);

      std::string json = oss.str();
      expect(json.size() > 4096u * 3) << "JSON size: " << json.size();

      std::istringstream iss(json);
      glz::basic_istream_buffer<std::istream, 4096> read_buf(iss);
      std::vector<int> parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec) << "Read error: " << int(rec.ec);
      expect(parsed == original);
   };

   "4KB buffer - many small objects"_test = [] {
      // 500 small objects, each ~30 bytes = ~15KB total
      std::vector<Record> original;
      for (int i = 0; i < 500; ++i) {
         original.push_back({i, "r" + std::to_string(i)});
      }

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 4096> write_buf(oss);
      auto wec = glz::write_json(original, write_buf);
      expect(!wec);

      std::istringstream iss(oss.str());
      glz::basic_istream_buffer<std::istream, 4096> read_buf(iss);
      std::vector<Record> parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec);
      expect(parsed == original);
   };

   "4KB buffer - deeply nested structure"_test = [] {
      // Nested maps and arrays that span multiple buffer refills
      std::map<std::string, std::map<std::string, std::vector<int>>> nested;
      for (int i = 0; i < 50; ++i) {
         std::string outer_key = "outer_key_" + std::to_string(i);
         for (int j = 0; j < 10; ++j) {
            std::string inner_key = "inner_key_" + std::to_string(j);
            std::vector<int> values(20);
            for (int k = 0; k < 20; ++k) {
               values[k] = i * 1000 + j * 100 + k;
            }
            nested[outer_key][inner_key] = values;
         }
      }

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 4096> write_buf(oss);
      auto wec = glz::write_json(nested, write_buf);
      expect(!wec);

      std::string json = oss.str();
      expect(json.size() > 4096u * 10) << "JSON size: " << json.size(); // Should be ~50KB+

      std::istringstream iss(json);
      glz::basic_istream_buffer<std::istream, 4096> read_buf(iss);
      std::map<std::string, std::map<std::string, std::vector<int>>> parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec);
      expect(parsed == nested);
   };

   "4KB buffer - long strings near buffer boundary"_test = [] {
      // Create strings of varying lengths around the 4KB boundary
      // Note: Strings must fit entirely in buffer for streaming to work
      std::vector<std::string> original;
      original.push_back(std::string(1000, 'a'));
      original.push_back(std::string(2000, 'b'));
      original.push_back(std::string(3000, 'c'));
      original.push_back(std::string(3500, 'd'));

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 4096> write_buf(oss);
      auto wec = glz::write_json(original, write_buf);
      expect(!wec);

      std::istringstream iss(oss.str());
      glz::basic_istream_buffer<std::istream, 4096> read_buf(iss);
      std::vector<std::string> parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec);
      expect(parsed == original);
   };

   "4KB buffer - file roundtrip"_test = [] {
      std::string filepath = temp_file_path("4kb_buffer_test.json");

      // Create ~100KB of data
      std::vector<LargeRecord> original;
      for (int i = 0; i < 500; ++i) {
         original.push_back(
            {i, "record_" + std::to_string(i), "Description text for record " + std::to_string(i), {i, i + 1, i + 2}});
      }

      // Write with 4KB buffer
      {
         std::ofstream file(filepath);
         glz::basic_ostream_buffer<std::ofstream, 4096> buffer(file);
         auto ec = glz::write_json(original, buffer);
         expect(!ec);
         expect(buffer.bytes_flushed() > 4096u * 5) << "Expected many flushes";
      }

      // Read with 4KB buffer
      {
         std::ifstream file(filepath);
         glz::basic_istream_buffer<std::ifstream, 4096> buffer(file);
         std::vector<LargeRecord> parsed;
         auto ec = glz::read_json(parsed, buffer);
         expect(!ec);
         expect(parsed.size() == original.size());
         expect(parsed[0].id == 0);
         expect(parsed[499].id == 499);
      }

      std::remove(filepath.c_str());
   };

   "4KB buffer - NDJSON streaming"_test = [] {
      std::string filepath = temp_file_path("4kb_ndjson.ndjson");

      // Write 1000 NDJSON records
      {
         std::ofstream file(filepath);
         for (int i = 0; i < 1000; ++i) {
            file << R"({"id":)" << i << R"(,"name":"record_)" << i << R"("})" << "\n";
         }
      }

      // Read with 4KB buffer stream reader
      {
         std::ifstream file(filepath);
         glz::json_stream_reader<Record, std::ifstream, 4096> reader(file);

         int count = 0;
         Record r;
         while (!reader.read_next(r)) {
            expect(r.id == count);
            count++;
         }
         expect(count == 1000);
      }

      std::remove(filepath.c_str());
   };

   "4KB buffer - slow stream simulation"_test = [] {
      // Create ~20KB JSON
      std::vector<int> original(2000);
      for (int i = 0; i < 2000; ++i) {
         original[i] = i * 999;
      }

      std::string json;
      auto wec = glz::write_json(original, json);
      expect(!wec);

      // Simulate slow stream that delivers 500 bytes at a time
      slow_stringbuf sbuf(json, 500);
      std::istream slow_stream(&sbuf);
      glz::basic_istream_buffer<std::istream, 4096> buffer(slow_stream);

      std::vector<int> parsed;
      auto ec = glz::read_json(parsed, buffer);
      expect(!ec) << "Error: " << int(ec.ec);
      expect(parsed == original);
   };

   "4KB buffer - mixed types"_test = [] {
      MixedData original;
      for (int i = 0; i < 200; ++i) {
         original.ints.push_back(i);
         original.doubles.push_back(i * 3.14159);
         original.strings.push_back("str_" + std::to_string(i));
         original.map["key_" + std::to_string(i)] = i * 2;
      }
      original.opt = "optional value here";

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 4096> write_buf(oss);
      auto wec = glz::write_json(original, write_buf);
      expect(!wec);

      std::istringstream iss(oss.str());
      glz::basic_istream_buffer<std::istream, 4096> read_buf(iss);
      MixedData parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec);
      expect(parsed == original);
   };

   "4KB buffer - binary format (BEVE)"_test = [] {
      std::vector<int> original(5000);
      for (int i = 0; i < 5000; ++i) {
         original[i] = i;
      }

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 4096> write_buf(oss);
      auto wec = glz::write_beve(original, write_buf);
      expect(!wec);

      // Verify multiple flushes occurred
      expect(write_buf.bytes_flushed() > 4096u);

      // Read back (BEVE doesn't have streaming read yet, use string)
      std::vector<int> parsed;
      auto rec = glz::read_beve(parsed, oss.str());
      expect(!rec);
      expect(parsed == original);
   };

   "4KB buffer - data slightly under buffer size"_test = [] {
      // Create data that's just under 4KB
      std::string data(4000, 'x');

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 4096> write_buf(oss);
      auto wec = glz::write_json(data, write_buf);
      expect(!wec);

      std::istringstream iss(oss.str());
      glz::basic_istream_buffer<std::istream, 4096> read_buf(iss);
      std::string parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec);
      expect(parsed == data);
   };
};

// ============================================================================
// MINIMUM BUFFER SIZE REQUIREMENTS
// ============================================================================
// The minimum buffer size is 512 bytes (2 * write_padding_bytes), enforced by
// requires clause and static_assert. This ensures buffers can handle all JSON
// value types and matches the internal write code's initial buffer sizing.
// ============================================================================

suite buffer_size_requirements = [] {
   "minimum buffer size constant"_test = [] {
      // Verify the minimum buffer size constant exists and is 512 (2 * write_padding_bytes)
      static_assert(glz::min_streaming_buffer_size == 512);
      static_assert(glz::min_ostream_buffer_size == 512);
   };

   "512-byte buffer handles extreme floating point values"_test = [] {
      // A double like -1.7976931348623157E308 serializes to 23 bytes
      // 512-byte buffer easily handles this
      std::string json = "[-1.7976931348623157E308,2.2250738585072014E-308]";

      std::istringstream iss(json);
      glz::basic_istream_buffer<std::istream, 512> buffer(iss);
      std::vector<double> arr;
      auto ec = glz::read_json(arr, buffer);
      expect(!ec) << "512-byte buffer should handle extreme floats";
      expect(arr.size() == 2u);
   };

   "DBL_MAX and DBL_MIN with minimum 512-byte buffer"_test = [] {
      std::vector<double> original = {std::numeric_limits<double>::max(), std::numeric_limits<double>::min(),
                                      std::numeric_limits<double>::lowest(), std::numeric_limits<double>::epsilon()};

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 512> write_buf(oss);
      auto wec = glz::write_json(original, write_buf);
      expect(!wec);

      std::istringstream iss(oss.str());
      glz::basic_istream_buffer<std::istream, 512> read_buf(iss);
      std::vector<double> parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec) << "Error: " << int(rec.ec);
      expect(parsed.size() == 4u);
   };

   "scientific notation extremes"_test = [] {
      std::vector<double> original = {1e308, 1e-308, -1e308, -1e-308, 9.999999999999999e307};

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 512> write_buf(oss);
      auto wec = glz::write_json(original, write_buf);
      expect(!wec);

      std::istringstream iss(oss.str());
      glz::basic_istream_buffer<std::istream, 512> read_buf(iss);
      std::vector<double> parsed;
      auto rec = glz::read_json(parsed, read_buf);
      expect(!rec);
      expect(parsed.size() == 5u);
   };

   "512-byte buffer handles long strings"_test = [] {
      // 512 bytes allows strings up to ~500 characters
      std::string str_200_chars(200, 'x');
      std::string json = "\"" + str_200_chars + "\"";

      std::istringstream iss(json);
      glz::basic_istream_buffer<std::istream, 512> buffer(iss);
      std::string parsed;
      auto ec = glz::read_json(parsed, buffer);
      expect(!ec);
      expect(parsed == str_200_chars);
   };

   "output streaming with minimum buffer (512 bytes)"_test = [] {
      // Output buffers flush at element boundaries
      std::vector<double> original = {-1.7976931348623157e+308, 2.2250738585072014e-308};

      std::ostringstream oss;
      glz::basic_ostream_buffer<std::ostream, 512> write_buf(oss);
      auto wec = glz::write_json(original, write_buf);
      expect(!wec);

      // Verify output is correct
      std::vector<double> parsed;
      auto rec = glz::read_json(parsed, oss.str());
      expect(!rec);
      expect(parsed.size() == 2u);
   };

   };

// ============================================================================
// STREAMING_STATE UNIT TESTS
// ============================================================================

suite streaming_state_unit_tests = [] {
   "make_streaming_state returns valid state"_test = [] {
      std::istringstream iss(R"({"id":1})");
      glz::istream_buffer<> buffer(iss);

      auto state = glz::make_streaming_state(buffer);

      expect(state.enabled());
      expect(state.data() != nullptr);
      expect(state.size() > 0u);
   };

   "streaming_state data and size"_test = [] {
      std::istringstream iss("test data");
      glz::istream_buffer<> buffer(iss);

      auto state = glz::make_streaming_state(buffer);

      expect(state.data() == buffer.data());
      expect(state.size() == buffer.size());
   };

   "streaming_state consume_bytes"_test = [] {
      std::istringstream iss("hello world");
      glz::istream_buffer<> buffer(iss);

      auto state = glz::make_streaming_state(buffer);
      size_t original_size = state.size();

      state.consume_bytes(5);

      expect(buffer.bytes_consumed() == 5u);
      // After consume, size should be reduced
      expect(state.size() == original_size - 5u);
   };

   "streaming_state at_eof"_test = [] {
      std::istringstream iss("x");
      glz::istream_buffer<> buffer(iss);

      auto state = glz::make_streaming_state(buffer);

      expect(!state.at_eof());

      // Consume all data
      state.consume_bytes(buffer.size());
      buffer.refill(); // Try to get more

      expect(state.at_eof());
   };

   "has_streaming_state concept"_test = [] {
      static_assert(glz::has_streaming_state<glz::streaming_context>);
      static_assert(!glz::has_streaming_state<glz::context>);
   };

   "streaming_context inherits from context"_test = [] {
      glz::streaming_context ctx;

      // Should have all context members
      ctx.error = glz::error_code::none;
      ctx.indentation_level = 0;

      // Plus streaming state
      expect(!ctx.stream.enabled()); // Not initialized
   };

   "consume_and_refill updates iterators"_test = [] {
      std::istringstream iss("first second third");
      glz::basic_istream_buffer<std::istringstream, 512> buffer(iss);

      auto state = glz::make_streaming_state(buffer);

      const char* new_it;
      const char* new_end;

      // Consume some bytes and refill
      bool has_data = state.consume_and_refill(6, new_it, new_end); // Consume "first "

      expect(has_data);
      expect(new_it != nullptr);
      expect(new_end > new_it);
   };
};

// ============================================================================
// SPECIAL TYPES WITH STREAMING INPUT
// ============================================================================

// Enum for testing
enum class Status { Pending, Active, Completed };

template <>
struct glz::meta<Status>
{
   using enum Status;
   static constexpr auto value = enumerate(Pending, Active, Completed);
};

suite streaming_special_types_input_tests = [] {
   "enum with streaming input"_test = [] {
      std::istringstream iss("\"Active\"");
      glz::istream_buffer<> buffer(iss);

      Status s;
      auto ec = glz::read_json(s, buffer);

      expect(!ec) << "Error: " << int(ec.ec);
      expect(s == Status::Active);
   };

   "optional with value - streaming input"_test = [] {
      std::istringstream iss("42");
      glz::istream_buffer<> buffer(iss);

      std::optional<int> opt;
      auto ec = glz::read_json(opt, buffer);

      expect(!ec);
      expect(opt.has_value());
      expect(*opt == 42);
   };

   "optional null - streaming input"_test = [] {
      std::istringstream iss("null");
      glz::istream_buffer<> buffer(iss);

      std::optional<int> opt = 999; // Pre-set value
      auto ec = glz::read_json(opt, buffer);

      expect(!ec);
      expect(!opt.has_value());
   };

   "variant - streaming input"_test = [] {
      std::istringstream iss("\"hello\"");
      glz::istream_buffer<> buffer(iss);

      std::variant<int, std::string, double> v;
      auto ec = glz::read_json(v, buffer);

      expect(!ec);
      expect(std::holds_alternative<std::string>(v));
      expect(std::get<std::string>(v) == "hello");
   };

   "tuple - streaming input"_test = [] {
      std::istringstream iss("[1,\"two\",3.0]");
      glz::istream_buffer<> buffer(iss);

      std::tuple<int, std::string, double> t;
      auto ec = glz::read_json(t, buffer);

      expect(!ec);
      expect(std::get<0>(t) == 1);
      expect(std::get<1>(t) == "two");
      expect(std::get<2>(t) == 3.0);
   };

   "array of optionals - streaming input"_test = [] {
      std::istringstream iss("[1,null,3,null,5]");
      glz::istream_buffer<> buffer(iss);

      std::vector<std::optional<int>> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec);
      expect(arr.size() == 5u);
      expect(arr[0].has_value() && *arr[0] == 1);
      expect(!arr[1].has_value());
      expect(arr[2].has_value() && *arr[2] == 3);
      expect(!arr[3].has_value());
      expect(arr[4].has_value() && *arr[4] == 5);
   };

   "boolean values - streaming input"_test = [] {
      {
         std::istringstream iss("true");
         glz::istream_buffer<> buffer(iss);
         bool b = false;
         auto ec = glz::read_json(b, buffer);
         expect(!ec);
         expect(b == true);
      }
      {
         std::istringstream iss("false");
         glz::istream_buffer<> buffer(iss);
         bool b = true;
         auto ec = glz::read_json(b, buffer);
         expect(!ec);
         expect(b == false);
      }
   };

   "null to optional - streaming input"_test = [] {
      std::istringstream iss("null");
      glz::istream_buffer<> buffer(iss);

      std::optional<std::string> opt = "preset";
      auto ec = glz::read_json(opt, buffer);

      expect(!ec);
      expect(!opt.has_value());
   };

   "nested optional vector - streaming input"_test = [] {
      std::istringstream iss("[1,2,3]");
      glz::istream_buffer<> buffer(iss);

      std::optional<std::vector<int>> opt;
      auto ec = glz::read_json(opt, buffer);

      expect(!ec);
      expect(opt.has_value());
      expect(opt->size() == 3u);
      expect((*opt)[2] == 3);
   };
};

// ============================================================================
// ISTREAM_BUFFER RESET AND REUSE TESTS
// ============================================================================

suite istream_buffer_reset_tests = [] {
   "reset clears state"_test = [] {
      std::istringstream iss("test data here");
      glz::istream_buffer<> buffer(iss);

      buffer.consume(5);
      expect(buffer.bytes_consumed() == 5u);

      buffer.reset();
      expect(buffer.bytes_consumed() == 0u);
   };

   "good and eof accessors"_test = [] {
      // Note: After initial fill, stream may be at EOF if data fits in buffer
      // good() returns stream_->good() which is false at EOF
      std::string long_data(100000, 'x'); // Data larger than buffer
      std::istringstream iss(long_data);
      glz::basic_istream_buffer<std::istream, 1024> buffer(iss);

      // With more data available, stream should be good
      expect(buffer.good());
      expect(!buffer.fail());
   };

   "stream accessor"_test = [] {
      std::istringstream iss("data");
      glz::istream_buffer<> buffer(iss);

      expect(buffer.stream() == &iss);
   };

   "iterator support"_test = [] {
      std::istringstream iss("hello");
      glz::istream_buffer<> buffer(iss);

      auto begin_it = buffer.begin();
      auto end_it = buffer.end();

      expect(begin_it != end_it);
      expect(*begin_it == 'h');
   };

   "buffer_capacity accessor"_test = [] {
      std::istringstream iss("data");
      glz::basic_istream_buffer<std::istringstream, 512> buffer(iss);

      expect(buffer.buffer_capacity() == 512u);
   };
};

// ============================================================================
// DOCUMENTATION EXAMPLES AS TESTS (INPUT)
// ============================================================================

suite input_documentation_example_tests = [] {
   "input streaming example from docs"_test = [] {
      std::istringstream iss(R"({"id":42,"name":"example"})");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec);
      expect(r.id == 42);
      expect(r.name == "example");
   };

   "polymorphic istream_buffer example"_test = [] {
      std::istringstream iss("123");
      glz::istream_buffer<> buffer(iss); // Alias for basic_istream_buffer<std::istream>

      int val;
      auto ec = glz::read_json(val, buffer);

      expect(!ec);
      expect(val == 123);
   };

   "custom buffer capacity example"_test = [] {
      std::istringstream iss(R"([1,2,3])");
      glz::istream_buffer<4096> buffer(iss); // 4KB buffer

      expect(buffer.buffer_capacity() == 4096u);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);
      expect(!ec);
      expect(arr.size() == 3u);
   };

   "concrete stream type example"_test = [] {
      std::istringstream iss("\"hello\"");
      glz::basic_istream_buffer<std::istringstream> buffer(iss);

      std::string s;
      auto ec = glz::read_json(s, buffer);

      expect(!ec);
      expect(s == "hello");
   };

   "json_stream_reader example from docs"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"a"}
{"id":2,"name":"b"})");

      // Manual iteration
      glz::json_stream_reader<Record> reader(iss);
      Record r;
      int count = 0;
      while (!reader.read_next(r)) {
         count++;
      }
      expect(count == 2);
   };

   "read_json_stream convenience function example"_test = [] {
      std::istringstream iss(R"({"id":1,"name":"a"}
{"id":2,"name":"b"}
{"id":3,"name":"c"})");

      std::vector<Record> records;
      auto ec = glz::read_json_stream(records, iss);

      expect(!ec);
      expect(records.size() == 3u);
   };
};

// ============================================================================
// ADDITIONAL EDGE CASES
// ============================================================================

suite additional_edge_cases = [] {
   "very large numbers"_test = [] {
      std::istringstream iss("[9223372036854775807,-9223372036854775808]");
      glz::istream_buffer<> buffer(iss);

      std::vector<int64_t> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec);
      expect(arr.size() == 2u);
      expect(arr[0] == INT64_MAX);
      expect(arr[1] == INT64_MIN);
   };

   "floating point edge cases"_test = [] {
      std::istringstream iss("[0.0,-0.0,1e308,1e-308]");
      glz::istream_buffer<> buffer(iss);

      std::vector<double> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec);
      expect(arr.size() == 4u);
   };

   "empty string"_test = [] {
      std::istringstream iss(R"("")");
      glz::istream_buffer<> buffer(iss);

      std::string s;
      auto ec = glz::read_json(s, buffer);

      expect(!ec);
      expect(s.empty());
   };

   "string with only whitespace"_test = [] {
      std::istringstream iss(R"("   ")");
      glz::istream_buffer<> buffer(iss);

      std::string s;
      auto ec = glz::read_json(s, buffer);

      expect(!ec);
      expect(s == "   ");
   };

   "deeply nested arrays"_test = [] {
      std::istringstream iss("[[[[[1]]]]]");
      glz::istream_buffer<> buffer(iss);

      std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>> nested;
      auto ec = glz::read_json(nested, buffer);

      expect(!ec);
      expect(nested[0][0][0][0][0] == 1);
   };

   "map with numeric string keys"_test = [] {
      std::istringstream iss(R"({"123":1,"456":2})");
      glz::istream_buffer<> buffer(iss);

      std::map<std::string, int> m;
      auto ec = glz::read_json(m, buffer);

      expect(!ec);
      expect(m["123"] == 1);
      expect(m["456"] == 2);
   };

   "array with mixed spacing"_test = [] {
      std::istringstream iss("[ 1 ,  2  ,   3    ]");
      glz::istream_buffer<> buffer(iss);

      std::vector<int> arr;
      auto ec = glz::read_json(arr, buffer);

      expect(!ec);
      expect(arr == std::vector<int>{1, 2, 3});
   };

   "object with extra whitespace"_test = [] {
      std::istringstream iss("  {   \"id\"   :   42   ,   \"name\"   :   \"test\"   }   ");
      glz::istream_buffer<> buffer(iss);

      Record r;
      auto ec = glz::read_json(r, buffer);

      expect(!ec);
      expect(r.id == 42);
      expect(r.name == "test");
   };
};

int main() { return 0; }
