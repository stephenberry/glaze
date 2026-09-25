// Glaze Library
// For the license information refer to glaze.hpp
// P2996 Reflection Test - Verifies C++26 P2996 reflection works with Glaze

#include <cstdint>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct TestStruct
{
   std::string name;
   int value;
   double data;
};

// C-style array in struct - requires P2996 reflection for automatic serialization (issue #1839)
struct SingleArrayStruct
{
   int data[10];
};

struct MixedArrayStruct
{
   uint16_t ints[2];
   float floats[3];
   std::string name;
};

struct NestedWithArray
{
   std::string label;
   double values[4];
   int count;
};

// Array of structs in a struct
struct Point
{
   double x;
   double y;
};

struct Polygon
{
   Point vertices[4];
   std::string name;
};

// Struct containing a struct with an array
struct Inner
{
   int values[3];
};

struct Outer
{
   Inner inner;
   std::string tag;
};

// Multi-dimensional C-style array
struct Matrix
{
   int data[2][3];
};

// Deeply nested: array of structs that contain arrays
struct Segment
{
   double coords[2];
};

struct Path
{
   Segment segments[3];
   int id;
};

// Test enum WITHOUT any glz::meta - used for reflect_enums option test
enum class Direction { North, South, East, West };

// Custom opts with reflect_enums enabled
struct reflect_enums_opts : glz::opts
{
   bool reflect_enums = true;
};

suite p2996_reflection = [] {
   "member_names"_test = [] {
      constexpr auto names = glz::member_names<TestStruct>;
      expect(names.size() == 3);
      expect(names[0] == "name");
      expect(names[1] == "value");
      expect(names[2] == "data");
   };

   "json round-trip"_test = [] {
      TestStruct obj{"test", 42, 3.14};
      auto json = glz::write_json(obj).value_or("error");

      TestStruct obj2{};
      expect(!glz::read_json(obj2, json));
      expect(obj2.name == "test");
      expect(obj2.value == 42);
   };
};

suite p2996_enums = [] {
   "enum_to_string"_test = [] {
      expect(glz::enum_to_string(Direction::North) == "North");
      expect(glz::enum_to_string(Direction::South) == "South");
      expect(glz::enum_to_string(Direction::East) == "East");
      expect(glz::enum_to_string(Direction::West) == "West");
   };

   "string_to_enum"_test = [] {
      expect(glz::string_to_enum<Direction>("North") == Direction::North);
      expect(glz::string_to_enum<Direction>("South") == Direction::South);
      expect(glz::string_to_enum<Direction>("East") == Direction::East);
      expect(glz::string_to_enum<Direction>("West") == Direction::West);
      expect(glz::string_to_enum<Direction>("Invalid") == std::nullopt);
   };

   "reflect_enums option"_test = [] {
      Direction d = Direction::East;

      std::string dir_json;
      expect(not glz::write<reflect_enums_opts{}>(d, dir_json));
      expect(dir_json == "\"East\"");

      Direction d2{};
      expect(not glz::read<reflect_enums_opts{}>(d2, dir_json));
      expect(d2 == Direction::East);

      expect(glz::write<reflect_enums_opts{}>(static_cast<Direction>(7), dir_json) == glz::error_code::unexpected_enum);
   };

   // The reflective enum-by-name reader scans the quoted key directly (not via skip_ws), so on a
   // non-null-terminated buffer (no trailing '\0' sentinel) it must respect end. Each input below
   // is read over an exact-size buffer. A key missing its closing quote must error rather than
   // scan past the end: with the unbounded scan, "North" (no closing quote) would read past the
   // buffer and wrongly succeed, so expecting an error here pins the bounded behavior. Under the
   // ASAN CI configuration this additionally catches the over-read itself.
   "reflect_enums non-null-terminated bounds"_test = [] {
      static constexpr auto options = [] {
         reflect_enums_opts o{};
         o.null_terminated = false;
         return o;
      }();

      for (const std::string_view s : {"\"North", "\"Sou", "\""}) {
         std::vector<char> buf{s.begin(), s.end()};
         Direction d{};
         const auto ec = glz::read<options>(d, std::string_view{buf.data(), buf.data() + buf.size()});
         expect(bool(ec)) << "unterminated reflective enum key should error";
      }

      // A complete value still reads correctly over an exact-size buffer.
      const std::string_view complete = "\"West\"";
      std::vector<char> buf{complete.begin(), complete.end()};
      Direction d{};
      const auto ec = glz::read<options>(d, std::string_view{buf.data(), buf.data() + buf.size()});
      expect(not ec);
      expect(d == Direction::West);
   };
};

