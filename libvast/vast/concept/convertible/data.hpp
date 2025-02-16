//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/fwd.hpp"

#include "vast/concept/parseable/core/parser.hpp"
#include "vast/concept/parseable/parse.hpp"
#include "vast/concept/printable/vast/legacy_type.hpp"
#include "vast/concepts.hpp"
#include "vast/data.hpp"
#include "vast/detail/narrow.hpp"
#include "vast/detail/type_traits.hpp"
#include "vast/error.hpp"
#include "vast/legacy_type.hpp"
#include "vast/logger.hpp"

#include <caf/error.hpp>

/// Assigns fields from `src` to `dst`.
/// The source must have a structure that matches the destination.
/// For example:
/// auto xs = record{               | struct foo {
///   {"a", "foo"},                 |   std::string a;
///   {"b", record{                 |   struct {
///     {"c", -42},                 |     integer c;
///     {"d", list{1, 2, 3}}        |     std::vector<count> d;
///   }},                           |   } b;
///   {"e", record{                 |   struct {
///     {"f", caf::none},           |     integer f;
///     {"g", caf::none},           |     std::optional<count> g;
///   }},                           |   } e;
///   {"h", true}                   |   bool h;
/// };                              | };
///
/// If a member of `out` is missing in `in`, the value does not get overwritten,
/// Similarly, data in `in` that does not match `out` is ignored.
///
/// A special overload that can turn a list of records into a key-value map
/// requires that one of the fields in the accompanying legacy_record_type has
/// the "key" attribute. This field will then be used as the key in the target
/// map.
/// NOTE: The overload for `data` is defined last for reasons explained there.

