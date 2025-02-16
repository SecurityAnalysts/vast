//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2019 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/format/csv.hpp"

#define SUITE format

#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast.hpp"
#include "vast/test/fixtures/actor_system.hpp"
#include "vast/test/fixtures/events.hpp"
#include "vast/test/test.hpp"

#include <algorithm>

using namespace vast;
using namespace std::string_literals;

namespace {

struct fixture : fixtures::deterministic_actor_system {
  const legacy_record_type l0 = legacy_record_type{
    {"ts", legacy_time_type{}},
    {"addr", legacy_address_type{}},
    {"port", legacy_count_type{}}}.name("l0");

  const legacy_record_type l1
    = legacy_record_type{{"s", legacy_string_type{}},
                         {"ptn", legacy_pattern_type{}},
                         {"lis", legacy_list_type{legacy_count_type{}}}}
        .name("l1");

  const legacy_record_type l2 = legacy_record_type{
    {"b", legacy_bool_type{}},
    {"c", legacy_count_type{}},
    {"r", legacy_real_type{}},
    {"i", legacy_integer_type{}},
    {"s", legacy_string_type{}},
    {"a", legacy_address_type{}},
    {"sn", legacy_subnet_type{}},
    {"t", legacy_time_type{}},
    {"d", legacy_duration_type{}},
    {"d2", legacy_duration_type{}},
    {"e", legacy_enumeration_type{{"FOO", "BAR", "BAZ"}}},
    {"lc", legacy_list_type{legacy_count_type{}}},
    {"lt", legacy_list_type{legacy_time_type{}}},
    {"msa", legacy_map_type{legacy_string_type{}, legacy_address_type{}}},
    {"mcs",
     legacy_map_type{
       legacy_count_type{},
       legacy_string_type{}}}}.name("l2");

  const legacy_record_type l3 = legacy_record_type{
    {"s1", legacy_string_type{}},
    {"s2", legacy_string_type{}},
    {"s2,3",
     legacy_string_type{}}}.name("l3");

  schema s;

  fixture() {
    s.add(l0);
    s.add(l1);
    s.add(l2);
    s.add(l3);
  }

  const caf::settings options = {};

  std::vector<table_slice>
  run(std::string_view data, size_t max_events, size_t max_slice_size) {
    auto in = std::make_unique<std::istringstream>(std::string{data});
    format::csv::reader reader{options, std::move(in)};
    reader.schema(s);
    std::vector<table_slice> slices;
    auto add_slice
      = [&](table_slice slice) { slices.emplace_back(std::move(slice)); };
    auto [err, num] = reader.read(max_events, max_slice_size, add_slice);
    REQUIRE_EQUAL(err, caf::none);
    size_t lines = std::count(data.begin(), data.end(), '\n');
    REQUIRE_EQUAL(num, std::min(lines, max_events));
    return slices;
  }
};

} // namespace

FIXTURE_SCOPE(csv_reader_tests, fixture)

std::string_view l0_log0 = R"__(ts,addr,port
2011-08-12T13:00:36.349948Z,147.32.84.165,1027
2011-08-12T13:08:01.360925Z,147.32.84.165,3101
2011-08-12T13:08:01.360925Z,147.32.84.165,1029
2011-08-12T13:09:35.498887Z,147.32.84.165,1029
2011-08-12T13:14:36.012344Z,147.32.84.165,1041
2011-08-12T14:59:11.994970Z,147.32.84.165,1046
2011-08-12T14:59:12.448311Z,147.32.84.165,1047
2011-08-13T13:04:24.640406Z,147.32.84.165,1089)__";

TEST(csv reader - simple) {
  auto slices = run(l0_log0, 8, 5);
  REQUIRE_EQUAL(slices[0].layout(), l0);
  CHECK(slices[1].at(0, 0, legacy_time_type{})
        == data{unbox(to<vast::time>("2011-08-12T14:59:11.994970Z"))});
  CHECK(slices[1].at(1, 2, legacy_count_type{}) == data{count{1047}});
}

std::string_view l0_log1 = R"__(ts,addr,port
2011-08-12T13:00:36.349948Z,"147.32.84.165",1027
"2011-08-12T13:08:01.360925Z",147.32.84.165,
2011-08-12T13:08:01.360925Z,,"1029"
2011-08-12T13:09:35.498887Z,147.32.84.165,1029
2011-08-12T13:14:36.012344Z,147.32.84.165,1041
,147.32.84.165,1046
,147.32.84.165,
,,)__";

