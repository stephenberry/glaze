// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/thread/threadpool.hpp"
#include "glaze/util/macros.hpp"

#include <random>
#include <numeric>

namespace glaze
{
   namespace study
   {
      struct param
      {
         std::string ptr{};
         std::string distribution = "";
         std::vector<raw_json> range{};
      };

      struct design
      {
         std::vector<param> params{};  //!< study parameters
         std::vector<std::unordered_map<std::string, raw_json>> states{};
         std::unordered_map<std::string, raw_json> overwrite{}; //!< pointer syntax and json representation
         std::default_random_engine::result_type seed{}; //!< Seed for randomized study
         size_t random_samples{};  //!< Number of runs to perform in randomized
                                   //!< study. If zero it will run a full
                                   //!< factorial ignoring random distributions
                                   //!< instead instead of a randomized study
      };
   }  // namespace study

   template <>
   struct meta<study::param>
   {
      using T = study::param;
      static constexpr auto value = glaze::object("id", &T::ptr, "*", &T::ptr, "dist", &T::distribution, "values", &T::range);
   };

   template <>
   struct meta<study::design>
   {
      using T = study::design;
      static constexpr auto value =
         glaze::object("params", &T::params, "states", &T::states, "overwrite", &T::overwrite,
         "seed", &T::seed, "random_samples", &T::random_samples);
   };

   namespace study {
      template <class State>
      void overwrite_state(State &state, const std::unordered_map<std::string, raw_json> &overwrites)
      {
         for (auto&& [json_ptr, raw_json_str] : overwrites) {
            glaze::write_from(state, json_ptr, raw_json_str.str);
         }
      }

      struct param_set
      {
         basic_ptr param_ptr{};
         std::vector<basic> elements{};
      };

      template <class State>
      struct full_factorial
      {
         State state{};
         std::vector<param_set> param_sets;
         std::size_t index{};
         std::size_t max_index{};

         full_factorial(State _state, const design& design)
            : state(std::move(_state))
         {
            max_index = design.params.empty() ? 0 : 1;

            overwrite_state(state, design.overwrite);

            for (auto &param : design.params) {
               param_sets.emplace_back(param_set_from_dist(param));
               auto num_elements = param_sets.back().elements.size();
               if (num_elements != 0) max_index *= num_elements;
            }
         }

         bool empty() const { return index >= max_index; }

         std::size_t size() const { return max_index; }

         const State& generate()
         {
            for (auto &param_set : param_sets) {
               const std::size_t this_size = std::max(
                  param_set.elements.size(), static_cast<std::size_t>(1));
               const std::size_t this_index = index % this_size;
               std::visit(
                  [&](auto &&param_ptr) {
                     using param_type =
                        std::remove_pointer_t<std::decay_t<decltype(param_ptr)>>;
                     
                     if (std::holds_alternative<double>(param_set.elements[this_index])) {
                        if constexpr (std::is_convertible_v<param_type, double>) {
                           *param_ptr =
                              static_cast<param_type>(std::get<double>(param_set.elements[this_index]));
                        }
                        else {
                           throw std::runtime_error("full_factorial::generate: elements type not convertible to design type");
                        }
                     }
                     else {
                        *param_ptr = std::get<param_type>(param_set.elements[this_index]);
                     }
                  },
                  param_set.param_ptr);
            }

            index++;
            return state;
         }

