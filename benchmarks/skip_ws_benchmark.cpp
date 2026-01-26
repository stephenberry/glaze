// Benchmark for skip_ws optimization (PR #2269)
// Compares minified vs prettified JSON read performance

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"

#include "glaze/json.hpp"

#include <random>
#include <string>
#include <vector>

struct TestObj
{
   int64_t id{};
   double value{};
   std::string name{};
   bool active{};
   std::vector<int> scores{};
};

struct Root
{
   std::vector<TestObj> data{};
};

static Root generate_data(size_t count)
{
   Root root;
   root.data.reserve(count);

   std::mt19937 rng(42);
   std::uniform_int_distribution<int> id_dist(1, 1000000);
   std::uniform_real_distribution<double> val_dist(0.0, 1000.0);
   std::uniform_int_distribution<int> bool_dist(0, 1);
   std::uniform_int_distribution<int> score_dist(0, 100);
   std::uniform_int_distribution<int> score_len_dist(1, 8);

   for (size_t i = 0; i < count; ++i) {
      TestObj s;
      s.id = id_dist(rng);
      s.value = val_dist(rng);
      s.active = bool_dist(rng) != 0;
      s.name = "user_" + std::to_string(i);

      int scores_len = score_len_dist(rng);
      s.scores.reserve(scores_len);
      for (int j = 0; j < scores_len; ++j) {
         s.scores.push_back(score_dist(rng));
      }
      root.data.push_back(std::move(s));
   }
   return root;
}

int main()
{
   constexpr size_t N = 10000;

   const Root original = generate_data(N);

   // Generate minified JSON
   std::string minified;
   glz::write_json(original, minified);

   // Generate prettified JSON
   std::string prettified;
   glz::write<glz::opts{.prettify = true}>(original, prettified);

   std::printf("Minified size:   %.2f MB\n", minified.size() / (1024.0 * 1024.0));
   std::printf("Prettified size: %.2f MB\n", prettified.size() / (1024.0 * 1024.0));
   std::printf("Items: %zu\n\n", N);

   // Minified read benchmark
   {
      bencher::stage stage;
      stage.name = "Minified JSON Read";

      stage.run("glz::read_json (minified)", [&] {
         Root dest;
         auto ec = glz::read_json(dest, minified);
         if (ec) std::abort();
         return minified.size();
      });

      bencher::print_results(stage);
   }

   // Prettified read benchmark
   {
      bencher::stage stage;
      stage.name = "Prettified JSON Read";

      stage.run("glz::read_json (prettified)", [&] {
         Root dest;
         auto ec = glz::read_json(dest, prettified);
         if (ec) std::abort();
         return prettified.size();
      });

      bencher::print_results(stage);
   }

   // Combined comparison
   {
      bencher::stage stage;
      stage.name = "Minified vs Prettified Read";

      stage.run("minified", [&] {
         Root dest;
         auto ec = glz::read_json(dest, minified);
         if (ec) std::abort();
         return minified.size();
      });

      stage.run("prettified", [&] {
         Root dest;
         auto ec = glz::read_json(dest, prettified);
         if (ec) std::abort();
         return prettified.size();
      });

      bencher::print_results(stage);
   }

   return 0;
}
