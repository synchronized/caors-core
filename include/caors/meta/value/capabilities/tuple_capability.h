// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_TUPLE_CAPABILITY_H_
#define CAORS_META_VALUE_CAPABILITIES_TUPLE_CAPABILITY_H_

#include "caors/meta/type_ops.h"
#include "caors/meta/value/capability_facade.h"
#include "caors/meta/value/view_factory.h"

namespace caors::meta {

template <> struct capability_bind_traits<tuple_view> {
  using view_type = tuple_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const tuple_ops*, any_ref source) noexcept {
    return make_tuple_view(source);
  }
};

/** @brief tuple-like 类型的类型级能力 facade。 */
class tuple_capability final : public type_capability_base<tuple_ops, tuple_view> {
public:
  using base_type = type_capability_base<tuple_ops, tuple_view>;

  tuple_capability() noexcept = default;
  explicit tuple_capability(meta::type type) noexcept : base_type(type) {}
};

} // namespace caors::meta

#endif