// C-style array in struct tests (issue #1839)
// With P2996 reflection, structs containing C-style arrays can be
// automatically reflected without requiring glz::meta specialization.
suite c_style_array_reflection = [] {
   "single array member reflection"_test = [] {
      constexpr auto names = glz::member_names<SingleArrayStruct>;
      expect(names.size() == 1);
      expect(names[0] == "data");
   };

   "single array member json round-trip"_test = [] {
      SingleArrayStruct obj{};
      for (int i = 0; i < 10; ++i) {
         obj.data[i] = i * 10;
      }

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"data":[0,10,20,30,40,50,60,70,80,90]})") << s;

      SingleArrayStruct obj2{};
      expect(not glz::read_json(obj2, s));
      for (int i = 0; i < 10; ++i) {
         expect(obj2.data[i] == i * 10);
      }
   };

   "mixed arrays and regular members"_test = [] {
      MixedArrayStruct obj{};
      obj.ints[0] = 1;
      obj.ints[1] = 2;
      obj.floats[0] = 1.5f;
      obj.floats[1] = 2.5f;
      obj.floats[2] = 3.5f;
      obj.name = "test";

      std::string s{};
      expect(not glz::write_json(obj, s));

      MixedArrayStruct obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.ints[0] == 1);
      expect(obj2.ints[1] == 2);
      expect(obj2.floats[0] == 1.5f);
      expect(obj2.floats[1] == 2.5f);
      expect(obj2.floats[2] == 3.5f);
      expect(obj2.name == "test");
   };

   "array member among scalar members"_test = [] {
      NestedWithArray obj{};
      obj.label = "sensor";
      obj.values[0] = 1.1;
      obj.values[1] = 2.2;
      obj.values[2] = 3.3;
      obj.values[3] = 4.4;
      obj.count = 42;

      std::string s{};
      expect(not glz::write_json(obj, s));

      NestedWithArray obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.label == "sensor");
      expect(obj2.count == 42);
      expect(obj2.values[0] == 1.1);
      expect(obj2.values[1] == 2.2);
      expect(obj2.values[2] == 3.3);
      expect(obj2.values[3] == 4.4);
   };

   "single array member beve round-trip"_test = [] {
      SingleArrayStruct obj{};
      for (int i = 0; i < 10; ++i) {
         obj.data[i] = i + 100;
      }

      std::string s{};
      expect(not glz::write_beve(obj, s));

      SingleArrayStruct obj2{};
      expect(not glz::read_beve(obj2, s));
      for (int i = 0; i < 10; ++i) {
         expect(obj2.data[i] == i + 100);
      }
   };
};

