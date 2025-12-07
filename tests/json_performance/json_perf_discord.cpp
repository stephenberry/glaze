// Discord message performance tests - split from json_performance.cpp for faster compilation
// This file contains large struct definitions that cause significant template instantiation
#include <optional>

#include "glaze/glaze.hpp"
#include "json_perf_common.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz::perf;

#ifdef NDEBUG
[[maybe_unused]] constexpr size_t iterations = 1'000'000;
#else
[[maybe_unused]] constexpr size_t iterations = 100'000;
#endif

struct icon_emoji_data
{
   std::optional<std::string> name{};
   std::nullptr_t id{nullptr};
};

struct permission_overwrite
{
   std::string allow{};
   std::string deny{};
   std::string id{};
   int64_t type{};
};

struct channel_data
{
   std::vector<permission_overwrite> permission_overwrites{};
   std::optional<std::string> last_message_id{};
   int64_t default_thread_rate_limit_per_user{};
   std::vector<std::nullptr_t> applied_tags{};
   std::vector<std::nullptr_t> recipients{};
   int64_t default_auto_archive_duration{};
   std::nullptr_t status{nullptr};
   std::string last_pin_timestamp{};
   std::nullptr_t topic{nullptr};
   int64_t rate_limit_per_user{};
   icon_emoji_data icon_emoji{};
   int64_t total_message_sent{};
   int64_t video_quality_mode{};
   std::string application_id{};
   std::string permissions{};
   int64_t message_count{};
   std::string parent_id{};
   int64_t member_count{};
   std::string owner_id{};
   std::string guild_id{};
   int64_t user_limit{};
   int64_t position{};
   std::string name{};
   std::string icon{};
   int64_t version{};
   int64_t bitrate{};
   std::string id{};
   int64_t flags{};
   int64_t type{};
   bool managed{};
   bool nsfw{};
};

struct user_data
{
   std::nullptr_t avatar_decoration_data{nullptr};
   std::optional<std::string> display_name{};
   std::optional<std::string> global_name{};
   std::optional<std::string> avatar{};
   std::nullptr_t banner{nullptr};
   std::nullptr_t locale{nullptr};
   std::string discriminator{};
   std::string user_name{};
   int64_t accent_color{};
   int64_t premium_type{};
   int64_t public_flags{};
   std::string email{};
   bool mfa_enabled{};
   std::string id{};
   int64_t flags{};
   bool verified{};
   bool system{};
   bool bot{};
};

struct member_data
{
   std::nullptr_t communication_disabled_until{nullptr};
   std::nullptr_t premium_since{nullptr};
   std::optional<std::string> nick{};
   std::nullptr_t avatar{nullptr};
   std::vector<std::string> roles{};
   std::string permissions{};
   std::string joined_at{};
   std::string guild_id{};
   user_data user{};
   int64_t flags{};
   bool pending{};
   bool deaf{};
   bool mute{};
};

struct tags_data
{
   std::nullptr_t premium_subscriber{nullptr};
   std::optional<std::string> bot_id{};
};

struct role_data
{
   std::nullptr_t unicode_emoji{nullptr};
   std::nullptr_t icon{nullptr};
   std::string permissions{};
   int64_t position{};
   std::string name{};
   bool mentionable{};
   int64_t version{};
   std::string id{};
   tags_data tags{};
   int64_t color{};
   int64_t flags{};
   bool managed{};
   bool hoist{};
};

