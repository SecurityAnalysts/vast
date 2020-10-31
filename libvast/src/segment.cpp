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

#include "vast/segment.hpp"

#include "vast/bitmap.hpp"
#include "vast/bitmap_algorithms.hpp"
#include "vast/detail/assert.hpp"
#include "vast/detail/byte_swap.hpp"
#include "vast/detail/narrow.hpp"
#include "vast/detail/overload.hpp"
#include "vast/die.hpp"
#include "vast/error.hpp"
#include "vast/fbs/segment.hpp"
#include "vast/fbs/utils.hpp"
#include "vast/fbs/version.hpp"
#include "vast/ids.hpp"
#include "vast/logger.hpp"
#include "vast/si_literals.hpp"
#include "vast/table_slice.hpp"
#include "vast/uuid.hpp"

#include <caf/binary_deserializer.hpp>
#include <caf/binary_serializer.hpp>

#include <functional>
#include <type_traits>

namespace vast {

namespace {

template <class Visitor>
auto visit(Visitor&& visitor, const segment* x) noexcept(
  std::conjunction_v<
    std::is_nothrow_invocable<Visitor>,
    std::is_nothrow_invocable<Visitor, const fbs::segment::v0*>>) {
  switch (x->root()->segment_type()) {
    case fbs::segment::Segment::NONE:
      return std::invoke(std::forward<Visitor>(visitor));
    case fbs::segment::Segment::v0:
      return std::invoke(std::forward<Visitor>(visitor),
                         x->root()->segment_as_v0());
  }
  die("unhandled segment type");
}

} // namespace

uuid segment::id() const {
  auto f = detail::overload{
    []() noexcept { return uuid::nil(); },
    [](const fbs::segment::v0* segment) {
      auto result = uuid::nil();
      if (auto err = unpack(*segment->uuid(), result))
        VAST_ERROR_ANON("failed to get uuid from segment:", render(err));
      return result;
    },
  };
  return visit(std::move(f), this);
}

vast::ids segment::ids() const {
  auto f = detail::overload{
    []() noexcept { return vast::ids{}; },
    [](const fbs::segment::v0* segment) {
      auto result = vast::ids{};
      for (auto&& flat_slice : *segment->slices()) {
        auto slice = flat_slice->data_nested_root();
        result.append_bits(false, slice->offset() - result.size());
        result.append_bits(true, slice->rows());
      }
      return result;
    },
  };
  return visit(std::move(f), this);
}

size_t segment::num_slices() const {
  auto f = detail::overload{
    []() noexcept { return size_t{}; },
    [](const fbs::segment::v0* segment) {
      return detail::narrow_cast<size_t>(segment->slices()->size());
    },
  };
  return visit(std::move(f), this);
}

caf::expected<std::vector<table_slice_ptr>>
segment::lookup(const vast::ids& xs) const {
  auto f = detail::overload{
    []() noexcept -> caf::expected<std::vector<table_slice_ptr>> {
      return caf::no_error;
    },
    [&](const fbs::segment::v0* segment)
      -> caf::expected<std::vector<table_slice_ptr>> {
      std::vector<table_slice_ptr> result;
      auto f = [](auto buffer) {
        auto slice = buffer->data_nested_root();
        return std::pair{slice->offset(), slice->offset() + slice->rows()};
      };
      auto g = [&](auto buffer) -> caf::error {
        // TODO: bind the lifetime of the table slice to the segment chunk.
        // This requires that table slices will be constructable from a chunk.
        // Until then, we stupidly deserialize the data into a new table
        // slice.
        table_slice_ptr slice;
        if (auto err = unpack(*buffer->data_nested_root(), slice))
          return err;
        result.push_back(std::move(slice));
        return caf::none;
      };
      auto begin = segment->slices()->begin();
      auto end = segment->slices()->end();
      if (auto error = select_with(xs, begin, end, f, g))
        return error;
      return result;
    },
  };
  return visit(std::move(f), this);
}

} // namespace vast
