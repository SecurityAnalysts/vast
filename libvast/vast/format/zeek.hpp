//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2019 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/fwd.hpp"

#include "vast/concept/parseable/core.hpp"
#include "vast/concept/parseable/numeric.hpp"
#include "vast/concept/parseable/string/any.hpp"
#include "vast/concept/parseable/vast/address.hpp"
#include "vast/concept/parseable/vast/subnet.hpp"
#include "vast/data.hpp"
#include "vast/defaults.hpp"
#include "vast/detail/line_range.hpp"
#include "vast/detail/string.hpp"
#include "vast/format/ostream_writer.hpp"
#include "vast/format/reader.hpp"
#include "vast/format/single_layout_reader.hpp"
#include "vast/format/writer.hpp"
#include "vast/schema.hpp"
#include "vast/table_slice_builder.hpp"

#include <caf/expected.hpp>
#include <caf/fwd.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vast::format::zeek {

/// Parses non-container types.
template <class Iterator, class Attribute>
struct zeek_parser {
  zeek_parser(Iterator& f, const Iterator& l, Attribute& attr)
    : f_{f}, l_{l}, attr_{attr} {
  }

  template <class Parser>
  bool parse(const Parser& p) const {
    return p(f_, l_, attr_);
  }

  template <class T>
  bool operator()(const T&) const {
    return false;
  }

  bool operator()(const legacy_bool_type&) const {
    return parse(parsers::tf);
  }

  bool operator()(const legacy_integer_type&) const {
    static auto p = parsers::i64->*[](integer::value_type x) { //
      return integer{x};
    };
    return parse(p);
  }

  bool operator()(const legacy_count_type&) const {
    static auto p = parsers::u64->*[](count x) { return x; };
    return parse(p);
  }

  bool operator()(const legacy_time_type&) const {
    static auto p = parsers::real->*[](real x) {
      auto i = std::chrono::duration_cast<duration>(double_seconds(x));
      return time{i};
    };
    return parse(p);
  }

  bool operator()(const legacy_duration_type&) const {
    static auto p = parsers::real->*[](real x) {
      return std::chrono::duration_cast<duration>(double_seconds(x));
    };
    return parse(p);
  }

  bool operator()(const legacy_string_type&) const {
    static auto p
      = +parsers::any->*[](std::string x) { return detail::byte_unescape(x); };
    return parse(p);
  }

  bool operator()(const legacy_pattern_type&) const {
    static auto p
      = +parsers::any->*[](std::string x) { return detail::byte_unescape(x); };
    return parse(p);
  }

  bool operator()(const legacy_address_type&) const {
    static auto p = parsers::addr->*[](address x) { return x; };
    return parse(p);
  }

  bool operator()(const legacy_subnet_type&) const {
    static auto p = parsers::net->*[](subnet x) { return x; };
    return parse(p);
  }

  Iterator& f_;
  const Iterator& l_;
  Attribute& attr_;
};

/// Constructs a polymorphic Zeek data parser.
template <class Iterator, class Attribute>
struct zeek_parser_factory {
  using result_type = rule<Iterator, Attribute>;

  zeek_parser_factory(const std::string& set_separator)
    : set_separator_{set_separator} {
  }

  template <class T>
  result_type operator()(const T&) const {
    return {};
  }

  result_type operator()(const legacy_bool_type&) const {
    return parsers::tf;
  }

  result_type operator()(const legacy_real_type&) const {
    return parsers::real->*[](real x) { return x; };
  }

  result_type operator()(const legacy_integer_type&) const {
    return parsers::i64->*[](integer::value_type x) { return integer{x}; };
  }

  result_type operator()(const legacy_count_type&) const {
    return parsers::u64->*[](count x) { return x; };
  }

  result_type operator()(const legacy_time_type&) const {
    return parsers::real->*[](real x) {
      auto i = std::chrono::duration_cast<duration>(double_seconds(x));
      return time{i};
    };
  }

  result_type operator()(const legacy_duration_type&) const {
    return parsers::real->*[](real x) {
      return std::chrono::duration_cast<duration>(double_seconds(x));
    };
  }

  result_type operator()(const legacy_string_type&) const {
    if (set_separator_.empty())
      return +parsers::any->*
             [](std::string x) { return detail::byte_unescape(x); };
    else
      return +(parsers::any - set_separator_)->*[](std::string x) {
        return detail::byte_unescape(x);
      };
  }

  result_type operator()(const legacy_pattern_type&) const {
    if (set_separator_.empty())
      return +parsers::any->*
             [](std::string x) { return detail::byte_unescape(x); };
    else
      return +(parsers::any - set_separator_)->*[](std::string x) {
        return detail::byte_unescape(x);
      };
  }

  result_type operator()(const legacy_address_type&) const {
    return parsers::addr->*[](address x) { return x; };
  }

  result_type operator()(const legacy_subnet_type&) const {
    return parsers::net->*[](subnet x) { return x; };
  }

  result_type operator()(const legacy_list_type& t) const {
    return (caf::visit(*this, t.value_type) % set_separator_)
             ->*[](std::vector<Attribute> x) { return list(std::move(x)); };
  }

  const std::string& set_separator_;
};

/// Constructs a Zeek data parser from a type and set separator.
template <class Iterator, class Attribute = data>
rule<Iterator, Attribute>
make_zeek_parser(const legacy_type& t, const std::string& set_separator = ",") {
  rule<Iterator, Attribute> r;
  auto sep = is_container(t) ? set_separator : "";
  return caf::visit(zeek_parser_factory<Iterator, Attribute>{sep}, t);
}

/// Parses non-container Zeek data.
template <class Iterator, class Attribute = data>
bool zeek_basic_parse(const legacy_type& t, Iterator& f, const Iterator& l,
                      Attribute& attr) {
  return caf::visit(zeek_parser<Iterator, Attribute>{f, l, attr}, t);
}

/// A Zeek reader.
class reader final : public single_layout_reader {
public:
  using super = single_layout_reader;

  /// Constructs a Zeek reader.
  /// @param options Additional options.
  /// @param input The stream of logs to read.
  reader(const caf::settings& options, std::unique_ptr<std::istream> in
                                       = nullptr);

  void reset(std::unique_ptr<std::istream> in) override;

  caf::error schema(vast::schema sch) override;

  vast::schema schema() const override;

  const char* name() const override;

protected:
  caf::error
  read_impl(size_t max_events, size_t max_slice_size, consumer& f) override;

private:
  using iterator_type = std::string_view::const_iterator;

  caf::error parse_header();

  std::unique_ptr<std::istream> input_;
  std::unique_ptr<detail::line_range> lines_;
  std::string separator_;
  std::string set_separator_;
  std::string empty_field_;
  std::string unset_field_;
  vast::schema schema_;
  legacy_type type_;
  legacy_record_type layout_;
  std::optional<size_t> proto_field_;
  std::vector<rule<iterator_type, data>> parsers_;
};

/// A Zeek writer.
class writer : public format::writer {
public:
  writer(writer&&) = default;

  writer& operator=(writer&&) = default;

  ~writer() override = default;

  /// Constructs a Zeek writer.
  /// @param options The configuration options for the writer.
  explicit writer(const caf::settings& options);

  caf::error write(const table_slice& e) override;

  caf::expected<void> flush() override;

  const char* name() const override;

private:
  std::filesystem::path dir_;
  legacy_type previous_layout_;
  bool show_timestamp_tags_;

  /// One writer for each layout.
  std::unordered_map<std::string, ostream_writer_ptr> writers_;
};

} // namespace vast::format::zeek
