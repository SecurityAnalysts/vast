//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/type/type.hpp"

namespace vast::experimental {

/// A regular expression.
class pattern_type final
  : public detail::concrete_basic_type<fbs::type::pattern_type::v0> {
public:
  using concrete_basic_type::concrete_basic_type;
};

// -- explicit template instantiations -----------------------------------------

extern template class detail::concrete_type<fbs::type::pattern_type::v0>;
extern template class detail::concrete_basic_type<fbs::type::pattern_type::v0>;

} // namespace vast::experimental
