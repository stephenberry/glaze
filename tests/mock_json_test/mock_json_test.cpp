// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/compare/approx.hpp"
#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

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
      expect(std::filesystem::exists(CURRENT_DIRECTORY "/json/fantasy_nations.json"))
         << "File doesn't exist: " CURRENT_DIRECTORY "/json/fantasy_nations.json";
      expect(!ec) << (glz::format_error(ec, buffer) + (" at: " CURRENT_DIRECTORY));
      std::string s = glz::write_json(v).value_or("error");
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
      std::string s = glz::write_json(v).value_or("error");
      // expect(glz::buffer_to_file(s, CURRENT_DIRECTORY "/json/stock_trades_out.json") == glz::error_code::none);
      std::string original{};
      expect(glz::file_to_buffer(original, CURRENT_DIRECTORY "/json/stock_trades.json") == glz::error_code::none);
      expect(s == original);
   };
};

struct url_t
{
   std::optional<std::vector<std::shared_ptr<url_t>>> urls{};
   std::string url{};
   std::string expanded_url{};
   std::string display_url{};
   std::vector<uint64_t> indices{};
};

struct metadata_t
{
   std::string result_type{};
   std::string iso_language_code{};
};

struct description_t
{
   std::vector<url_t> urls{};
};

struct entities_t
{
   description_t description{};
   std::optional<url_t> url{};
};

struct user_t
{
   uint64_t id{};
   std::string id_str{};
   std::string name{};
   std::string screen_name{};
   std::string location{};
   std::string description{};
   std::optional<std::string> url{};
   entities_t entities{};
   bool protected_ = false;
   uint64_t followers_count{};
   uint64_t friends_count{};
   uint64_t listed_count{};
   std::string created_at{};
   uint64_t favourites_count{};
   std::optional<int64_t> utc_offset{};
   std::optional<std::string> time_zone{};
   bool geo_enabled{};
   bool verified{};
   uint64_t statuses_count{};
   std::string lang{};
   bool contributors_enabled{};
   bool is_translator{};
   bool is_translation_enabled{};
   std::string profile_background_color{};
   std::string profile_background_image_url{};
   std::string profile_background_image_url_https{};
   bool profile_background_tile{};
   std::string profile_image_url{};
   std::string profile_image_url_https{};
   std::string profile_banner_url{};
   std::string profile_link_color{};
   std::string profile_sidebar_border_color{};
   std::string profile_sidebar_fill_color{};
   std::string profile_text_color{};
   bool profile_use_background_image{};
   bool default_profile{};
   bool default_profile_image{};
   bool following{};
   bool follow_request_sent{};
   bool notifications{};
};

template <>
struct glz::meta<user_t>
{
   using T = user_t;
   static constexpr auto value =
      object(&T::id, &T::id_str, &T::name, &T::screen_name, &T::location, &T::description, &T::url, &T::entities,
             "protected", &T::protected_, &T::followers_count, &T::friends_count, &T::listed_count, &T::created_at,
             &T::favourites_count, &T::utc_offset, &T::time_zone, &T::geo_enabled, &T::verified, &T::statuses_count,
             &T::lang, &T::contributors_enabled, &T::is_translator, &T::is_translation_enabled,
             &T::profile_background_color, &T::profile_background_image_url, &T::profile_background_image_url_https,
             &T::profile_background_tile, &T::profile_image_url, &T::profile_image_url_https, &T::profile_banner_url,
             &T::profile_link_color, &T::profile_sidebar_border_color, &T::profile_sidebar_fill_color,
             &T::profile_text_color, &T::profile_use_background_image, &T::default_profile, &T::default_profile_image,
             &T::following, &T::follow_request_sent, &T::notifications);
};

struct user_mention_t
{
   std::string screen_name{};
   std::string name{};
   uint64_t id{};
   std::string id_str{};
   std::vector<uint64_t> indices;
};

struct size_type
{
   uint32_t w{};
   uint32_t h{};
   std::string resize{};
};

struct sizes_type
{
   size_type medium{};
   size_type small{};
   size_type thumb{};
   size_type large{};
};

struct media_t
{
   uint64_t id{};
   std::string id_str{};
   std::vector<uint64_t> indices;
   std::string media_url{};
   std::string media_url_https{};
   std::string url{};
   std::string display_url{};
   std::string expanded_url{};
   std::string type{};
   sizes_type sizes{};
   uint64_t source_status_id{};
   std::string source_status_id_str{};
};

struct hashtag_t
{
   std::string text{};
   std::vector<uint64_t> indices{};
};

struct status_entities_t
{
   std::vector<hashtag_t> hashtags{};
   std::vector<std::string> symbols{};
   std::vector<url_t> urls{};
   std::vector<user_mention_t> user_mentions{};
   std::optional<std::vector<media_t>> media{};
};

struct retweeted_status_t
{
   metadata_t metadata{};
};

struct status_t
{
   metadata_t metadata{};
   std::string created_at{};
   uint64_t id{};
   std::string id_str{};
   std::string text{};
   std::string source{};
   bool truncated{};
   std::optional<uint64_t> in_reply_to_status_id{};
   std::optional<std::string> in_reply_to_status_id_str{};
   std::optional<uint64_t> in_reply_to_user_id{};
   std::optional<std::string> in_reply_to_user_id_str{};
   std::optional<std::string> in_reply_to_screen_name{};
   user_t user{};
   std::optional<uint64_t> geo{};
   std::optional<std::array<double, 2>> coordinates{};
   std::optional<std::string> place{};
   std::optional<std::string> contributors{};
   std::shared_ptr<status_t> retweeted_status{};
   uint64_t retweet_count{};
   uint64_t favorite_count{};
   status_entities_t entities{};
   bool favorited{};
   bool retweeted{};
   bool possibly_sensitive{};
   std::string lang{};
};

struct search_metadata_t
{
   double completed_in{};
   uint64_t max_id{};
   std::string max_id_str{};
   std::string next_results{};
   std::string query{};
   std::string refresh_url{};
   uint64_t count{};
   uint64_t since_id{};
   std::string since_id_str{};
};

struct twitter_t
{
   std::vector<status_t> statuses;
   search_metadata_t search_metadata{};
};

suite twitter_test = [] {
   "twitter"_test = [] {
      twitter_t v{};
      std::string buffer{};
      auto ec = glz::read_file_json(v, CURRENT_DIRECTORY "/json/twitter.json", buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      std::string s = glz::write_json(v).value_or("error");
      // expect(glz::buffer_to_file(s, CURRENT_DIRECTORY "/json/twitter_out.json") == glz::error_code::none);
      std::string original{};
      expect(glz::file_to_buffer(original, CURRENT_DIRECTORY "/json/twitter.json") == glz::error_code::none);
      expect(s == original);
   };
};

int main() { return 0; }
