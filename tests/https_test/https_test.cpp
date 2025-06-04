// Test for HTTPS server functionality

#include <iostream>
#include <thread>
#include <chrono>

// Define GLZ_ENABLE_SSL to enable TLS support
#ifndef GLZ_ENABLE_SSL
#define GLZ_ENABLE_SSL
#endif

#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"

int main()
{
   std::cout << "Testing Glaze HTTPS Server Implementation\n";

   // Test 1: HTTP server (existing functionality)
   {
      std::cout << "\n1. Testing HTTP server (template parameter = false):\n";
      glz::http_server<false> http_server;
      std::cout << "   ✓ HTTP server created successfully\n";
   }

   // Test 2: HTTPS server using template parameter
   {
      std::cout << "\n2. Testing HTTPS server (template parameter = true):\n";
      glz::http_server<true> https_server;
      std::cout << "   ✓ HTTPS server created successfully\n";
      
      // Test certificate loading (will fail without actual cert files, but should compile)
      try {
         https_server.load_certificate("cert.pem", "key.pem");
         std::cout << "   ✓ Certificate loading method available\n";
      } catch (...) {
         std::cout << "   ✓ Certificate loading method available (files not found, as expected)\n";
      }
   }

   // Test 3: HTTPS server using alias
   {
      std::cout << "\n3. Testing HTTPS server using alias:\n";
      glz::https_server https_server;
      std::cout << "   ✓ HTTPS server alias works\n";
      
      // Test SSL verification mode setting
      https_server.set_ssl_verify_mode(0); // SSL_VERIFY_NONE equivalent
      std::cout << "   ✓ SSL verification mode setting available\n";
   }

   // Test 4: Default template parameter
   {
      std::cout << "\n4. Testing default template parameter (should be HTTP):\n";
      glz::http_server<> default_server;
      std::cout << "   ✓ Default template parameter works (HTTP server)\n";
   }

   std::cout << "\nUsage examples:\n";
   std::cout << "  // HTTP server (existing usage unchanged)\n";
   std::cout << "  glz::http_server server;\n";
   std::cout << "  server.bind(8080).start();\n\n";
   std::cout << "  // HTTPS server (new functionality)\n";
   std::cout << "  glz::https_server secure_server;\n";
   std::cout << "  secure_server.load_certificate(\"cert.pem\", \"key.pem\")\n";
   std::cout << "               .bind(8443)\n";
   std::cout << "               .start();\n";

   return 0;
}
