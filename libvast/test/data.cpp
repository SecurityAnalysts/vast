//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#define SUITE data

#include "vast/data.hpp"

#include "vast/concept/convertible/to.hpp"
#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/data.hpp"
#include "vast/concept/printable/stream.hpp"
#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/data.hpp"
#include "vast/concept/printable/vast/json.hpp"
#include "vast/detail/deserialize.hpp"
#include "vast/detail/serialize.hpp"
#include "vast/test/test.hpp"

#include <caf/test/dsl.hpp>

using namespace vast;
using namespace std::chrono_literals;

TEST(list) {
  REQUIRE(std::is_same_v<std::vector<data>, list>);
}

TEST(maps) {
  map ports{{"ssh", 22u}, {"http", 80u}, {"https", 443u}, {"imaps", 993u}};
  CHECK(ports.size() == 4);
  auto i = ports.find("ssh");
  REQUIRE(i != ports.end());
  CHECK(i->second == 22u);
  i = ports.find("imaps");
  REQUIRE(i != ports.end());
  CHECK(i->second == 993u);
  CHECK(ports.emplace("telnet", 23u).second);
  CHECK(!ports.emplace("http", 8080u).second);
}

TEST(flatten) {
  // clang-format off
  auto rt = legacy_record_type{
    {"a", legacy_string_type{}},
    {"b", legacy_record_type{
      {"c", legacy_integer_type{}},
      {"d", legacy_list_type{legacy_integer_type{}}}
    }},
    {"e", legacy_record_type{
      {"f", legacy_address_type{}},
      {"g", legacy_count_type{}}
    }},
    {"h", legacy_bool_type{}}
  };
  auto xs = record{
    {"a", "foo"},
    {"b", record{
      {"c", integer{-42}},
      {"d", list{integer{1}, integer{2}, integer{3}}}
    }},
    {"e", record{
      {"f", caf::none},
      {"g", caf::none},
    }},
    {"h", true}
  };
  // clang-format on
  auto values = std::vector<data>{
    "foo",     integer{-42}, list{integer{1}, integer{2}, integer{3}},
    caf::none, caf::none,    true};
  auto r = vast::test::unbox(make_record(rt, std::vector<data>(values)));
  REQUIRE_EQUAL(r, xs);
  MESSAGE("flatten");
  auto fr = flatten(r);
  auto ftr = vast::test::unbox(flatten(r, rt));
  CHECK_EQUAL(fr, ftr);
  REQUIRE_EQUAL(fr.size(), values.size());
  CHECK_EQUAL(fr["b.c"], integer{-42});
}

TEST(merge) {
  // clang-format off
  const auto xs = record{
    {"a", "foo"},
    {"b", record{
      {"c", integer{-42}},
      {"d", list{integer{1}, integer{2}, integer{3}}}
    }},
    {"c", record{
      {"a", "bar"}
    }}
  };
  const auto ys = record{
    {"a", "bar"},
    {"b", record{
      {"a", integer{42}},
      {"d", list{integer{4}, integer{5}, integer{6}}}
    }},
    {"c", "not a record yet"}
  };
  {
    auto expected = record{
      {"a", "foo"},
      {"b", record{
        {"a", integer{42}},
        {"d", list{integer{1}, integer{2}, integer{3}}},
        {"c", integer{-42}}
      }},
      {"c", record{
        {"a", "bar"}
      }}
    };
    auto copy = ys;
    merge(xs, copy, policy::merge_lists::no);
    CHECK_EQUAL(copy, expected);
  }
  {
    auto expected = record{
      {"a", "foo"},
      {"b", record{
        {"a", integer{42}},
        {"d", list{integer{4}, integer{5}, integer{6},
                   integer{1}, integer{2}, integer{3}}},
        {"c", integer{-42}}
      }},
      {"c", record{
        {"a", "bar"}
      }}
    };
    auto copy = ys;
    merge(xs, copy, policy::merge_lists::yes);
    CHECK_EQUAL(copy, expected);
  }
  // clang-format on
}

TEST(strip) {
  auto xs = record{
    {"a", record{}},
    {"b", count{5u}},
    {"c",
     record{
       {"d",
        record{
          {"e", record{}},
          {"f", caf::none},
        }},
     }},
    {"g", caf::none},
  };
  auto expected = record{{"b", count{5u}}};
  CHECK_EQUAL(strip(xs), expected);
}

