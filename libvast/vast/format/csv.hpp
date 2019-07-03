/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#pragma once

#include <caf/none.hpp>

#include "vast/config.hpp"

#include "vast/concept/parseable/core.hpp"
#include "vast/concept/parseable/numeric.hpp"
#include "vast/concept/parseable/stream.hpp"
#include "vast/concept/parseable/string.hpp"
#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/address.hpp"
#include "vast/concept/parseable/vast/offset.hpp"
#include "vast/concept/parseable/vast/si.hpp"
#include "vast/concept/parseable/vast/time.hpp"

#include "vast/concept/parseable/core/operators.hpp"
#include "vast/concept/parseable/core/rule.hpp"
#include "vast/concept/parseable/string.hpp"
#include "vast/concept/printable/core.hpp"
#include "vast/concept/printable/numeric.hpp"
#include "vast/concept/printable/string.hpp"
#include "vast/concept/printable/vast/data.hpp"
#include "vast/detail/line_range.hpp"
#include "vast/format/ostream_writer.hpp"
#include "vast/format/single_layout_reader.hpp"
#include "vast/schema.hpp"

namespace vast::format::csv {

class writer : public format::ostream_writer {
public:
  using super = format::ostream_writer;

  using super::super;

  caf::error write(const table_slice& x) override;

  const char* name() const override;

private:
  std::string last_layout_;
};

/// A reader for CSV data. It operates with a *selector* to determine the
/// mapping of CSV object to the appropriate record type in the schema.
class reader final : public single_layout_reader {
public:
  using super = single_layout_reader;

  /// Constructs a CSV reader.
  /// @param table_slice_type The ID for table slice type to build.
  /// @param in The stream of CSV lines.
  explicit reader(caf::atom_value table_slice_type,
                  std::unique_ptr<std::istream> in = nullptr);

  void reset(std::unique_ptr<std::istream> in);

  caf::error schema(vast::schema sch) override;

  vast::schema schema() const override;

  const char* name() const override;

protected:
  caf::error read_impl(size_t max_events, size_t max_slice_size,
                       consumer& f) override;

private:
  using iterator_type = std::string::const_iterator;
  using parser_type = rule<iterator_type>;
  caf::error read_header(std::string_view line);

  std::unique_ptr<std::istream> input_;
  std::unique_ptr<detail::line_range> lines_;
  vast::schema schema_;
  caf::optional<parser_type> parser_;
};

} // namespace vast::format::csv
