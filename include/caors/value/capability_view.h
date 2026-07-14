// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_CAPABILITY_VIEW_H_
#define CAORS_VALUE_CAPABILITY_VIEW_H_

#include "caors/meta/capability.h"
#include "caors/value/any_ref.h"
#include "caors/value/view_factory.h"

namespace caors::meta {

template <> struct capability_view_traits<value::indirect_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static value::indirect_view bind(value::any_ref source) {
    return source.create_indirect_view();
  }
};

template <> struct capability_view_traits<value::sequence_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static value::sequence_view bind(value::any_ref source) {
    return source.create_sequence_view();
  }
};

template <> struct capability_view_traits<value::set_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static value::set_view bind(value::any_ref source) {
    return source.create_set_view();
  }
};

template <> struct capability_view_traits<value::associative_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static value::associative_view bind(value::any_ref source) {
    return source.create_associative_view();
  }
};

template <> struct capability_view_traits<value::pair_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static value::pair_view bind(value::any_ref source) {
    return source.create_pair_view();
  }
};

template <> struct capability_view_traits<value::tuple_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static value::tuple_view bind(value::any_ref source) {
    return source.create_tuple_view();
  }
};

template <> struct capability_view_traits<value::variant_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static value::variant_view bind(value::any_ref source) {
    return source.create_variant_view();
  }
};

} // namespace caors::meta

#endif