TEST(csv reader - empty fields) {
  auto slices = run(l0_log1, 8, 5);
  REQUIRE_EQUAL(slices[0].layout(), l0);
  CHECK(slices[1].at(0, 1, legacy_address_type{})
        == data{unbox(to<address>("147.32.84.165"))});
  CHECK(slices[1].at(1, 2, legacy_count_type{}) == data{caf::none});
}

std::string_view l1_log_string = R"__(s
hello
)__";

TEST(csv reader - string) {
  auto slices = run(l1_log_string, 1, 1);
  auto l1_string = legacy_record_type{{"s", legacy_string_type{}}}.name("l1");
  REQUIRE_EQUAL(slices[0].layout(), l1_string);
  CHECK(slices[0].at(0, 0, legacy_string_type{}) == data{"hello"});
}

std::string_view l1_log_pattern = R"__(ptn
hello
)__";

TEST(csv reader - pattern) {
  auto slices = run(l1_log_pattern, 1, 1);
  auto l1_pattern
    = legacy_record_type{{"ptn", legacy_pattern_type{}}}.name("l1");
  REQUIRE_EQUAL(slices[0].layout(), l1_pattern);
  CHECK(slices[0].at(0, 0, legacy_pattern_type{}) == data{pattern{"hello"}});
}

std::string_view l1_log0 = R"__(s,ptn,lis
hello,world,[1,2]
Tom,appeared,[42,1337]
on,the,[42,1337]
sidewalk,with,[42,1337]
a,bucket,[42,1337]
of,whitewash,[42,1337]
and,a,[42,1337]
long-handled,brush,[42,1337]
He,surveyed the,[42,1337]
fence,and,[42,1337]
all,gladness,[42,1337]
left,him,[42,1337]
and ,a,[42,1337]
deep,melancholy,[42,1337]
settled,down,[42,1337]
upon,his,[42,1337]
spirit,Thirty,[42,1337]
yards,of,[42,1337]
board, fence,[42,1337]
nine,feet,[42,1337]
high,Life,[42,1337]
to,him,[42,1337]
seemed,hollow,[42,1337]
and,existence,[42,1337]
but,a,[42,1337]
burden,Sighing,[42,1337]
,,)__";

TEST(csv reader - layout with container) {
  auto slices = run(l1_log0, 20, 20);
  REQUIRE_EQUAL(slices[0].layout(), l1);
  CHECK(slices[0].at(10, 1, legacy_pattern_type{})
        == data{pattern{"gladness"}});
  auto xs = vast::list{};
  xs.emplace_back(data{count{42}});
  xs.emplace_back(data{count{1337}});
  CHECK(slices[0].at(19, 2, legacy_list_type{legacy_count_type{}}) == data{xs});
}

std::string_view l1_log1 = R"__(s,ptn
hello,world
Tom,appeared
"on",the
sidewalk,"with"
a,bucket
of,whitewash
and,a
long-handled,brush
He,surveyed the
fence,and
all,gladness
left,him
and ,a
deep,melancholy
settled,down
upon,his
spirit,Thirty
yards,of
board, fence
nine,feet
high,Life
to,him
seemed,hollow
and,existence
but,a
burden,Sighing
,,)__";

TEST(csv reader - sublayout construction) {
  auto l1_sub = legacy_record_type{{"s", legacy_string_type{}},
                                   {"ptn", legacy_pattern_type{}}}
                  .name("l1");
  auto slices = run(l1_log1, 20, 20);
  REQUIRE_EQUAL(slices[0].layout(), l1_sub);
  CHECK(slices[0].at(10, 1, legacy_pattern_type{})
        == data{pattern{"gladness"}});
}

std::string_view l2_log_msa = R"__(msa
{ foo=1.2.3.4, bar=2001:db8:: })__";

TEST(csv reader - map string->address) {
  auto slices = run(l2_log_msa, 1, 1);
  auto t = legacy_map_type{legacy_string_type{}, legacy_address_type{}};
  auto l2_msa = legacy_record_type{{"msa", t}}.name("l2");
  REQUIRE_EQUAL(slices[0].layout(), l2_msa);
  auto m = vast::map{};
  m.emplace(data{"foo"}, unbox(to<address>("1.2.3.4")));
  m.emplace(data{"bar"}, unbox(to<address>("2001:db8::")));
  CHECK_EQUAL(materialize(slices[0].at(0, 0, t)), data{m});
}

std::string_view l2_log_vp = R"__(lc
[1, 2, 3, 4, 5]
[])__";