namespace vast {

namespace detail {

// clang-format off
template <concepts::insertable To>
  requires requires {
    typename To::key_type;
    typename To::mapped_type;
  }
caf::error insert_to_map(To& dst, typename To::key_type&& key,
                         typename To::mapped_type&& value) {
  auto entry = dst.find(key);
  if (entry == dst.end()) {
    dst.insert({std::move(key), std::move(value)});
  } else {
    // If the mapped type implements the Semigroup concept the values get
    // combined automatically.
    if constexpr (concepts::semigroup<typename To::mapped_type>)
      entry->second = mappend(std::move(entry->second), std::move(value));
    else
      // TODO: Consider continuing if the old and new values are the same.
      return caf::make_error(ec::convert_error,
                             fmt::format(": redefinition of {} detected: \"{}\" "
                                         "vs \"{}\"",
                                         key, entry->second, value));
  }
  return caf::none;
}
// clang-format on

template <class... Args>
[[nodiscard]] caf::error
prepend(caf::error&& in, const char* fstring, Args&&... args) {
  if (in) {
    auto f = fmt::format("{}{{}}", fstring);
    auto new_msg = in.context().apply({
      [&](std::string& s) {
        return caf::make_message(fmt::format(
          VAST_FMT_RUNTIME(f), std::forward<Args>(args)..., std::move(s)));
      },
    });
    if (new_msg)
      in.context() = std::move(*new_msg);
    else
      in.context() = caf::make_message(
        fmt::format(VAST_FMT_RUNTIME(fstring), std::forward<Args>(args)...));
  }
  return std::move(in);
}

} // namespace detail

/// Checks if `from` can be converted to `to`, i.e. whether a viable overload
/// of / `convert(from, to)` exists.
// NOTE: You might wonder why this is defined as a macro instead of a concept.
// The issue here is that we're looking for overloads of a non-member function,
// and the rules for name lookup for free functions mandate that regular
// qualified or unqualified lookup is done when the definition of the template
// is parsed, and only dependent lookup is done in the instantiation phase in
// case the call is unqualified. That means a concept would only be able to
// detect overloads that are declared later iff they happen to be in the same
// namespace as their arguments, but it won't pick up overloads like
// `convert(std::string, std::string)` or `convert(uint64_t, uint64_t)`.
// https://timsong-cpp.github.io/cppwp/n4868/temp.res#temp.dep.candidate
// It would be preferable to forward declare `is_concrete_convertible` but that
// is not allowed.
// The only real way to solve this is to replace function overloading with
// specializations of a converter struct template.
#define IS_TYPED_CONVERTIBLE(from, to, type)                                   \
  requires {                                                                   \
    { vast::convert(from, to, type) } -> concepts::same_as<caf::error>;        \
  }

#define IS_UNTYPED_CONVERTIBLE(from, to)                                       \
  requires {                                                                   \
    { vast::convert(from, to) } -> concepts::same_as<caf::error>;              \
  }

template <class T>
concept has_layout = requires {
  concepts::same_as<decltype(T::layout), legacy_record_type>;
};

// Overload for records.
template <concepts::inspectable To>
caf::error
convert(const record& src, To& dst, const legacy_record_type& layout);

template <has_layout To>
caf::error convert(const record& src, To& dst);

template <has_layout To>
caf::error convert(const data& src, To& dst);

// Generic overload when `src` and `dst` are of the same type.
// TODO: remove the `!concepts::integral` constraint once count is a real type.
template <class Type, class T>
  requires(!concepts::integral<T>)
|| concepts::same_as<bool, T> caf::error
  convert(const T& src, T& dst, const Type&) {
  dst = src;
  return caf::none;
}

// Dispatch to standard conversion.
// clang-format off
template <class From, class To, class Type>
  requires (!concepts::same_as<From, To>) &&
           concepts::convertible_to<From, To>
caf::error convert(const From& src, To& dst, const Type&) {
  dst = src;
  return caf::none;
}
// clang-format on

// Overload for counts.
template <concepts::unsigned_integral To>
caf::error convert(const count& src, To& dst, const legacy_count_type&) {
  if constexpr (sizeof(To) >= sizeof(count)) {
    dst = src;
  } else {
    if (src < std::numeric_limits<To>::min()
        || src > std::numeric_limits<To>::max())
      return caf::make_error(ec::convert_error,
                             fmt::format(": {} can not be represented by the "
                                         "target variable [{}, {}]",
                                         src, std::numeric_limits<To>::min(),
                                         std::numeric_limits<To>::max()));
    dst = detail::narrow_cast<To>(src);
  }
  return caf::none;
}

template <concepts::unsigned_integral To>
caf::error convert(const integer& src, To& dst, const legacy_count_type&) {
  if (src.value < 0)
    return caf::make_error(
      ec::convert_error, fmt::format(": {} can not be negative ({})",
                                     detail::pretty_type_name(dst), src.value));
  if constexpr (sizeof(To) >= sizeof(count)) {
    dst = src.value;
  } else {
    if (src.value
        > static_cast<integer::value_type>(std::numeric_limits<To>::max()))
      return caf::make_error(ec::convert_error,
                             fmt::format(": {} can not be represented by the "
                                         "target variable [{}, {}]",
                                         src, std::numeric_limits<To>::min(),
                                         std::numeric_limits<To>::max()));
    dst = detail::narrow_cast<To>(src.value);
  }
  return caf::none;
}

// Overload for integers.
template <concepts::signed_integral To>
caf::error convert(const integer& src, To& dst, const legacy_integer_type&) {
  if constexpr (sizeof(To) >= sizeof(integer::value)) {
    dst = src.value;
  } else {
    if (src.value < std::numeric_limits<To>::min()
        || src.value > std::numeric_limits<To>::max())
      return caf::make_error(ec::convert_error,
                             fmt::format(": {} can not be represented by the "
                                         "target variable [{}, {}]",
                                         src, std::numeric_limits<To>::min(),
                                         std::numeric_limits<To>::max()));
    dst = detail::narrow_cast<To>(src.value);
  }
  return caf::none;
}

// Overload for enums.
// clang-format off
template <class To>
  requires std::is_enum_v<To>
caf::error convert(const std::string& src, To& dst, const legacy_enumeration_type& t) {
  auto i = std::find(t.fields.begin(), t.fields.end(), src);
  if (i == t.fields.end())
    return caf::make_error(ec::convert_error,
                           fmt::format(": {} is not a value of {}", src,
                                       detail::pretty_type_name(dst)));
  dst = detail::narrow_cast<To>(std::distance(t.fields.begin(), i));
  return caf::none;
}
// clang-format on

template <class From, class To, class Type>
caf::error convert(const From& src, std::optional<To>& dst, const Type& t) {
  if (!dst)
    dst = To{};
  return convert(src, *dst, t);
}

template <class From, class To, class Type>
caf::error convert(const From& src, caf::optional<To>& dst, const Type& t) {
  if (!dst)
    dst = To{};
  return convert(src, *dst, t);
}

// Overload for lists.
template <concepts::appendable To>
caf::error convert(const list& src, To& dst, const legacy_list_type& t) {
  size_t num = 0;
  for (const auto& element : src) {
    typename To::value_type v{};
    if (auto err = convert(element, v, t.value_type))
      return detail::prepend(std::move(err), "[{}]", num);
    dst.push_back(std::move(v));
    num++;
  }
  return caf::none;
}

// Overload for maps.
// clang-format off
template <concepts::insertable To>
  requires requires {
    typename To::key_type;
    typename To::mapped_type;
  }
// clang-format on
caf::error convert(const map& src, To& dst, const legacy_map_type& t) {
  // TODO: Use structured bindings outside of the lambda once clang supports
  // that.
  for (const auto& x : src) {
    auto err = [&] {
      const auto& [data_key, data_value] = x;
      typename To::key_type key{};
      if (auto err = convert(data_key, key, t.key_type))
        return err;
      typename To::mapped_type value{};
      if (auto err = convert(data_value, value, t.value_type))
        return err;
      return detail::insert_to_map(dst, std::move(key), std::move(value));
    }();
    if (err)
      return detail::prepend(std::move(err), ".{}", x.first);
  }
  return caf::none;
}

// Overload for record to map.
// clang-format off
template <concepts::insertable To>
  requires requires {
    typename To::key_type;
    typename To::mapped_type;
  }
// clang-format on
caf::error convert(const record& src, To& dst, const legacy_map_type& t) {
  // TODO: Use structured bindings outside of the lambda once clang supports
  // that.
  for (const auto& x : src) {
    auto err = [&] {
      const auto& [data_key, data_value] = x;
      typename To::key_type key{};
      if (auto err = convert(data_key, key, t.key_type))
        return err;
      typename To::mapped_type value{};
      if (auto err = convert(data_value, value, t.value_type))
        return err;
      return detail::insert_to_map(dst, std::move(key), std::move(value));
    }();
    if (err)
      return detail::prepend(std::move(err), ".{}", x.first);
  }
  return caf::none;
}

// TODO: Consider moving this to data.hpp
caf::expected<const data*>
get(const record& rec,
    const detail::stack_vector<const record_field*, 64>& trace);

// Overload for list<record> to map.
// NOTE: This conversion type needs a field with the "key" attribute in the
// legacy_record_type. The field with the "key" attribute is pulled out and used
// as the key for the new entry in the destination map.
// clang-format off
template <concepts::insertable To>
  requires requires {
    typename To::key_type;
    typename To::mapped_type;
  }
// clang-format on
caf::error convert(const list& src, To& dst, const legacy_list_type& t) {
  const auto* r = caf::get_if<legacy_record_type>(&t.value_type);
  if (!r)
    return caf::make_error(
      ec::convert_error,
      fmt::format(": expected a legacy_record_type, but got {}", t.value_type));
  // Look for the "key" attribute in `r`.
  auto key_field = legacy_record_type::each::range_state{};
  for (const auto& leaf : legacy_record_type::each(*r)) {
    if (has_attribute(leaf.type(), "key")) {
      if (!key_field.offset.empty())
        return caf::make_error(
          ec::convert_error, fmt::format(": key field must be unique: {}", *r));
      key_field = leaf;
    }
  }
  if (key_field.offset.empty())
    return caf::make_error(
      ec::convert_error,
      fmt::format(": record type in list is missing a key field: {}", *r));
  std::vector<std::string_view> path;
  for (const auto* f : key_field.trace)
    path.emplace_back(f->name);
  // TODO: Consider adding an overload that takes a trace as argument.
  auto pruned = remove_field(*r, path);
  VAST_ASSERT(pruned);
  for (const auto& element : src) {
    const auto* rec = caf::get_if<record>(&element);
    if (!rec)
      return caf::make_error(ec::convert_error, ": no record in list");
    // Find the value from the record
    const auto data_key = get(*rec, key_field.trace);
    if (!data_key)
      return data_key.error();
    if (*data_key == nullptr)
      continue;
    typename To::key_type key{};
    if (auto err = convert(**data_key, key, key_field.type()))
      return err;
    using mapped_type = typename To::mapped_type;
    mapped_type value{};
    if (auto err = convert(*rec, value, *pruned))
      return err;
    if (auto err = detail::insert_to_map(dst, std::move(key), std::move(value)))
      return err;
  }
  return caf::none;
}

class record_inspector {
public:
  using result_type = caf::error;

