#include <array>
#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <string>
#include <vector>

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   // use a vector with null termination instead of a std::string to avoid
   // small string optimization to hide bounds problems
   std::vector<char> buffer{Data, Data + Size};
   buffer.push_back('\0');

   [[maybe_unused]] auto s = glz::read_jsonc<my_struct>(std::string_view{buffer.data(), Size});
   if (s) {
      // hooray! valid json found
   }

   // A valid document has to survive the formatters unchanged. Two independent comment scanners run
   // here -- skip_comment on the read path and read_jsonc_comment in the writers -- and a
   // disagreement between them is invisible to either side on its own: the reader accepted comment
   // shapes the writer truncated, and the writer emitted shapes the reader could not read back.
   //
   // Gated on validate_jsonc rather than on the read, because a read stops at the first complete
   // value and says nothing about what trails it. Nothing is promised about a document that does not
   // validate: minifying is free to run tokens together that only whitespace separated, which is
   // what dropping a line comment does to "8 // c" followed by "6".
   {
      std::vector<char> read_buffer{Data, Data + Size};
      read_buffer.push_back('\0');
      const std::string_view document{read_buffer.data(), Size};
      glz::generic value{};
      if (not glz::validate_jsonc(document) && not glz::read_jsonc(value, document)) {
         std::string original{};
         if (not glz::write_json(value, original)) {
            auto round_trips = [&](const std::string& formatted) {
               glz::generic reparsed{};
               if (glz::read_jsonc(reparsed, formatted)) {
                  return false;
               }
               std::string rewritten{};
               if (glz::write_json(reparsed, rewritten)) {
                  return false;
               }
               return rewritten == original;
            };

            std::string minify_in{Data, Data + Size};
            std::string minified{};
            if (not glz::minify_jsonc(minify_in, minified)) {
               if (not round_trips(minified)) {
                  __builtin_trap();
               }
            }

            const std::string prettify_in{Data, Data + Size};
            std::string prettified{};
            if (not glz::prettify_jsonc(prettify_in, prettified)) {
               if (not round_trips(prettified)) {
                  __builtin_trap();
               }
            }
         }
      }
   }

   return 0;
}