TEST(construction) {
  CHECK(caf::holds_alternative<caf::none_t>(data{}));
  CHECK(caf::holds_alternative<bool>(data{true}));
  CHECK(caf::holds_alternative<bool>(data{false}));
  CHECK(caf::holds_alternative<integer>(data{integer{0}}));
  CHECK(caf::holds_alternative<integer>(data{integer{42}}));
  CHECK(caf::holds_alternative<integer>(data{integer{-42}}));
  CHECK(caf::holds_alternative<count>(data{42u}));
  CHECK(caf::holds_alternative<real>(data{4.2}));
  CHECK(caf::holds_alternative<std::string>(data{"foo"}));
  CHECK(caf::holds_alternative<std::string>(data{std::string{"foo"}}));
  CHECK(caf::holds_alternative<pattern>(data{pattern{"foo"}}));
  CHECK(caf::holds_alternative<address>(data{address{}}));
  CHECK(caf::holds_alternative<subnet>(data{subnet{}}));
  CHECK(caf::holds_alternative<list>(data{list{}}));
  CHECK(caf::holds_alternative<map>(data{map{}}));
}

TEST(relational_operators) {
  data d1;
  data d2;
  CHECK(d1 == d2);
  CHECK(!(d1 < d2));
  CHECK(d1 <= d2);
  CHECK(d1 >= d2);
  CHECK(!(d1 > d2));

  d2 = integer{42};
  CHECK(d1 != d2);
  CHECK(d1 < d2);
  CHECK(d1 <= d2);
  CHECK(!(d1 >= d2));
  CHECK(!(d1 > d2));

  d1 = integer{42};
  d2 = caf::none;
  CHECK(d1 != d2);
  CHECK(!(d1 < d2));
  CHECK(!(d1 <= d2));
  CHECK(d1 >= d2);
  CHECK(d1 > d2);

  d2 = integer{1377};
  CHECK(d1 != d2);
  CHECK(d1 < d2);
  CHECK(d1 <= d2);
  CHECK(!(d1 >= d2));
  CHECK(!(d1 > d2));
}

TEST(evaluation) {
  MESSAGE("in");
  data lhs{"foo"};
  data rhs{"foobar"};
  CHECK(evaluate(lhs, relational_operator::in, rhs));
  CHECK(evaluate(rhs, relational_operator::not_in, lhs));
  CHECK(evaluate(rhs, relational_operator::ni, lhs));
  CHECK(evaluate(rhs, relational_operator::not_in, lhs));
  MESSAGE("equality");
  lhs = count{42};
  rhs = count{1337};
  CHECK(evaluate(lhs, relational_operator::less_equal, rhs));
  CHECK(evaluate(lhs, relational_operator::less, rhs));
  CHECK(evaluate(lhs, relational_operator::not_equal, rhs));
  CHECK(!evaluate(lhs, relational_operator::equal, rhs));
  MESSAGE("network types");
  lhs = *to<address>("10.0.0.1");
  rhs = *to<subnet>("10.0.0.0/8");
  CHECK(evaluate(lhs, relational_operator::in, rhs));
  lhs = *to<subnet>("10.0.42.0/16");
  CHECK(evaluate(lhs, relational_operator::in, rhs));
  rhs = *to<subnet>("10.0.42.0/17");
  CHECK(!evaluate(lhs, relational_operator::in, rhs));
  MESSAGE("mixed types");
  rhs = real{4.2};
  CHECK(!evaluate(lhs, relational_operator::equal, rhs));
  CHECK(evaluate(lhs, relational_operator::not_equal, rhs));
}

TEST(evaluation - pattern matching) {
  CHECK(evaluate(pattern{"f.*o"}, relational_operator::equal, "foo"));
  CHECK(evaluate("foo", relational_operator::equal, pattern{"f.*o"}));
  CHECK(evaluate("foo", relational_operator::match, pattern{"f.*o"}));
}

TEST(serialization) {
  list xs;
  xs.emplace_back(count{80});
  xs.emplace_back(count{53});
  xs.emplace_back(count{8});
  auto x0 = data{xs};
  std::vector<char> buf;
  CHECK_EQUAL(detail::serialize(buf, x0), caf::none);
  data x1;
  CHECK_EQUAL(detail::deserialize(buf, x1), caf::none);
  CHECK_EQUAL(x0, x1);
}