         param_set param_set_from_dist(const param &dist)
         {
            param_set param_set;

            bool found{};
            detail::seek_impl(
               [&](auto &&val) {
                  if constexpr (std::is_assignable_v<
                                   basic, std::decay_t<decltype(val)>>) {
                     found = true;
                     param_set.param_ptr = &val;
                  }
                  else {
                     throw std::runtime_error(
                        "Study params only support basic types like double, "
                        "int, bool, or std::string");
                  }
               },
               state, dist.ptr);
            if (!found) {
               throw std::runtime_error("Param \"" + dist.ptr +
                                        "\" doesnt exist");
            }

            if (dist.distribution == "elements") {
               std::visit(
                  [&](auto &&param_ptr) {
                     for (auto &&json : dist.range) {
                        auto elem = *param_ptr;
                        read_json(elem, json.str);
                        param_set.elements.emplace_back(elem);
                     }
                  },
                  param_set.param_ptr);
            }
            else if (dist.distribution == "linspace") {
               if (dist.range.size() != 3) {
                  throw std::runtime_error(
                     "glaze::study::full_factorial::param_set_from_dist: Linspace "
                     "distribution's range must have 3 elements!");
               }

               double start{};
               double step{};
               double stop{};

               glaze::read_json(start, dist.range[0].str);
               glaze::read_json(step, dist.range[1].str);
               glaze::read_json(stop, dist.range[2].str);

               if (start > stop) {
                  std::swap(start, stop);
               }

               for (; start <= stop; start += step) {
                  param_set.elements.emplace_back(start);
               }
            }
            else {
               throw std::runtime_error(
                  "glaze::study::full_factorial::param_set_from_dist: Unknown "
                  "distribution for non random study '"
                  + dist.distribution + "'!");
            }

            return param_set;
         }
      };

      template <class Generator>
      void run_study(Generator& g, auto&& f)
      {
         glaze::pool tpool{};
         size_t job_num = 0;
         while (!g.empty()) {
            // generate mutates
            tpool.emplace_back([=, state = g.generate()] {
               f(std::move(state), job_num);
            });
            ++job_num;
         }
         tpool.wait();
      }

      struct random_param
      {
         basic_ptr param_ptr{};
         basic value{};
         std::function<basic()> gen{};
         void apply()
         {
            std::visit(
               [&](auto&& p) {
                  *p = std::get<
                     std::remove_pointer_t<std::decay_t<decltype(p)>>>(
                     value);
               },
               param_ptr);
         }
      };

      template <class State>
      struct random_doe
      {
         State state{};

         std::default_random_engine::result_type seed{};
         size_t random_samples{};

         std::default_random_engine engine{};
         std::vector<size_t> resample_indices{};
         size_t index = 0;

         std::vector<std::vector<random_param>> params_per_state{};

         random_doe(State _state, const design &design)
            : state(std::move(_state))
         {
            overwrite_state(state, design.overwrite);

            engine.seed(seed);
            resample_indices.resize(random_samples);
            std::iota(std::begin(resample_indices), std::end(resample_indices),
                      0);

            params_per_state.resize(random_samples);
            const size_t dim = design.params.size();

            if (params_per_state.size() > 0) {
               auto &params = params_per_state.front();
               params.resize(dim);
               for (std::size_t i = 0; i < dim; i++) {
                  params[i] = param_from_dist(design.params[i]);
               }
            }

            resample(1.0);
         }

         bool empty() const { return index >= params_per_state.size(); }

         const State& generate()
         {
            auto &params = params_per_state[index++];
            for (auto &param : params) {
               param.apply();
            }

            return state;
         }

         void reset() { index = 0; }

         std::size_t size() { return params_per_state.size(); }

         void resample(double ratio)
         {
            std::shuffle(std::begin(resample_indices),
                         std::end(resample_indices), engine);
            std::size_t to_resample = static_cast<std::size_t>(
               std::ceil(ratio * params_per_state.size()));

            for (std::size_t i = 0; i < to_resample; i++) {
               auto &params = params_per_state[resample_indices[i]];
               for (auto &param : params) {
                  param.value = param.gen();
               }
            }

            reset();
         }