suite c_style_array_of_structs = [] {
   "array of structs"_test = [] {
      Polygon obj{};
      obj.vertices[0] = {0.0, 0.0};
      obj.vertices[1] = {1.0, 0.0};
      obj.vertices[2] = {1.0, 1.0};
      obj.vertices[3] = {0.0, 1.0};
      obj.name = "square";

      std::string s{};
      expect(not glz::write_json(obj, s));

      Polygon obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.name == "square");
      expect(obj2.vertices[0].x == 0.0);
      expect(obj2.vertices[0].y == 0.0);
      expect(obj2.vertices[1].x == 1.0);
      expect(obj2.vertices[2].y == 1.0);
      expect(obj2.vertices[3].x == 0.0);
      expect(obj2.vertices[3].y == 1.0);
   };

   "nested struct containing array"_test = [] {
      Outer obj{};
      obj.inner.values[0] = 10;
      obj.inner.values[1] = 20;
      obj.inner.values[2] = 30;
      obj.tag = "outer";

      std::string s{};
      expect(not glz::write_json(obj, s));

      Outer obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.tag == "outer");
      expect(obj2.inner.values[0] == 10);
      expect(obj2.inner.values[1] == 20);
      expect(obj2.inner.values[2] == 30);
   };

   "multi-dimensional array"_test = [] {
      Matrix obj{};
      obj.data[0][0] = 1;
      obj.data[0][1] = 2;
      obj.data[0][2] = 3;
      obj.data[1][0] = 4;
      obj.data[1][1] = 5;
      obj.data[1][2] = 6;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"data":[[1,2,3],[4,5,6]]})") << s;

      Matrix obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.data[0][0] == 1);
      expect(obj2.data[0][2] == 3);
      expect(obj2.data[1][0] == 4);
      expect(obj2.data[1][2] == 6);
   };

   "array of structs containing arrays"_test = [] {
      Path obj{};
      obj.segments[0].coords[0] = 0.0;
      obj.segments[0].coords[1] = 0.0;
      obj.segments[1].coords[0] = 1.0;
      obj.segments[1].coords[1] = 2.0;
      obj.segments[2].coords[0] = 3.0;
      obj.segments[2].coords[1] = 4.0;
      obj.id = 99;

      std::string s{};
      expect(not glz::write_json(obj, s));

      Path obj2{};
      expect(not glz::read_json(obj2, s));
      expect(obj2.id == 99);
      expect(obj2.segments[0].coords[0] == 0.0);
      expect(obj2.segments[0].coords[1] == 0.0);
      expect(obj2.segments[1].coords[0] == 1.0);
      expect(obj2.segments[1].coords[1] == 2.0);
      expect(obj2.segments[2].coords[0] == 3.0);
      expect(obj2.segments[2].coords[1] == 4.0);
   };

   "array of structs beve round-trip"_test = [] {
      Path obj{};
      obj.segments[0].coords[0] = 1.5;
      obj.segments[0].coords[1] = 2.5;
      obj.segments[1].coords[0] = 3.5;
      obj.segments[1].coords[1] = 4.5;
      obj.segments[2].coords[0] = 5.5;
      obj.segments[2].coords[1] = 6.5;
      obj.id = 42;

      std::string s{};
      expect(not glz::write_beve(obj, s));

      Path obj2{};
      expect(not glz::read_beve(obj2, s));
      expect(obj2.id == 42);
      expect(obj2.segments[0].coords[0] == 1.5);
      expect(obj2.segments[1].coords[1] == 4.5);
      expect(obj2.segments[2].coords[0] == 5.5);
   };
};

// reflect_array marks a type as a positional array without enumerating its members
struct PositionalPoint
{
   double x{};
   double y{};
   std::string label{};
};

template <>
struct glz::meta<PositionalPoint>
{
   static constexpr auto value = glz::reflect_array{};
};

static_assert(glz::glaze_array_t<PositionalPoint>);

suite p2996_reflect_array = [] {
   "reflect_array json round-trip"_test = [] {
      std::vector<PositionalPoint> values{};
      std::string buffer = R"([[1,2,"a"],[3,4,"b"]])";
      expect(!glz::read_json(values, buffer));
      expect(values.size() == 2);
      expect(values[1].y == 4.0);
      expect(values[1].label == "b");

      auto written = glz::write_json(values).value_or("error");
      expect(written == buffer) << written;
   };

   "reflect_array beve round-trip"_test = [] {
      PositionalPoint obj{1.5, 2.5, "origin"};
      std::string s{};
      expect(not glz::write_beve(obj, s));

      PositionalPoint obj2{};
      expect(not glz::read_beve(obj2, s));
      expect(obj2.x == 1.5);
      expect(obj2.y == 2.5);
      expect(obj2.label == "origin");
   };
};

// Inherited members (issue #2852): the reflection lists the members of base classes as well,
// base members first, so a derived type round-trips its whole state without a glz::meta.
struct InheritBase
{
   std::string name;
   int id{};
};

struct InheritDerived : InheritBase
{
   std::string extra;
};

struct InheritSecond : InheritDerived
{
   double factor{};
};

// A virtual base is one shared subobject, so its members appear once however many paths reach it
struct RootMember
{
   int root{};
};

struct LeftBranch : virtual RootMember
{
   int left{};
};

struct RightBranch : virtual RootMember
{
   int right{};
};

struct VirtualDiamond : LeftBranch, RightBranch
{
   int bottom{};
};

// A private base and a private member inside it are reachable through unchecked access
class PrivateBaseMembers
{
   int hidden{1};

  public:
   int shown{2};
};

struct PrivateBaseDerived : private PrivateBaseMembers
{
   int own{3};
};

// A polymorphic base contributes its data members and not the vtable pointer
struct PolyBase
{
   virtual ~PolyBase() = default;
   int pv{5};
};

struct PolyDerived : PolyBase
{
   int pd{6};
};

