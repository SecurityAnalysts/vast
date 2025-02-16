//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2019 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "fixtures/table_slices.hpp"

#include "vast/chunk.hpp"
#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/data.hpp"
#include "vast/detail/append.hpp"
#include "vast/format/test.hpp"
#include "vast/value_index.hpp"
#include "vast/value_index_factory.hpp"

#include <span>

using namespace std::string_literals;
using namespace vast;

namespace vast {

/// Constructs table slices filled with random content for testing purposes.
/// @param num_slices The number of table slices to generate.
/// @param slice_size The number of rows per table slices.
/// @param layout The layout of the table slice.
/// @param offset The offset of the first table slize.
/// @param seed The seed value for initializing the random-number generator.
/// @returns a list of randomnly filled table slices or an error.
/// @relates table_slice
caf::expected<std::vector<table_slice>>
make_random_table_slices(size_t num_slices, size_t slice_size,
                         legacy_record_type layout, id offset, size_t seed) {
  schema sc;
  sc.add(layout);
  // We have no access to the actor system, so we can only pick the default
  // table slice type here. This ignores any user-defined overrides. However,
  // this function is only meant for testing anyways.
  caf::settings opts;
  caf::put(opts, "vast.import.test.seed", seed);
  caf::put(opts, "vast.import.max-events", std::numeric_limits<size_t>::max());
  format::test::reader src{std::move(opts), nullptr};
  src.schema(std::move(sc));
  std::vector<table_slice> result;
  auto add_slice = [&](table_slice slice) {
    slice.offset(offset);
    offset += slice.rows();
    result.emplace_back(std::move(slice));
  };
  result.reserve(num_slices);
  if (auto err = src.read(num_slices * slice_size, slice_size, add_slice).first)
    return err;
  return result;
}

/// Converts the table slice into a 2-D matrix in row-major order such that
/// each row represents an event.
/// @param slice The table slice to convert.
/// @param first_row An offset to the first row to consider.
/// @param num_rows Then number of rows to consider. (0 = all rows)
/// @returns a 2-D matrix of data instances corresponding to *slice*.
/// @requires first_row < slice.rows()
/// @requires num_rows <= slice.rows() - first_row
/// @note This function exists primarily for unit testing because it performs
/// excessive memory allocations.
std::vector<std::vector<data>>
make_data(const table_slice& slice, size_t first_row, size_t num_rows) {
  VAST_ASSERT(first_row < slice.rows());
  VAST_ASSERT(num_rows <= slice.rows() - first_row);
  if (num_rows == 0)
    num_rows = slice.rows() - first_row;
  std::vector<std::vector<data>> result;
  result.reserve(num_rows);
  auto fl = flatten(slice.layout());
  for (size_t i = 0; i < num_rows; ++i) {
    std::vector<data> xs;
    xs.reserve(slice.columns());
    for (size_t j = 0; j < slice.columns(); ++j)
      xs.emplace_back(
        materialize(slice.at(first_row + i, j, fl.fields[j].type)));
    result.push_back(std::move(xs));
  }
  return result;
}

std::vector<std::vector<data>>
make_data(const std::vector<table_slice>& slices) {
  std::vector<std::vector<data>> result;
  result.reserve(rows(slices));
  for (auto& slice : slices)
    detail::append(result, make_data(slice));
  return result;
}

} // namespace vast