TEST(csv reader - list of count) {
  auto slices = run(l2_log_vp, 2, 100);
  auto t = legacy_list_type{legacy_count_type{}};
  auto l2_vp = legacy_record_type{{"lc", t}}.name("l2");
  REQUIRE_EQUAL(slices[0].layout(), l2_vp);
  CHECK(slices[0].at(0, 0, t) == data{list{1u, 2u, 3u, 4u, 5u}});
  CHECK(slices[0].at(1, 0, t) == data{list{}});
}

std::string_view l2_log_subnet = R"__(sn
"1.2.3.4/20"
2001:db8::/125)__";

TEST(csv reader - subnet) {
  auto slices = run(l2_log_subnet, 2, 2);
  auto l2_subnet = legacy_record_type{{"sn", legacy_subnet_type{}}}.name("l2");
  REQUIRE_EQUAL(slices[0].layout(), l2_subnet);
  CHECK(slices[0].at(0, 0, legacy_subnet_type{})
        == data{unbox(to<subnet>("1.2.3.4/20"))});
  CHECK(slices[0].at(1, 0, legacy_subnet_type{})
        == data{unbox(to<subnet>("2001:db8::/125"))});
}

std::string_view l2_log_duration = R"__(d,d2
"42s",5days)__";

TEST(csv reader - duration) {
  auto slices = run(l2_log_duration, 1, 1);
  auto l2_duration = legacy_record_type{{"d", legacy_duration_type{}},
                                        {"d2", legacy_duration_type{}}}
                       .name("l2");
  REQUIRE_EQUAL(slices[0].layout(), l2_duration);
  CHECK(slices[0].at(0, 0, legacy_duration_type{})
        == data{unbox(to<duration>("42s"))});
}

std::string_view l2_log_reord = R"__(msa, c, r, i, b,  a,  sn, d,  e,  t, lc, lt
{ foo=1.2.3.4, bar=2001:db8:: },424242,4.2,-1337,T,147.32.84.165,192.168.0.1/24,42s,BAZ,2011-08-12+14:59:11.994970,[ 5555,0],[ 2019-04-30T11:46:13Z ])__";
// FIXME: Parsing maps in csv is broken, see ch12358.
//   = R"__(msa, c, r, i, b,  a,  sn, d,  e,  t,  lc, lt, mcs
// { foo=1.2.3.4, bar=2001:db8::
// },424242,4.2,-1337,T,147.32.84.165,42/udp,192.168.0.1/24,42s,BAZ,2011-08-12+14:59:11.994970,
// [ 5555/tcp, 0/icmp ],[ 2019-04-30T11:46:13Z ],{ 1=FOO, 1024=BAR! })__";

TEST(csv reader - reordered layout) {
  auto slices = run(l2_log_reord, 1, 1);
  auto l2_sub = legacy_record_type{{"msa", legacy_map_type{legacy_string_type{}, legacy_address_type{}}},
                            {"c", legacy_count_type{}},
                            {"r", legacy_real_type{}},
                            {"i", legacy_integer_type{}},
                            {"b", legacy_bool_type{}},
                            {"a", legacy_address_type{}},
                            {"sn", legacy_subnet_type{}},
                            {"d", legacy_duration_type{}},
                            {"e", legacy_enumeration_type{{"FOO", "BAR", "BAZ"}}},
                            {"t", legacy_time_type{}},
                            {"lc", legacy_list_type{legacy_count_type{}}},
                            {"lt", legacy_list_type{legacy_time_type{}}},
                            // FIXME: Parsing maps in csv is broken, see ch12358.
                            // {"mcs", legacy_map_type{legacy_count_type{}, legacy_string_type{}}}
                            }
                  .name("l2");
  REQUIRE_EQUAL(slices[0].layout(), l2_sub);
  CHECK(slices[0].at(0, 0, l2_sub.at("msa")->type)
        == data{map{{data{"foo"}, unbox(to<address>("1.2.3.4"))},
                    {data{"bar"}, unbox(to<address>("2001:db8::"))}}});
  CHECK(slices[0].at(0, 1, l2_sub.at("c")->type) == data{count{424242}});
  CHECK(slices[0].at(0, 2, l2_sub.at("r")->type) == data{real{4.2}});
  CHECK(slices[0].at(0, 3, l2_sub.at("i")->type) == data{integer{-1337}});
  CHECK(slices[0].at(0, 4, l2_sub.at("b")->type) == data{true});
  CHECK(slices[0].at(0, 5, l2_sub.at("a")->type)
        == data{unbox(to<address>("147.32.84.165"))});
  CHECK(slices[0].at(0, 6, l2_sub.at("sn")->type)
        == data{unbox(to<subnet>("192.168.0.1/24"))});
  CHECK(slices[0].at(0, 7, l2_sub.at("d")->type)
        == data{unbox(to<duration>("42s"))});
  CHECK(slices[0].at(0, 8, l2_sub.at("e")->type) == data{enumeration{2}});
  CHECK(slices[0].at(0, 9, l2_sub.at("t")->type)
        == data{unbox(to<vast::time>("2011-08-12+14:59:11.994970"))});
  CHECK(slices[0].at(0, 10, l2_sub.at("lc")->type) == data{list{5555u, 0u}});
  CHECK(slices[0].at(0, 11, l2_sub.at("lt")->type)
        == data{list{unbox(to<vast::time>("2019-04-30T11:46:13Z"))}});
  auto m = map{};
  m[1u] = data{"FOO"};
  m[1024u] = data{"BAR!"};
  // FIXME: Parsing maps in csv is broken, see ch12358.
  // CHECK_EQUAL(materialize(slices[0].at(0, 14)), data{m});
}

