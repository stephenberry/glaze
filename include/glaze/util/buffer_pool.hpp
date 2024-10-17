#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace glz
{
   struct buffer_pool
   {
      buffer_pool() = default;

      // Disable copy and move semantics
      buffer_pool(const buffer_pool&) = delete;
      buffer_pool& operator=(const buffer_pool&) = delete;
      buffer_pool(buffer_pool&&) = delete;
      buffer_pool& operator=(buffer_pool&&) = delete;

      // Acquire a buffer as a shared_ptr with a custom deleter
      std::shared_ptr<std::string> acquire()
      {
         std::lock_guard lock{mtx};
         if (available.empty()) {
            const auto current_size = buffers.size();
            const auto new_size = buffers.size() * 2;
            buffers.resize(new_size);
            for (size_t i = current_size; i < new_size; ++i) {
               available.emplace_back(i);
            }
         }

         const auto index = available.back();
         available.pop_back();

         // Create a shared_ptr with a custom deleter
         return std::shared_ptr<std::string>(&buffers[index],
                                             [this, index](std::string*) { this->release(index); });
      }

      size_t size() const
      {
         std::lock_guard lock{mtx};
         return buffers.size();
      }

      size_t available_size() const
      {
         std::lock_guard lock{mtx};
         return available.size();
      }

     private:
      void release(size_t index)
      {
         std::lock_guard lock{mtx};
         available.emplace_back(index);
      }

      mutable std::mutex mtx{};
      std::deque<std::string> buffers{2};
      std::vector<size_t> available{0, 1}; // indices of available buffers
   };
}