namespace fixtures {

table_slices::table_slices() {
  // Define our test layout.
  layout = legacy_record_type{
    {"a", legacy_bool_type{}},
    {"b", legacy_integer_type{}},
    {"c", legacy_count_type{}},
    {"d", legacy_real_type{}},
    {"e", legacy_duration_type{}},
    {"f", legacy_time_type{}},
    {"g", legacy_string_type{}},
    {"h", legacy_pattern_type{}},
    {"i", legacy_address_type{}},
    {"j", legacy_subnet_type{}},
    {"l", legacy_list_type{legacy_count_type{}}},
    {"n", legacy_map_type{legacy_count_type{}, legacy_bool_type{}}},
    // test_lists
    {"va", legacy_list_type{legacy_bool_type{}}},
    {"vb", legacy_list_type{legacy_integer_type{}}},
    {"vc", legacy_list_type{legacy_count_type{}}},
    {"vd", legacy_list_type{legacy_real_type{}}},
    {"ve", legacy_list_type{legacy_duration_type{}}},
    {"vf", legacy_list_type{legacy_time_type{}}},
    {"vg", legacy_list_type{legacy_string_type{}}},
    {"vh", legacy_list_type{legacy_pattern_type{}}},
    {"vi", legacy_list_type{legacy_address_type{}}},
    {"vj", legacy_list_type{legacy_subnet_type{}}},
    // {"vl", legacy_list_type{legacy_list_type{legacy_count_type{}}}},
    // {"vm", legacy_list_type{legacy_map_type{legacy_count_type{}, legacy_bool_type{}}}},
    // -- test_maps_left
    {"maa", legacy_map_type{legacy_bool_type{}, legacy_bool_type{}}},
    {"mba", legacy_map_type{legacy_integer_type{}, legacy_bool_type{}}},
    {"mca", legacy_map_type{legacy_count_type{}, legacy_bool_type{}}},
    {"mda", legacy_map_type{legacy_real_type{}, legacy_bool_type{}}},
    {"mea", legacy_map_type{legacy_duration_type{}, legacy_bool_type{}}},
    {"mfa", legacy_map_type{legacy_time_type{}, legacy_bool_type{}}},
    {"mga", legacy_map_type{legacy_string_type{}, legacy_bool_type{}}},
    {"mha", legacy_map_type{legacy_pattern_type{}, legacy_bool_type{}}},
    {"mia", legacy_map_type{legacy_address_type{}, legacy_bool_type{}}},
    {"mja", legacy_map_type{legacy_subnet_type{}, legacy_bool_type{}}},
    // {"mla", legacy_map_type{legacy_list_type{legacy_count_type{}}, legacy_bool_type{}}},
    // {"mna", legacy_map_type{legacy_map_type{legacy_count_type{}, legacy_bool_type{}}, legacy_bool_type{}}},
    // -- test_maps_right (intentionally no maa)
    {"mab", legacy_map_type{legacy_bool_type{}, legacy_integer_type{}}},
    {"mac", legacy_map_type{legacy_bool_type{}, legacy_count_type{}}},
    {"mad", legacy_map_type{legacy_bool_type{}, legacy_real_type{}}},
    {"mae", legacy_map_type{legacy_bool_type{}, legacy_duration_type{}}},
    {"maf", legacy_map_type{legacy_bool_type{}, legacy_time_type{}}},
    {"mag", legacy_map_type{legacy_bool_type{}, legacy_string_type{}}},
    {"mah", legacy_map_type{legacy_bool_type{}, legacy_pattern_type{}}},
    {"mai", legacy_map_type{legacy_bool_type{}, legacy_address_type{}}},
    {"maj", legacy_map_type{legacy_bool_type{}, legacy_subnet_type{}}},
    // {"mal", legacy_map_type{legacy_bool_type{}, legacy_list_type{legacy_count_type{}}}},
    // {"man", legacy_map_type{legacy_bool_type{}, legacy_map_type{legacy_count_type{}, legacy_bool_type{}}}},
	{"aas", legacy_alias_type{legacy_alias_type{legacy_string_type{}}}},
  }.name("test");
  // A bunch of test data for nested type combinations.
  // clang-format off
  auto test_lists = ""s
    + ", [T]"s // va
    + ", [+7]"s // vb
    + ", [42]"s // vc
    + ", [4.2]"s // vd
    + ", [1337ms]"s // ve
    + ", [2018-12-24]"s // vf
    + ", [\"foo\"]"s // vg
    + ", [/foo.*bar/]"s // vh
    + ", [127.0.0.1]"s // vi
    + ", [10.0.0.0/8]"s // vj
    // + ", [[1, 2, 3]]"s // vl
    // + ", [{1 -> T, 2 -> F, 3 -> T}]"s // vm
    ;
  auto test_maps_left = ""s
    + ", {T -> T}"s // maa
    + ", {+7 -> T}"s // mba
    + ", {42 -> T}"s // mca
    + ", {4.2 -> T}"s // mda
    + ", {1337ms -> T}"s // mea
    + ", {2018-12-24 -> T}"s // mfa
    + ", {\"foo\" -> T}"s // mga
    + ", {/foo.*bar/ -> T}"s // mha
    + ", {127.0.0.1 -> T}"s // mia
    + ", {10.0.0.0/8 -> T}"s // mja
    // + ", {[1, 2, 3] -> T}"s // mla
    // + ", {{1 -> T, 2 -> F, 3 -> T} -> T}"s // mna
    ;
  auto test_maps_right = ""s
    // (intentionally no maa)
    + ", {T -> +7}"s // mab
    + ", {T -> 42}"s // mac
    + ", {T -> 4.2}"s // mad
    + ", {T -> 1337ms}"s // mae
    + ", {T -> 2018-12-24}"s // maf
    + ", {T -> \"foo\"}"s // mag
    + ", {T -> /foo.*bar/}"s // mah
    + ", {T -> 127.0.0.1}"s // mai
    + ", {T -> 10.0.0.0/8}"s // maj
    // + ", {T -> [1, 2, 3]}"s // mal
    // + ", {T -> {1 -> T, 2 -> F, 3 -> T}}"s // man
    ;
  auto test_collections
    = test_lists
    + test_maps_left
    + test_maps_right
    ;
  // clang-format on
  // Initialize test data.
  auto rows = std::vector<std::string>{
    "[T, +7, 42, 4.2, 1337ms, 2018-12-24, \"foo\", /foo.*bar/, 127.0.0.1,"
    " 10.0.0.0/8, [1, 2, 3], {1 -> T, 2 -> F, 3 -> T}"
      + test_collections + ", \"aas\"]",
    "[F, -7, 43, 0.42, -1337ms, 2018-12-25, \"bar\", nil, ::1, 64:ff9b::/96,"
    " [], {}"
      + test_collections + ", \"aas\"]",
  };
  for (auto& row : rows) {
    auto xs = unbox(to<data>(row));
    test_data.push_back(caf::get<list>(xs));
  }
}

void table_slices::run() {
  if (builder == nullptr)
    FAIL("no valid builder found; missing fixture initialization?");
  test_add();
  test_equality();
  test_copy();
  test_manual_serialization();
  test_smart_pointer_serialization();
  test_message_serialization();
  test_append_column_to_index();
}

caf::binary_deserializer table_slices::make_source() {
  return caf::binary_deserializer{sys, buf};
}

caf::binary_serializer table_slices::make_sink() {
  buf.clear();
  return caf::binary_serializer{sys, buf};
}

table_slice table_slices::make_slice() {
  for (auto& xs : test_data)
    for (auto& x : xs)
      if (!builder->add(make_view(x)))
        FAIL("builder failed to add element");
  return builder->finish();
}

vast::data_view table_slices::at(size_t row, size_t col) const {
  VAST_ASSERT(row < test_data.size());
  VAST_ASSERT(col < test_data[row].size());
  return make_view(test_data[row][col]);
}

void table_slices::test_add() {
  MESSAGE(">> test table_slice_builder::add");
  auto slice = make_slice();
  CHECK_EQUAL(slice.rows(), 2u);
  auto flat_layout = flatten(layout);
  CHECK_EQUAL(slice.columns(), flat_layout.fields.size());

  for (size_t row = 0; row < slice.rows(); ++row)
    for (size_t col = 0; col < slice.columns(); ++col) {
      MESSAGE("checking value at (" << row << ',' << col << ')');
      CHECK_EQUAL(slice.at(row, col, flat_layout.fields[col].type),
                  at(row, col));
    }
}

void table_slices::test_equality() {
  MESSAGE(">> test equality");
  auto slice1 = make_slice();
  auto slice2 = make_slice();
  CHECK_EQUAL(slice1, slice2);
}

void table_slices::test_copy() {
  MESSAGE(">> test copy");
  auto slice1 = make_slice();
  table_slice slice2{slice1};
  CHECK_EQUAL(slice1, slice2);
}

void table_slices::test_manual_serialization() {
  MESSAGE(">> test manual serialization via inspect");
  MESSAGE("make slices");
  auto slice1 = make_slice();
  table_slice slice2;
  MESSAGE("save content of the first slice into the buffer");
  auto sink = make_sink();
  CHECK_EQUAL(inspect(sink, slice1), caf::none);
  MESSAGE("load content for the second slice from the buffer");
  auto source = make_source();
  CHECK_EQUAL(inspect(source, slice2), caf::none);
  MESSAGE("check result of serialization roundtrip");
  REQUIRE_NOT_EQUAL(slice2.encoding(), table_slice_encoding::none);
  CHECK_EQUAL(slice1, slice2);
}

void table_slices::test_smart_pointer_serialization() {
  MESSAGE(">> test smart pointer serialization");
  MESSAGE("make slices");
  auto slice1 = make_slice();
  table_slice slice2;
  MESSAGE("save content of the first slice into the buffer");
  auto sink = make_sink();
  CHECK_EQUAL(sink(slice1), caf::none);
  MESSAGE("load content for the second slice from the buffer");
  auto source = make_source();
  CHECK_EQUAL(source(slice2), caf::none);
  MESSAGE("check result of serialization roundtrip");
  REQUIRE_NOT_EQUAL(slice2.encoding(), table_slice_encoding::none);
  CHECK_EQUAL(slice1, slice2);
}

void table_slices::test_message_serialization() {
  MESSAGE(">> test message serialization");
  MESSAGE("make slices");
  auto slice1 = caf::make_message(make_slice());
  caf::message slice2;
  MESSAGE("save content of the first slice into the buffer");
  auto sink = make_sink();
  CHECK_EQUAL(sink(slice1), caf::none);
  MESSAGE("load content for the second slice from the buffer");
  auto source = make_source();
  CHECK_EQUAL(source(slice2), caf::none);
  MESSAGE("check result of serialization roundtrip");
  REQUIRE(slice2.match_elements<table_slice>());
  CHECK_EQUAL(slice1.get_as<table_slice>(0), slice2.get_as<table_slice>(0));
  // FIXME: Make the table slice builders use `table_slice_encoding` as key.
  // CHECK_EQUAL(slice2.get_as<table_slice>(0).encoding(),
  //             builder->implementation_id());
}

void table_slices::test_append_column_to_index() {
  MESSAGE(">> test append_column_to_index");
  auto idx = factory<value_index>::make(legacy_integer_type{}, caf::settings{});
  REQUIRE_NOT_EQUAL(idx, nullptr);
  auto slice = make_slice();
  slice.offset(0);
  slice.append_column_to_index(1, *idx);
  CHECK_EQUAL(idx->offset(), 2u);
  constexpr auto less = relational_operator::less;
  CHECK_EQUAL(unbox(idx->lookup(less, make_view(integer{3}))), make_ids({1}));
}

} // namespace fixtures
