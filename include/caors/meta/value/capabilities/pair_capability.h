// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_PAIR_CAPABILITY_H_
#define CAORS_META_VALUE_CAPABILITIES_PAIR_CAPABILITY_H_

#include "caors/meta/capability_bind.h"
#include "caors/meta/type_ops.h"
#include "caors/meta/value/view_factory.h"

namespace caors::meta {

template <> struct capability_bind_traits<pair_view> {
  using view_type = pair_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const pair_ops*, any_ref source) noexcept {
    return make_pair_view(source);
  }
};

/** @brief pair-like 类型的类型级能力 facade。 */
class pair_capability final : public type_capability_base<pair_ops, pair_view> {
public:
  using base_type = type_capability_base<pair_ops, pair_view>;

  pair_capability() noexcept = default;
  explicit pair_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_key_type() const noexcept {
    return valid() ? type_.get_key_type() : meta::type{};
  }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }
};

} // namespace caors::meta

#endif
