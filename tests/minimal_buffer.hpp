// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

// Buffer types that promise no more than Glaze's buffer concepts require; see the comment on
// `concept contiguous` in glaze/concepts/container_concepts.hpp for why that matters.
//
// A writable fixture cannot be minimal: writing needs output_buffer, hence range and vector_like,
// so qt_style_buffer is forced to carry iterators and a subscript. read_only_buffer is the only
// one that pins "exactly data() and size()", so it is what catches code indexing a buffer it was
// only ever promised a pointer and a length for.
namespace test_buffers
{
   // Shaped like Qt's QByteArray: contiguous and resizable, with emptiness spelled isEmpty().
   // Deliberately omits empty(), clear(), push_back(), append() and reserve(); glaze must not
   // reach for any of them.
   struct qt_style_buffer
   {
      using value_type = char;
      using reference = char&;
      using const_reference = const char&;

      std::vector<char> bytes{};

      char& operator[](size_t i) { return bytes[i]; }
      const char& operator[](size_t i) const { return bytes[i]; }

      char* data() { return bytes.data(); }
      const char* data() const { return bytes.data(); }
      size_t size() const { return bytes.size(); }
      void resize(size_t n) { bytes.resize(n); }

      char* begin() { return bytes.data(); }
      char* end() { return bytes.data() + bytes.size(); }
      const char* begin() const { return bytes.data(); }
      const char* end() const { return bytes.data() + bytes.size(); }

      bool isEmpty() const { return bytes.empty(); }

      void assign(std::string_view sv) { bytes.assign(sv.begin(), sv.end()); }
   };

   // Exactly `contiguous` and nothing more: no subscript, no iterators, no emptiness member of any
   // spelling. Read-only, so it carries only what a parse needs to walk a span of bytes.
   struct read_only_buffer
   {
      std::vector<char> bytes{};

      explicit read_only_buffer(std::string_view sv) : bytes(sv.begin(), sv.end()) {}
      read_only_buffer() = default;

      const char* data() const { return bytes.data(); }
      size_t size() const { return bytes.size(); }
   };
}
