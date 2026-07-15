// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITY_VIEW_H_
#define CAORS_META_VALUE_CAPABILITY_VIEW_H_

#include "caors/meta/capability.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/capability_facade.h"
#include "caors/meta/value/view_factory.h"

namespace caors::meta {

template <> struct capability_view_traits<indirect_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static indirect_view bind(any_ref source) noexcept {
    return make_indirect_view(source);
  }
};

template <> struct capability_view_traits<sequence_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static sequence_view bind(any_ref source) noexcept {
    return make_sequence_view(source);
  }
};

template <> struct capability_view_traits<set_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static set_view bind(any_ref source) noexcept { return make_set_view(source); }
};

template <> struct capability_view_traits<associative_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static associative_view bind(any_ref source) noexcept {
    return make_associative_view(source);
  }
};

template <> struct capability_view_traits<pair_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static pair_view bind(any_ref source) noexcept { return make_pair_view(source); }
};

template <> struct capability_view_traits<tuple_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static tuple_view bind(any_ref source) noexcept { return make_tuple_view(source); }
};

template <> struct capability_view_traits<variant_view> {
  static constexpr bool supported = true;

  [[nodiscard]] static variant_view bind(any_ref source) noexcept {
    return make_variant_view(source);
  }
};

} // namespace caors::meta

#endif