std::string_view l2_line_endings = "d,d2\r\n42s,5days\n10s,1days\r\n";

TEST(csv reader - line endings) {
  auto slices = run(l2_line_endings, 2, 2);
  auto l2_duration = legacy_record_type{{"d", legacy_duration_type{}},
                                        {"d2", legacy_duration_type{}}}
                       .name("l2");
  REQUIRE_EQUAL(slices[0].layout(), l2_duration);
  CHECK(slices[0].at(0, 0, legacy_duration_type{})
        == data{unbox(to<duration>("42s"))});
  CHECK(slices[0].at(0, 1, legacy_duration_type{})
        == data{unbox(to<duration>("5days"))});
  CHECK(slices[0].at(1, 0, legacy_duration_type{})
        == data{unbox(to<duration>("10s"))});
  CHECK(slices[0].at(1, 1, legacy_duration_type{})
        == data{unbox(to<duration>("1days"))});
}

// Below are strings that extensively test quoting and escaping for string
// fields and column names. For other field types, other tests above have
// quoted field sprinkled all over them.

std::string_view l3_quoted_strings_header = R"__(s1,"s2,3"
a,b
c,d)__";

TEST(csv reader - quoted strings in header) {
  auto slices = run(l3_quoted_strings_header, 2, 2);
  auto l3_strings = legacy_record_type{{"s1", legacy_string_type{}},
                                       {"s2,3", legacy_string_type{}}}
                      .name("l3");
  REQUIRE_EQUAL(slices[0].layout(), l3_strings);
  CHECK(slices[0].at(0, 0, legacy_string_type{}) == data{"a"});
  CHECK(slices[0].at(0, 1, legacy_string_type{}) == data{"b"});
  CHECK(slices[0].at(1, 0, legacy_string_type{}) == data{"c"});
  CHECK(slices[0].at(1, 1, legacy_string_type{}) == data{"d"});
}

std::string_view l3_quoted_strings_1 = R"__(s1
"hello, world")__";

std::string_view l3_quoted_strings_2 = R"__(s1,s2
a,"b,c"
"d,e,\"f",\"g)__";

TEST(csv reader - quoted string) {
  {
    auto slices = run(l3_quoted_strings_1, 1, 1);
    auto l3_strings
      = legacy_record_type{{"s1", legacy_string_type{}}}.name("l3");
    REQUIRE_EQUAL(slices[0].layout(), l3_strings);
    CHECK(slices[0].at(0, 0, legacy_string_type{}) == data{"hello, world"});
  }
  {
    auto slices = run(l3_quoted_strings_2, 2, 2);
    auto l3_strings = legacy_record_type{{"s1", legacy_string_type{}},
                                         {"s2", legacy_string_type{}}}
                        .name("l3");
    REQUIRE_EQUAL(slices[0].layout(), l3_strings);
    CHECK(slices[0].at(0, 0, legacy_string_type{}) == data{"a"});
    CHECK(slices[0].at(0, 1, legacy_string_type{}) == data{"b,c"});
    CHECK(slices[0].at(1, 0, legacy_string_type{}) == data{"d,e,\"f"});
    CHECK(slices[0].at(1, 1, legacy_string_type{}) == data{"\\\"g"});
  }
}

FIXTURE_SCOPE_END()
