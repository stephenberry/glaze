// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "glaze/glaze.hpp"

#include "boost/ut.hpp"
#include "glaze/compare/approx.hpp"

using namespace boost::ut;

struct fantasy_nation_t
{
   std::string country_name{};
   uint64_t population{};
   std::string ruler{};
   std::string government_type{};
   std::string currency{};
   std::string language{};
   std::string national_anthem{};
   std::string national_flag_color{};
   std::string national_motto{};
   std::string national_animal{};
   std::string national_flower{};
   std::string national_food{};
   std::string national_hero{};
   std::string national_sport{};
   std::string national_monument{};
   std::string climate{};
   std::string terrain{};
   std::string national_holiday{};
   std::string national_dish{};
   std::string national_drink{};
   std::string national_dress{};
   std::string national_music_genre{};
   std::string national_art_form{};
   std::string national_festival{};
   std::string national_legend{};
   std::string national_currency_symbol{};
   std::string national_architecture_style{};
};

suite fantasy_nations = [] {
   "fantasy_nations"_test = [] {
      std::vector<fantasy_nation_t> v{};
      std::string buffer{};
      auto ec = glz::read_file_json(v, CURRENT_DIRECTORY "/json/fantasy_nations.json", buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      std::string s = glz::write_json(v);
      std::string original{};
      expect(glz::file_to_buffer(original, CURRENT_DIRECTORY "/json/fantasy_nations.json") == glz::error_code::none);
      expect(s == original);
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