  template <class To>
  caf::error apply(const legacy_record_type::each::range_state& f, To& dst) {
    // Find the value from the record
    const auto data_value = get(src, f.trace);
    if (!data_value)
      return data_value.error();
    if (*data_value == nullptr)
      return caf::none;
    auto err = caf::visit(
      [&]<class Data, class Type>(const Data& d, const Type& t) -> caf::error {
        using concrete_type = std::decay_t<Type>;
        if constexpr (detail::is_any_v<concrete_type, legacy_alias_type,
                                       legacy_none_type>) {
          // Data conversion of none or alias type does not make sense.
          return caf::make_error(ec::convert_error, ": can't convert alias or "
                                                    "none types");
        } else {
          if constexpr (std::is_same_v<Data, caf::none_t>) {
            if constexpr (std::is_default_constructible_v<To>)
              new (&dst) To{};
            return caf::none;
          } else {
            if constexpr (IS_TYPED_CONVERTIBLE(d, dst, t))
              return convert(d, dst, t);
            if constexpr (IS_UNTYPED_CONVERTIBLE(d, dst))
              return convert(d, dst);
            else
              return caf::make_error(
                ec::convert_error,
                fmt::format(": can't convert from {} to {} with type {}",
                            detail::pretty_type_name(d),
                            detail::pretty_type_name(dst), t));
          }
        }
        return caf::none;
      },
      **data_value, f.type());
    return detail::prepend(std::move(err), ".{}", f.key());
  }

