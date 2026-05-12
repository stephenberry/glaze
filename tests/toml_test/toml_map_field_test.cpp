// Standalone regression test for issue #2540.
//
// Before this fix, glaze's TOML struct writer treated a ``writable_map_t``
// struct field as a scalar (PASS 1), emitting ``items = `` and then handing
// off to the map writer which produced bare ``key = value\n`` lines without
// table-header wrapping.  The two writers composed into invalid TOML that
// did not round-trip:
//
//     items = bar = false
//     foo = true
//
// The fix routes map-typed struct fields through PASS 2 as TOML tables.
//
// This test is intentionally kept small and standalone so it can run quickly
// in isolation -- the full ``toml_test`` suite has unrelated long-running
// cases that mask iteration on this issue.

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/toml.hpp"

namespace
{
   struct Item
   {
      bool enabled;
   };

   struct Cfg1
   {
      std::map<std::string, Item> items;
   };

   struct Cfg2
   {
      std::map<std::string, bool> items;
   };

   struct Cfg3
   {
      std::map<std::string, std::vector<int>> items;
   };

   struct Cfg4
   {
      std::map<std::string, Item> items;
   };

   int failures = 0;

   void check(bool cond, std::string_view label, std::string_view detail = "")
   {
      if (cond) {
         std::cout << "PASS  " << label << "\n";
      }
      else {
         std::cout << "FAIL  " << label;
         if (!detail.empty()) {
            std::cout << "\n      " << detail;
         }
         std::cout << "\n";
         ++failures;
      }
   }

   bool contains(std::string_view haystack, std::string_view needle) { return haystack.find(needle) != std::string_view::npos; }
}

template <>
struct glz::meta<Item>
{
   using T = Item;
   static constexpr auto value = object("enabled", &T::enabled);
};

template <>
struct glz::meta<Cfg1>
{
   using T = Cfg1;
   static constexpr auto value = object("items", &T::items);
};

template <>
struct glz::meta<Cfg2>
{
   using T = Cfg2;
   static constexpr auto value = object("items", &T::items);
};

template <>
struct glz::meta<Cfg3>
{
   using T = Cfg3;
   static constexpr auto value = object("items", &T::items);
};

template <>
struct glz::meta<Cfg4>
{
   using T = Cfg4;
   static constexpr auto value = object("items", &T::items);
};

int main()
{
   // --- Case 1: map<string, struct> as a struct field -----------------
   {
      Cfg1 c{.items = {{"foo", {true}}, {"bar", {false}}}};
      std::string buf;
      const auto ec = glz::write_toml(c, buf);
      check(!ec, "Case 1 write returns no error");
      // Expected: ``items = { foo = { ... }, bar = { ... } }`` (inline table).
      check(contains(buf, "items = { "), "Case 1 opens inline table on `items = { `", buf);
      check(contains(buf, "foo = {"), "Case 1 nests struct value inline", buf);
      check(contains(buf, "enabled = true"), "Case 1 emits enabled = true", buf);
      check(contains(buf, "enabled = false"), "Case 1 emits enabled = false", buf);
      check(contains(buf, " }"), "Case 1 closes inline table", buf);
      // Must NOT emit the broken pre-fix scalar form.
      check(!contains(buf, "items = bar = "), "Case 1 does not emit invalid `items = bar = ...`", buf);

      // Roundtrip
      Cfg1 r{};
      const auto rec = glz::read_toml(r, buf);
      check(!rec, "Case 1 read parses the emitted output");
      check(r.items.size() == 2, "Case 1 read recovers two entries");
      check(r.items["foo"].enabled == true, "Case 1 foo.enabled == true");
      check(r.items["bar"].enabled == false, "Case 1 bar.enabled == false");
   }

   // --- Case 2: map<string, bool> as a struct field -------------------
   {
      Cfg2 c{.items = {{"foo", true}, {"bar", false}}};
      std::string buf;
      const auto ec = glz::write_toml(c, buf);
      check(!ec, "Case 2 write returns no error");
      check(contains(buf, "items = { "), "Case 2 opens inline table on `items = { `", buf);
      check(contains(buf, "foo = true"), "Case 2 emits foo = true", buf);
      check(contains(buf, "bar = false"), "Case 2 emits bar = false", buf);
      check(contains(buf, " }"), "Case 2 closes inline table", buf);
      check(!contains(buf, "items = bar = "), "Case 2 does not emit invalid `items = bar = ...`", buf);

      Cfg2 r{};
      const auto rec = glz::read_toml(r, buf);
      check(!rec, "Case 2 read parses the emitted output");
      check(r.items.size() == 2, "Case 2 read recovers two entries");
      check(r.items["foo"] == true, "Case 2 foo == true");
      check(r.items["bar"] == false, "Case 2 bar == false");
   }

   // --- Case 3: map<string, vector<int>> as a struct field ------------
   {
      Cfg3 c{.items = {{"foo", {1, 2}}, {"bar", {3}}}};
      std::string buf;
      const auto ec = glz::write_toml(c, buf);
      check(!ec, "Case 3 write returns no error");
      check(contains(buf, "items = { "), "Case 3 opens inline table on `items = { `", buf);
      check(contains(buf, "foo = ["), "Case 3 emits foo entry as inline array", buf);
      check(contains(buf, "bar = ["), "Case 3 emits bar entry as inline array", buf);

      Cfg3 r{};
      const auto rec = glz::read_toml(r, buf);
      check(!rec, "Case 3 read parses the emitted output");
      check(r.items.size() == 2, "Case 3 read recovers two entries");
      check(r.items["foo"] == std::vector<int>({1, 2}), "Case 3 foo == {1,2}");
      check(r.items["bar"] == std::vector<int>({3}), "Case 3 bar == {3}");
   }

   // --- Case 4: empty map<string, struct> field -----------------------
   {
      Cfg4 c{};
      std::string buf;
      const auto ec = glz::write_toml(c, buf);
      check(!ec, "Case 4 (empty map) write returns no error");
      // Empty form should be ``items = {}`` to keep TOML valid.
      check(contains(buf, "items = {}"), "Case 4 emits `items = {}`", buf);

      Cfg4 r{};
      const auto rec = glz::read_toml(r, buf);
      check(!rec, "Case 4 read parses `items = {}`");
      check(r.items.empty(), "Case 4 read recovers empty map");
   }

   if (failures == 0) {
      std::cout << "\nALL OK\n";
      return 0;
   }
   std::cout << "\n" << failures << " FAILURES\n";
   return 1;
}