// An empty base holds no members and must not shift the indices of the members that exist
struct EmptyBaseMembers
{};

struct WithEmptyBase : EmptyBaseMembers
{
   int value{};
};

// A member that hides a member of a base class is a member of its own, so a glz::meta is how the
// two are given a key each, which is the escape the documentation describes
struct ShadowedBase
{
   int x{1};
};

struct ShadowedDerived : ShadowedBase
{
   int x{2};
};

template <>
struct glz::meta<ShadowedDerived>
{
   using T = ShadowedDerived;
   static constexpr auto value = glz::object("base_x", &ShadowedBase::x, "x", &T::x);
};

// A glz::meta written for a base class is not consulted for a derived type that is reflected
// automatically: the reflection reads the base's data members, so the base keeps its renamed key for
// itself and the derived type writes the member's own name
struct RenamedBase
{
   int raw{1};
};
template <>
struct glz::meta<RenamedBase>
{
   static constexpr auto value = glz::object("renamed", &RenamedBase::raw);
};

struct RenamedBaseDerived : RenamedBase
{
   int extra{2};
};

// A base inherited twice non-virtually has two subobjects, which the reflection refuses to name, so
// the meta reaches each of them through a lambda: &DiamondLeft::root is an int DiamondRoot::*, and
// applying that to the type with two DiamondRoot subobjects is what the compiler calls ambiguous
struct DiamondRoot
{
   int root{1};
};

struct DiamondLeft : DiamondRoot
{
   int left{2};
};

struct DiamondRight : DiamondRoot
{
   int right{3};
};

struct DiamondMixed : DiamondLeft, DiamondRight
{
   int bottom{4};
};

template <>
struct glz::meta<DiamondMixed>
{
   using T = DiamondMixed;
   static constexpr auto value =
      glz::object("root_via_left", [](auto& self) -> auto& { return self.DiamondLeft::root; }, "left",
                  [](auto& self) -> auto& { return self.left; }, "root_via_right",
                  [](auto& self) -> auto& { return self.DiamondRight::root; }, "right",
                  [](auto& self) -> auto& { return self.right; }, "bottom",
                  [](auto& self) -> auto& { return self.bottom; });
};

// glz::modify layers on top of the inherited members, because it augments pure reflection rather than
// replacing it
struct ModifiedBase
{
   int inherited{1};
};

struct ModifiedDerived : ModifiedBase
{
   int own{2};
};

template <>
struct glz::meta<ModifiedDerived>
{
   using T = ModifiedDerived;
   static constexpr auto modify = glz::object("renamed_own", &T::own);
};

// A base member and the member that hides it share a name, so a modify entry that names the member by
// name alone would bind whichever comes first. The pointer carries the class the member was declared
// in, and that is what says which of the two the entry means
struct HidingBase
{
   int x{1};
};

struct HidingDerived : HidingBase
{
   int x{2};
};

template <>
struct glz::meta<HidingDerived>
{
   static constexpr auto modify = glz::object("base_x", &HidingBase::x, "own_x", &HidingDerived::x);
};

// Two bases that repeat a name, with an entry for each: both name the same string, and only the
// declaring class tells the entries apart
struct RepeatingLeft
{
   int id{1};
};

struct RepeatingRight
{
   int id{2};
};

struct RepeatingBoth : RepeatingLeft, RepeatingRight
{
   int own{3};
};

template <>
struct glz::meta<RepeatingBoth>
{
   static constexpr auto modify = glz::object("left_id", &RepeatingLeft::id, "right_id", &RepeatingRight::id);
};

// The array-shaped writes take their element count from the same member list, so they carry the
// inherited members too
struct ArrayBase
{
   int first{1};
};

struct ArrayDerived : ArrayBase
{
   int second{2};
};

template <>
struct glz::meta<ArrayDerived>
{
   static constexpr auto value = glz::reflect_array{};
};

// An empty base repeated non-virtually has two subobjects as well, but neither holds a member, so the
// type reflects exactly once and stays reflectable
struct EmptyTag
{};

struct TaggedLeft : EmptyTag
{
   int left{1};
};

struct TaggedRight : EmptyTag
{
   int right{2};
};

struct TaggedMixed : TaggedLeft, TaggedRight
{
   int bottom{3};
};