  template <class... Ts>
  caf::error operator()(Ts&&... xs) {
    const auto& rng = legacy_record_type::each(layout);
    auto it = rng.begin();
    return caf::error::eval([&]() -> caf::error {
      if constexpr (!caf::meta::is_annotation<Ts>::value)
        return apply(*it++, xs);
      return caf::none;
    }...);
  }

  const legacy_record_type& layout;
  const record& src;
};

// Overload for records.
template <concepts::inspectable To>
caf::error
convert(const record& src, To& dst, const legacy_record_type& layout) {
  if (layout.fields.empty()) {
    if constexpr (has_layout<To>)
      return convert(src, dst);
    else
      return caf::make_error(ec::convert_error,
                             fmt::format(": destination types must have a "
                                         "static layout definition: {}",
                                         detail::pretty_type_name(dst)));
  }
  auto ri = record_inspector{layout, src};
  return inspect(ri, dst);
}

template <has_layout To>
caf::error convert(const record& src, To& dst) {
  return convert(src, dst, dst.layout);
}

template <has_layout To>
caf::error convert(const data& src, To& dst) {
  if (const auto* r = caf::get_if<record>(&src))
    return convert(*r, dst);
  return caf::make_error(ec::convert_error,
                         fmt::format(": expected record, but got {}", src));
}

// TODO: Move to a dedicated header after conversion is refactored to use
// specialization.
template <registered_parser_type To>
caf::error convert(std::string_view src, To& dst) {
  const auto* f = src.begin();
  if (!parse(f, src.end(), dst))
    return caf::make_error(ec::convert_error,
                           fmt::format(": unable to parse \"{}\" into a {}",
                                       src, detail::pretty_type_name(dst)));
  return caf::none;
}

// A concept to detect whether any previously declared overloads of
// `convert` can be used for a combination of `Type`, `From`, and `To`.
template <class From, class To, class Type>
concept is_concrete_typed_convertible
  = requires(const From& src, To& dst, const Type& type) {
  { vast::convert(src, dst, type) } -> concepts::same_as<caf::error>;
};

// The same concept but this time to check for any untyped convert
// overloads.
template <class From, class To>
concept is_concrete_untyped_convertible = requires(const From& src, To& dst) {
  { vast::convert(src, dst) } -> concepts::same_as<caf::error>;
};

// NOTE: This overload has to be last because we need to be able to detect
// all other overloads with `is_concrete_convertible`. At the same time,
// it must not be declared before to prevent recursing into itself because
// of the non-explicit constructor of `data`.
template <class To>
caf::error convert(const data& src, To& dst, const legacy_type& t) {
  return caf::visit(
    [&]<class From, class Type>(const From& x, const Type& t) {
      if constexpr (is_concrete_typed_convertible<From, To, Type>)
        return convert(x, dst, t);
      else if constexpr (is_concrete_untyped_convertible<From, To>)
        return convert(x, dst);
      else
        return caf::make_error(ec::convert_error,
                               fmt::format("can't convert from {} to {} with "
                                           "type {}",
                                           detail::pretty_type_name(x),
                                           detail::pretty_type_name(dst), t));
    },
    src, t);
}

} // namespace vast
