// Common utilities for JSON performance tests
#pragma once

#include <iostream>
#include <optional>
#include <string_view>

namespace glz::perf
{
   // We scale all speeds by the minified JSON byte length, so that libraries which do not efficiently write JSON do not
   // get an unfair advantage. We want to know how fast the libraries will serialize/deserialize with respect to one
   // another.
   inline size_t minified_byte_length{};

   struct results
   {
      std::string_view name{};
      std::string_view url{};
      size_t iterations{};

      std::optional<size_t> json_byte_length{};
      std::optional<double> json_read{};
      std::optional<double> json_write{};
      std::optional<double> json_roundtrip{};

      std::optional<size_t> binary_byte_length{};
      std::optional<double> beve_write{};
      std::optional<double> beve_read{};
      std::optional<double> beve_roundtrip{};

      void print(bool use_minified = true)
      {
         if (json_roundtrip) {
            std::cout << name << " json roundtrip: " << *json_roundtrip << " s\n";
         }

         if (json_byte_length) {
            std::cout << name << " json byte length: " << *json_byte_length << '\n';
         }

         if (json_write) {
            if (json_byte_length) {
               const auto byte_length = use_minified ? minified_byte_length : *json_byte_length;
               const auto MBs = iterations * byte_length / (*json_write * 1048576);
               std::cout << name << " json write: " << *json_write << " s, " << MBs << " MB/s\n";
            }
            else {
               std::cout << name << " json write: " << *json_write << " s\n";
            }
         }

         if (json_read) {
            if (json_byte_length) {
               const auto byte_length = use_minified ? minified_byte_length : *json_byte_length;
               const auto MBs = iterations * byte_length / (*json_read * 1048576);
               std::cout << name << " json read: " << *json_read << " s, " << MBs << " MB/s\n";
            }
            else {
               std::cout << name << " json read: " << *json_read << " s\n";
            }
         }

         if (beve_roundtrip) {
            std::cout << '\n';
            std::cout << name << " beve roundtrip: " << *beve_roundtrip << " s\n";
         }

         if (binary_byte_length) {
            std::cout << name << " beve byte length: " << *binary_byte_length << '\n';
         }

         if (beve_write) {
            if (binary_byte_length) {
               const auto MBs = iterations * *binary_byte_length / (*beve_write * 1048576);
               std::cout << name << " beve write: " << *beve_write << " s, " << MBs << " MB/s\n";
            }
            else {
               std::cout << name << " beve write: " << *beve_write << " s\n";
            }
         }

         if (beve_read) {
            if (binary_byte_length) {
               const auto MBs = iterations * *binary_byte_length / (*beve_read * 1048576);
               std::cout << name << " beve read: " << *beve_read << " s, " << MBs << " MB/s\n";
            }
            else {
               std::cout << name << " beve read: " << *beve_read << " s\n";
            }
         }

         std::cout << "\n---\n" << std::endl;
      }
   };
}
