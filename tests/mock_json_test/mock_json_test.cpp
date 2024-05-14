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
      expect(std::filesystem::exists(CURRENT_DIRECTORY)) << "Directory doesn't exist: " CURRENT_DIRECTORY;
      expect(std::filesystem::exists(CURRENT_DIRECTORY "/json/fantasy_nations.json")) << "File doesn't exist: " CURRENT_DIRECTORY "/json/fantasy_nations.json";
      expect(!ec) << (glz::format_error(ec, buffer) + (" at: " CURRENT_DIRECTORY));
      std::string s = glz::write_json(v);
      std::string original{};
      expect(glz::file_to_buffer(original, CURRENT_DIRECTORY "/json/fantasy_nations.json") == glz::error_code::none);
      expect(s == original);
   };
};

struct stock_trade_t
{
   uint64_t trade_id{};
   std::string stock_symbol{};
   uint64_t quantity{};
   double purchase_price{};
   double sale_price{};
   std::string purchase_date{};
   std::string sale_date{};
   double profit{};
   double brokerage_fee{};
   double total_cost{};
   double total_revenue{};
   double profit_margin{};
   std::string trade_type{};
   std::string sector{};
   std::string industry{};
};

suite stock_trades = [] {
   "stock_trades"_test = [] {
      std::vector<stock_trade_t> v{};
      std::string buffer{};
      auto ec = glz::read_file_json(v, CURRENT_DIRECTORY "/json/stock_trades.json", buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      std::string s = glz::write_json(v);
      //expect(glz::buffer_to_file(s, CURRENT_DIRECTORY "/json/stock_trades_out.json") == glz::error_code::none);
      std::string original{};
      expect(glz::file_to_buffer(original, CURRENT_DIRECTORY "/json/stock_trades.json") == glz::error_code::none);
      expect(s == original);
   };
};


int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