// A base inherited twice non-virtually that does hold members cannot be reflected at all, and the
// count asks before it hands out a number. These three shapes pin the verdict, so a compiler that
// does not ship the query the walk needs fails to build here instead of losing the check quietly.
struct UnnamableRoot
{
   int root{1};
};

struct UnnamableLeft : UnnamableRoot
{
   int left{2};
};

struct UnnamableRight : UnnamableRoot
{
   int right{3};
};

struct UnnamableDiamond : UnnamableLeft, UnnamableRight
{
   int bottom{4};
};

// A repeated base that holds nothing itself still duplicates a member it inherits non-virtually, and
// a repeated base whose members all come from a virtual base of it reflects them once
struct NestedRoot
{
   int nested{1};
};

struct NestedMid : NestedRoot
{};

struct NestedLeft : NestedMid
{
   int left{2};
};

struct NestedRight : NestedMid
{
   int right{3};
};

struct NestedDiamond : NestedLeft, NestedRight
{
   int bottom{4};
};

struct SharedRoot
{
   int shared{1};
};

struct SharedMid : virtual SharedRoot
{};

struct SharedLeft : SharedMid
{
   int left{2};
};

struct SharedRight : SharedMid
{
   int right{3};
};

struct SharedDiamond : SharedLeft, SharedRight
{
   int bottom{4};
};

// one path to the root is virtual and one is not, so the second subobject exists just the same, in
// either order of the bases. The shape is odd enough that a compiler warns about the ambiguity it
// creates, which is the case being pinned
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winaccessible-base"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winaccessible-base"
#endif
struct EdgeRoot
{
   int root{1};
};

struct EdgeVirtual : virtual EdgeRoot
{
   int virt{2};
};

struct EdgeVirtualFirst : EdgeVirtual, EdgeRoot
{
   int bottom{3};
};

struct EdgePlainFirst : EdgeRoot, EdgeVirtual
{
   int bottom{3};
};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