TEST(printable) {
  // Ensure that we don't produce trailing zeros for floating point data.
  auto x = data{-4.2};
  CHECK_EQUAL(to_string(x), "-4.2");
  x = 3.14;
  CHECK_EQUAL(to_string(x), "3.14");
}

TEST(parseable) {
  auto p = make_parser<data>();
  data d;
  MESSAGE("bool");
  auto str = "T"s;
  auto f = str.begin();
  auto l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == true);
  MESSAGE("numbers");
  str = "+1001"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK_EQUAL(f, l);
  CHECK_EQUAL(d, integer{1001});
  str = "1001"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == 1001u);
  str = "10.01"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == 10.01);
  MESSAGE("string");
  str = R"("bar")";
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == "bar");
  MESSAGE("pattern");
  str = "/foo/"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == pattern{"foo"});
  MESSAGE("address");
  str = "10.0.0.1"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == *to<address>("10.0.0.1"));
  MESSAGE("list");
  str = "[42,4.2,nil]"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == list{42u, 4.2, caf::none});
  MESSAGE("map");
  str = "{T->1,F->0}"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == map{{true, 1u}, {false, 0u}});
}

TEST(convert - caf::config_value) {
  // clang-format off
  auto x = record{
    {"x", "foo"},
    {"r", record{
      {"i", integer{-42}},
      {"u", 42u},
      {"r", record{
        {"u", 3.14}
      }},
    }},
    {"delta", 12ms},
    {"uri", "https://tenzir.com/"},
    {"xs", list{integer{1}, integer{2}, integer{3}}},
    {"ys", list{integer{1}, "foo", 3.14}},
    {"zs", list{record{{"z", true}}, map{{42u, 4.2}}}}
  };
  // clang-format on
  using namespace caf;
  auto y = config_value::dictionary{};
  y.emplace("x", "foo");
  auto r = config_value::dictionary{};
  r.emplace("i", -42);
  r.emplace("u", 42u);
  auto rr = config_value::dictionary{};
  rr.emplace("u", 3.14);
  r.emplace("r", std::move(rr));
  y.emplace("r", std::move(r));
  y.emplace("delta", timespan{12ms});
  y.emplace("uri", "https://tenzir.com/"); // maybe in the future as caf::uri
  y.emplace("xs", make_config_value_list(1, 2, 3));
  y.emplace("ys", make_config_value_list(1, "foo", 3.14));
  auto z0 = config_value::dictionary{};
  z0.emplace("z", true);
  auto z1 = config_value::dictionary{};
  z1.emplace("42", 4.2);
  y.emplace("zs", make_config_value_list(std::move(z0), std::move(z1)));
  CHECK_EQUAL(unbox(to<settings>(x)), y);
  CHECK_EQUAL(unbox(to<dictionary<config_value>>(x)), y);
}

TEST(convert - caf::config_value - null) {
  // clang-format off
  auto x = record{
    {"valid", "foo"},
    {"invalid", caf::none}
  };
  // clang-format on
  using namespace caf;
  auto y = to<dictionary<config_value>>(x);
  REQUIRE(!y.engaged());
  CHECK_EQUAL(y.error(), ec::type_clash);
  // If we flatten the record first and weed out null values, it'll work.
  auto flat = flatten(x);
  auto& [k, v] = as_vector(flat).back();
  flat.erase(k);
  y = to<dictionary<config_value>>(flat);
  REQUIRE(y.engaged());
}

// We can't really test that a given call doesn't produce a stack overflow, so
// instead we test here that the fields that are nested deeper than
// `max_recursion_depth` are cut off during `flatten()`.
TEST(nesting depth) {
  auto x = record{{"leaf", integer{1}}};
  for (size_t i = 0; i < defaults::max_recursion; ++i) {
    auto tmp = record{{"nested", std::exchange(x, {})}};
    x = tmp;
  }
  auto final = record{{"branch1", x}, {"branch2", integer{4}}};
  CHECK_EQUAL(depth(final), defaults::max_recursion + 2);
  auto flattened = flatten(final);
  CHECK_EQUAL(depth(flattened), 1ull);
}
