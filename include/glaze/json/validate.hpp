// Glaze Library
// For the license information refer to glaze.hpp

#include <string>
#include <optional>
#include <sstream>

namespace glaze
{
   namespace detail
   {
      struct SourceInfo
      {
         size_t line{};
         size_t column{};
         std::string context;
      };

      template <class Buffer>
      std::optional<SourceInfo> get_source_info(Buffer const& buffer,
                                                const std::size_t index)
      {
         if (index >= buffer.size()) {
            return std::nullopt;
         }

         const std::size_t r_index = buffer.size() - index - 1;
         const auto start = std::begin(buffer) + index;
         const auto count = std::count(std::begin(buffer), start, '\n');
         const auto rstart = std::rbegin(buffer) + r_index;
         const auto pnl = std::find(rstart, std::rend(buffer), '\n');
         const auto dist = std::distance(rstart, pnl);
         const auto nnl = std::find(start, std::end(buffer), '\n');

         std::string context{
            std::begin(buffer) +
               (pnl == std::rend(buffer) ? 0 : index - dist + 1),
            nnl};
         return SourceInfo{static_cast<std::size_t>(count + 1),
                           static_cast<std::size_t>(dist), context};
      }

      inline std::string generate_error_string(std::string const& error,
                                               SourceInfo const& info,
                                               std::string const& filename = "")
      {
         std::stringstream ss{};

         if (!filename.empty()) {
            ss << filename << ":";
         }

         ss << info.line << ":" << info.column << ": " << error << "\n";
         ss << "\t" << info.context << "\n";
         ss << "\t";

         for (std::size_t i = 1; i < info.column; i++) {
            ss << " ";
         }

         ss << "^\n";
         return ss.str();
      }
   }  // namespace detail
}  // namespace test