struct guild_data
{
   std::nullptr_t latest_on_boarding_question_id{nullptr};
   std::vector<std::nullptr_t> guild_scheduled_events{};
   std::nullptr_t safety_alerts_channel_id{nullptr};
   std::nullptr_t inventory_settings{nullptr};
   std::vector<std::nullptr_t> voice_states{};
   std::nullptr_t discovery_splash{nullptr};
   std::nullptr_t vanity_url_code{nullptr};
   std::nullptr_t application_id{nullptr};
   std::nullptr_t afk_channel_id{nullptr};
   int64_t default_message_notifications{};
   int64_t max_stage_video_channel_users{};
   std::string public_updates_channel_id{};
   std::nullptr_t description{nullptr};
   std::vector<std::nullptr_t> threads{};
   std::vector<channel_data> channels{};
   int64_t premium_subscription_count{};
   int64_t approximate_presence_count{};
   std::vector<std::string> features{};
   std::vector<std::string> stickers{};
   bool premium_progress_bar_enabled{};
   std::vector<member_data> members{};
   std::nullptr_t hub_type{nullptr};
   int64_t approximate_member_count{};
   int64_t explicit_content_filter{};
   int64_t max_video_channel_users{};
   std::nullptr_t splash{nullptr};
   std::nullptr_t banner{nullptr};
   std::string system_channel_id{};
   std::string widget_channel_id{};
   std::string preferred_locale{};
   int64_t system_channel_flags{};
   std::string rules_channel_id{};
   std::vector<role_data> roles{};
   int64_t verification_level{};
   std::string permissions{};
   int64_t max_presences{};
   std::string discovery{};
   std::string joined_at{};
   int64_t member_count{};
   int64_t premium_tier{};
   std::string owner_id{};
   int64_t max_members{};
   int64_t afk_timeout{};
   bool widget_enabled{};
   std::string region{};
   int64_t nsfw_level{};
   int64_t mfa_level{};
   std::string name{};
   std::string icon{};
   bool unavailable{};
   std::string id{};
   int64_t flags{};
   bool large{};
   bool owner{};
   bool nsfw{};
   bool lazy{};
};

struct discord_message
{
   std::string t{};
   guild_data d{};
   int64_t op{};
   int64_t s{};
};

template <>
struct glz::meta<icon_emoji_data>
{
   using T = icon_emoji_data;
   static constexpr auto value = object("name", &T::name, "id", &T::id);
};

template <>
struct glz::meta<permission_overwrite>
{
   using T = permission_overwrite;
   static constexpr auto value = object("allow", &T::allow, "deny", &T::deny, "id", &T::id, "type", &T::type);
};

template <>
struct glz::meta<channel_data>
{
   using T = channel_data;
   static constexpr auto value = object(
      "permission_overwrites", &T::permission_overwrites, "last_message_id", &T::last_message_id,
      "default_thread_rate_limit_per_user", &T::default_thread_rate_limit_per_user, "applied_tags", &T::applied_tags,
      "recipients", &T::recipients, "default_auto_archive_duration", &T::default_auto_archive_duration, "status",
      &T::status, "last_pin_timestamp", &T::last_pin_timestamp, "topic", &T::topic, "rate_limit_per_user",
      &T::rate_limit_per_user, "icon_emoji", &T::icon_emoji, "total_message_sent", &T::total_message_sent,
      "video_quality_mode", &T::video_quality_mode, "application_id", &T::application_id, "permissions",
      &T::permissions, "message_count", &T::message_count, "parent_id", &T::parent_id, "member_count", &T::member_count,
      "owner_id", &T::owner_id, "guild_id", &T::guild_id, "user_limit", &T::user_limit, "position", &T::position,
      "name", &T::name, "icon", &T::icon, "version", &T::version, "bitrate", &T::bitrate, "id", &T::id, "flags",
      &T::flags, "type", &T::type, "managed", &T::managed, "nsfw", &T::nsfw);
};

template <>
struct glz::meta<user_data>
{
   using T = user_data;
   static constexpr auto value =
      object("avatar_decoration_data", &T::avatar_decoration_data, "display_name", &T::display_name, "global_name",
             &T::global_name, "avatar", &T::avatar, "banner", &T::banner, "locale", &T::locale, "discriminator",
             &T::discriminator, "user_name", &T::user_name, "accent_color", &T::accent_color, "premium_type",
             &T::premium_type, "public_flags", &T::public_flags, "email", &T::email, "mfa_enabled", &T::mfa_enabled,
             "id", &T::id, "flags", &T::flags, "verified", &T::verified, "system", &T::system, "bot", &T::bot);
};

template <>
struct glz::meta<member_data>
{
   using T = member_data;
   static constexpr auto value =
      object("communication_disabled_until", &T::communication_disabled_until, "premium_since", &T::premium_since,
             "nick", &T::nick, "avatar", &T::avatar, "roles", &T::roles, "permissions", &T::permissions, "joined_at",
             &T::joined_at, "guild_id", &T::guild_id, "user", &T::user, "flags", &T::flags, "pending", &T::pending,
             "deaf", &T::deaf, "mute", &T::mute);
};