         random_param param_from_dist(const param &dist)
         {
            random_param result{};

            bool found{};
            detail::seek_impl(
               [&](auto &&val) {
                  if constexpr (std::is_assignable_v<
                                   basic, std::decay_t<decltype(val)>>) {
                     found = true;
                     result.param_ptr = &val;
                  }
                  else {
                     throw std::runtime_error(
                        "Study params only support basic types like double, "
                        "int, bool, or std::string");
                  }
               },
               state, dist.ptr);
            if (!found) {
               throw std::runtime_error("Param \"" + dist.ptr +
                                        "\" doesnt exist");
            }

            if (dist.distribution == "elements") {
               if (dist.range.size() == 0) {
                  throw std::runtime_error(
                     "glaze::study::random_doe::param_from_dist: Elements "
                     "distribution's cannot be empty!");
               }
               std::vector<basic> elements{};
               std::visit(
                  [&](auto &&param_ptr) {
                     for (auto &&json : dist.range) {
                        auto elem = *param_ptr;
                        read_json(elem, json.str);
                        elements.emplace_back(elem);
                     }
                  },
                  result.param_ptr);
               result.gen() =
                  [this, dist = std::uniform_int_distribution<std::size_t>(
                      0, dist.range.size() - 1),
                   elements = std::move(elements)]() {
                     std::size_t element_index = dist(this->engine);
                     return elements[element_index];
               };
            }
            else if (dist.distribution == "linspace") {
               if (dist.range.size() != 3) {
                  throw std::runtime_error(
                     "glaze::study::random_doe::param_from_dist: Linspace "
                     "distribution's range does not have 3 elements!");
               }

               double start{};
               double stop{};

               glaze::read_json(start, dist.range[0].str);
               glaze::read_json(stop, dist.range[2].str);

               if (start > stop) {
                  std::swap(start, stop);
               }

               result.gen() = [this,
                               dist = std::uniform_real_distribution<double>(start, stop)]() {
                  return dist(this->engine);
               };
            }
            else if (dist.distribution == "uniform") {
               if (dist.range.size() != 2) {
                  throw std::runtime_error(
                     "glaze::study::random_doe::param_from_dist: Uniform "
                     "distribution's range must have 2 elements!");
               }

               double start{};
               double stop{};

               glaze::read_json(start, dist.range[0].str);
               glaze::read_json(stop, dist.range[1].str);

               if (start > stop) {
                  std::swap(start, stop);
               }

               result.gen() = [this,
                               dist = std::uniform_real_distribution<double>(
                                  start, stop)]() {
                 return dist(this->engine);
               };
            }
            else if (dist.distribution == "normal") {
               if (dist.range.size() != 2) {
                  throw std::runtime_error(
                     "glaze::study::random_doe::param_from_dist: Normal "
                     "distribution's range must have 2 elements!");
               }

               double mean{};
               double std_dev{};

               glaze::read_json(mean, dist.range[0].str);
               glaze::read_json(std_dev, dist.range[1].str);

               result.gen() =
                  [this, dist = std::normal_distribution<double>(
                                        mean, std_dev)]() {
                  return dist(this->engine);
               };
            }
            else {
               throw std::runtime_error(
                  "glaze::study::random_doe::param_from_dist: Unknown "
                  "distribution");
            }
         }
      };

      struct ProgressBar
      {
         std::size_t width;
         std::size_t completed;
         std::size_t total;
         double time_taken;
      };

      inline std::ostream &operator<<(std::ostream &o, ProgressBar const &bar)
      {
         const std::size_t one = 1;
         const std::size_t total = std::max(bar.total, one);
         const std::size_t completed = std::min(bar.completed, total);
         const auto progress = static_cast<double>(completed) / total;
         const auto percentage = static_cast<size_t>(std::round(progress * 100));

         if (bar.width > 2) {
            const std::size_t len = bar.width - 2;
            const std::size_t filled =
               static_cast<unsigned int>(std::round(progress * len));

            o << "[";

            for (std::size_t i = 0; i < filled; i++) {
               o << '=';
            }

            for (std::size_t i = 0; i < len - filled; i++) {
               o << '-';
            }

            o << "] ";
         }

         const std::size_t eta_s = static_cast<std::size_t>(
            std::round(static_cast<double>(total - completed) * bar.time_taken /
                       std::max(completed, one)));
         const std::size_t minutes = eta_s / 60;
         const std::size_t seconds_remaining = eta_s - minutes * 60;
         o << std::round(percentage) << "% | ETA: " << minutes << "m"
           << seconds_remaining << "s | " << completed << "/" << total;

         return o;
      }
   }  // namespace study
}  // namespace glaze