suite p2996_inherited_members = [] {
   "member names include inherited members, base first"_test = [] {
      constexpr auto names = glz::member_names<InheritSecond>;
      // a count is pinned at compile time, so a wrong one fails the build rather than a later index
      static_assert(names.size() == 4);
      expect(names[0] == "name");
      expect(names[1] == "id");
      expect(names[2] == "extra");
      expect(names[3] == "factor");
      static_assert(glz::reflect<InheritSecond>::size == 4);
   };

   "inherited members round-trip through JSON"_test = [] {
      InheritSecond obj{};
      obj.name = "base";
      obj.id = 7;
      obj.extra = "derived";
      obj.factor = 2.5;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"name":"base","id":7,"extra":"derived","factor":2.5})") << s;

      InheritSecond back{};
      expect(not glz::read_json(back, s));
      expect(back.name == "base");
      expect(back.id == 7);
      expect(back.extra == "derived");
      expect(back.factor == 2.5);
   };

   "inherited members round-trip through BEVE"_test = [] {
      InheritSecond obj{};
      obj.name = "beve";
      obj.id = 11;
      obj.extra = "fields";
      obj.factor = 0.5;

      std::string s{};
      expect(not glz::write_beve(obj, s));

      InheritSecond back{};
      expect(not glz::read_beve(back, s));
      expect(back.name == "beve");
      expect(back.id == 11);
      expect(back.extra == "fields");
      expect(back.factor == 0.5);
   };

   "a virtual base contributes its members once"_test = [] {
      constexpr auto names = glz::member_names<VirtualDiamond>;
      static_assert(names.size() == 4);
      expect(names[0] == "root");
      expect(names[1] == "left");
      expect(names[2] == "right");
      expect(names[3] == "bottom");

      VirtualDiamond obj{};
      obj.root = 1;
      obj.left = 2;
      obj.right = 3;
      obj.bottom = 4;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"root":1,"left":2,"right":3,"bottom":4})") << s;

      VirtualDiamond back{};
      expect(not glz::read_json(back, s));
      expect(back.root == 1);
      expect(back.left == 2);
      expect(back.right == 3);
      expect(back.bottom == 4);
   };

   "members of a private base are reflected and read back"_test = [] {
      constexpr auto names = glz::member_names<PrivateBaseDerived>;
      static_assert(names.size() == 3);
      expect(names[0] == "hidden");
      expect(names[1] == "shown");
      expect(names[2] == "own");

      PrivateBaseDerived obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"hidden":1,"shown":2,"own":3})") << s;

      // the private members cannot be read here, so the read is pinned by what the object writes
      // back: the document carries values the defaults do not have
      PrivateBaseDerived back{};
      expect(not glz::read_json(back, R"({"hidden":8,"shown":9,"own":7})"));
      expect(back.own == 7);
      std::string s2{};
      expect(not glz::write_json(back, s2));
      expect(s2 == R"({"hidden":8,"shown":9,"own":7})") << s2;
   };

   "a polymorphic base contributes its members, not the vtable"_test = [] {
      constexpr auto names = glz::member_names<PolyDerived>;
      static_assert(names.size() == 2);
      expect(names[0] == "pv");
      expect(names[1] == "pd");

      PolyDerived obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"pv":5,"pd":6})") << s;

      PolyDerived back{};
      expect(not glz::read_json(back, s));
      expect(back.pv == 5);
      expect(back.pd == 6);
   };

   "a glz::meta gives a hidden member its own key"_test = [] {
      std::string s{};
      expect(not glz::write_json(ShadowedDerived{}, s));
      expect(s == R"({"base_x":1,"x":2})") << s;

      ShadowedDerived back{};
      expect(not glz::read_json(back, s));
      expect(back.ShadowedBase::x == 1);
      expect(back.x == 2);
   };

   "an empty base contributes no members"_test = [] {
      constexpr auto names = glz::member_names<WithEmptyBase>;
      static_assert(names.size() == 1);
      expect(names[0] == "value");

      WithEmptyBase obj{};
      obj.value = 9;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"value":9})") << s;

      WithEmptyBase back{};
      expect(not glz::read_json(back, s));
      expect(back.value == 9);
   };

   "a base's glz::meta is not consulted for the derived type"_test = [] {
      RenamedBase base{};
      base.raw = 7;

      std::string base_json{};
      expect(not glz::write_json(base, base_json));
      expect(base_json == R"({"renamed":7})") << base_json;

      RenamedBaseDerived derived{};
      derived.raw = 8;
      derived.extra = 9;

      constexpr auto names = glz::member_names<RenamedBaseDerived>;
      static_assert(names.size() == 2);
      expect(names[0] == "raw");
      expect(names[1] == "extra");

      std::string derived_json{};
      expect(not glz::write_json(derived, derived_json));
      expect(derived_json == R"({"raw":8,"extra":9})") << derived_json;
   };

   "a glz::meta names the two subobjects of a repeated base"_test = [] {
      DiamondMixed obj{};
      obj.DiamondLeft::root = 5;
      obj.left = 6;
      obj.DiamondRight::root = 7;
      obj.right = 8;
      obj.bottom = 9;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"root_via_left":5,"left":6,"root_via_right":7,"right":8,"bottom":9})") << s;

      DiamondMixed back{};
      expect(not glz::read_json(back, s));
      expect(back.DiamondLeft::root == 5);
      expect(back.left == 6);
      expect(back.DiamondRight::root == 7);
      expect(back.right == 8);
      expect(back.bottom == 9);
   };

   "glz::modify layers on top of the inherited members"_test = [] {
      ModifiedDerived obj{};
      obj.inherited = 5;
      obj.own = 6;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"inherited":5,"renamed_own":6})") << s;

      ModifiedDerived back{};
      expect(not glz::read_json(back, s));
      expect(back.inherited == 5);
      expect(back.own == 6);
   };

   "a modify entry reaches the member its pointer names"_test = [] {
      // The name alone cannot say which member is meant here, because two of them answer to it: the
      // entry used to bind to the first, rename that slot and leave the member it names to be written
      // again under its own key, which dropped one of the two values from the document
      RepeatingBoth obj{};
      obj.RepeatingLeft::id = 11;
      obj.RepeatingRight::id = 22;
      obj.own = 33;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"left_id":11,"right_id":22,"own":33})") << s;

      RepeatingBoth back{};
      expect(not glz::read_json(back, s));
      expect(back.RepeatingLeft::id == 11);
      expect(back.RepeatingRight::id == 22);
      expect(back.own == 33);
   };

   "a modify entry names the member that hides a base member"_test = [] {
      HidingDerived obj{};
      obj.HidingBase::x = 41;
      obj.x = 42;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"base_x":41,"own_x":42})") << s;

      HidingDerived back{};
      expect(not glz::read_json(back, s));
      expect(back.HidingBase::x == 41);
      expect(back.x == 42);
   };

   "the array-shaped writes carry the inherited members"_test = [] {
      constexpr auto as_arrays = glz::opt_true<glz::opts{.format = glz::BEVE}, glz::structs_as_arrays_opt_tag{}>;

      // an automatically reflected type writes one element per member of the hierarchy
      InheritSecond auto_reflected{};
      auto_reflected.name = "array";
      auto_reflected.id = 21;
      auto_reflected.extra = "elements";
      auto_reflected.factor = 2.0;

      std::string auto_beve{};
      expect(not glz::write<as_arrays>(auto_reflected, auto_beve));

      InheritSecond auto_back{};
      expect(not glz::read<as_arrays>(auto_back, auto_beve));
      expect(auto_back.name == "array");
      expect(auto_back.id == 21);
      expect(auto_back.extra == "elements");
      expect(auto_back.factor == 2.0);

      // glz::reflect_array takes its element count from the same member list, so the base member is
      // one of the two elements
      static_assert(glz::detail::count_members<ArrayDerived> == 2);

      ArrayDerived reflected{};
      reflected.first = 12;
      reflected.second = 13;

      std::string beve{};
      expect(not glz::write<as_arrays>(reflected, beve));

      ArrayDerived reflected_back{};
      expect(not glz::read<as_arrays>(reflected_back, beve));
      expect(reflected_back.first == 12);
      expect(reflected_back.second == 13);
   };

   "an empty base repeated non-virtually reflects nothing twice"_test = [] {
      constexpr auto names = glz::member_names<TaggedMixed>;
      static_assert(names.size() == 3);
      expect(names[0] == "left");
      expect(names[1] == "right");
      expect(names[2] == "bottom");

      TaggedMixed obj{};
      obj.left = 4;
      obj.right = 5;
      obj.bottom = 6;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"left":4,"right":5,"bottom":6})") << s;

      TaggedMixed back{};
      expect(not glz::read_json(back, s));
      expect(back.left == 4);
      expect(back.right == 5);
      expect(back.bottom == 6);
   };

   "the reflection refuses a repeated base that holds members"_test = [] {
      // the verdicts are pinned here because `count_members` consumes them in a static_assert, so the
      // refusal itself cannot be exercised as a positive test: a compiler that lost the query the walk
      // needs would let every one of these through, and the negative pins below fail the build instead
      static_assert(glz::detail::members_are_namable(^^VirtualDiamond));
      static_assert(glz::detail::members_are_namable(^^TaggedMixed));
      static_assert(glz::detail::members_are_namable(^^SharedDiamond));
      static_assert(not glz::detail::members_are_namable(^^UnnamableDiamond));
      static_assert(not glz::detail::members_are_namable(^^NestedDiamond));
      static_assert(not glz::detail::members_are_namable(^^EdgeVirtualFirst));
      static_assert(not glz::detail::members_are_namable(^^EdgePlainFirst));

      expect(not glz::detail::members_are_namable(^^UnnamableDiamond))
         << "a second subobject of a non-virtual base holds members the compiler cannot name";
      expect(not glz::detail::members_are_namable(^^NestedDiamond))
         << "the repeated base holds nothing itself, but it inherits a member non-virtually";
      expect(not glz::detail::members_are_namable(^^EdgeVirtualFirst))
         << "a non-virtual edge to a base another edge reaches virtually is a second subobject";
      expect(not glz::detail::members_are_namable(^^EdgePlainFirst))
         << "and the same holds with the bases written the other way round";
      expect(glz::detail::members_are_namable(^^SharedDiamond))
         << "members that all come from a virtual base of the repeated base are reflected once";
   };

   "members of a virtual base of a repeated base are reflected once"_test = [] {
      constexpr auto names = glz::member_names<SharedDiamond>;
      static_assert(names.size() == 4);
      expect(names[0] == "shared");
      expect(names[1] == "left");
      expect(names[2] == "right");
      expect(names[3] == "bottom");

      SharedDiamond obj{};
      obj.shared = 11;
      obj.left = 12;
      obj.right = 13;
      obj.bottom = 14;

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"shared":11,"left":12,"right":13,"bottom":14})") << s;

      SharedDiamond back{};
      expect(not glz::read_json(back, s));
      expect(back.shared == 11);
      expect(back.left == 12);
      expect(back.right == 13);
      expect(back.bottom == 14);
   };
};

int main() {}