template <>
struct glz::meta<tags_data>
{
   using T = tags_data;
   static constexpr auto value = object("premium_subscriber", &T::premium_subscriber, "bot_id", &T::bot_id);
};

template <>
struct glz::meta<role_data>
{
   using T = role_data;
   static constexpr auto value =
      object("unicode_emoji", &T::unicode_emoji, "icon", &T::icon, "permissions", &T::permissions, "position",
             &T::position, "name", &T::name, "mentionable", &T::mentionable, "version", &T::version, "id", &T::id,
             "tags", &T::tags, "color", &T::color, "flags", &T::flags, "managed", &T::managed, "hoist", &T::hoist);
};

template <>
struct glz::meta<guild_data>
{
   using T = guild_data;
   static constexpr auto value = object(
      "latest_on_boarding_question_id", &T::latest_on_boarding_question_id, "guild_scheduled_events",
      &T::guild_scheduled_events, "safety_alerts_channel_id", &T::safety_alerts_channel_id, "inventory_settings",
      &T::inventory_settings, "voice_states", &T::voice_states, "discovery_splash", &T::discovery_splash,
      "vanity_url_code", &T::vanity_url_code, "application_id", &T::application_id, "afk_channel_id",
      &T::afk_channel_id, "default_message_notifications", &T::default_message_notifications,
      "max_stage_video_channel_users", &T::max_stage_video_channel_users, "public_updates_channel_id",
      &T::public_updates_channel_id, "description", &T::description, "threads", &T::threads, "channels", &T::channels,
      "premium_subscription_count", &T::premium_subscription_count, "approximate_presence_count",
      &T::approximate_presence_count, "features", &T::features, "stickers", &T::stickers,
      "premium_progress_bar_enabled", &T::premium_progress_bar_enabled, "members", &T::members, "hub_type",
      &T::hub_type, "approximate_member_count", &T::approximate_member_count, "explicit_content_filter",
      &T::explicit_content_filter, "max_video_channel_users", &T::max_video_channel_users, "splash", &T::splash,
      "banner", &T::banner, "system_channel_id", &T::system_channel_id, "widget_channel_id", &T::widget_channel_id,
      "preferred_locale", &T::preferred_locale, "system_channel_flags", &T::system_channel_flags, "rules_channel_id",
      &T::rules_channel_id, "roles", &T::roles, "verification_level", &T::verification_level, "permissions",
      &T::permissions, "max_presences", &T::max_presences, "discovery", &T::discovery, "joined_at", &T::joined_at,
      "member_count", &T::member_count, "premium_tier", &T::premium_tier, "owner_id", &T::owner_id, "max_members",
      &T::max_members, "afk_timeout", &T::afk_timeout, "widget_enabled", &T::widget_enabled, "region", &T::region,
      "nsfw_level", &T::nsfw_level, "mfa_level", &T::mfa_level, "name", &T::name, "icon", &T::icon, "unavailable",
      &T::unavailable, "id", &T::id, "flags", &T::flags, "large", &T::large, "owner", &T::owner, "nsfw", &T::nsfw,
      "lazy", &T::lazy);
};

template <>
struct glz::meta<discord_message>
{
   using value_type = discord_message;
   static constexpr auto value =
      object("t", &value_type::t, "d", &value_type::d, "op", &value_type::op, "s", &value_type::s);
};

template <class T, auto Opts>
auto generic_tester()
{
   T obj{};

   std::string buffer{};
   expect(not glz::write_json(obj, buffer));

   auto t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read<Opts>(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
      if (glz::write_json(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   auto t1 = std::chrono::steady_clock::now();

   results r{Opts.minified ? "Glaze (.minified)" : "Glaze", "https://github.com/stephenberry/glaze", iterations};
   r.json_roundtrip = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // write performance
   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::write<Opts>(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.json_byte_length = buffer.size();
   minified_byte_length = *r.json_byte_length;
   r.json_write = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // read performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read_json(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.json_read = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // validate performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::validate_json(buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   std::cout << "validation time: " << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6
             << '\n';

   // beve write performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::write_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.binary_byte_length = buffer.size();
   r.beve_write = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // beve read performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.beve_read = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // beve round trip

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
      if (glz::write_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.beve_roundtrip = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   r.print();

   return r;
}

suite discord_test = [] { "discord"_test = [] { generic_tester<discord_message, glz::opts{}>(); }; };

int main() { return 0; }
