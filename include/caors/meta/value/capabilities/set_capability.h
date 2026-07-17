// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_SET_CAPABILITY_H_
#define CAORS_META_VALUE_CAPABILITIES_SET_CAPABILITY_H_

#include "caors/meta/type_ops.h"
#include "caors/meta/value/capability_facade.h"
#include "caors/meta/value/view_factory.h"

namespace caors::meta {

template <> struct capability_bind_traits<set_view> {
  using view_type = set_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const set_ops*, any_ref source) noexcept {
    return make_set_view(source);
  }
};

/** @brief 集合容器的类型级能力 facade。 */
class set_capability final : public type_capability_base<set_ops, set_view> {
public:
  using base_type = type_capability_base<set_ops, set_view>;

  set_capability() noexcept = default;
  explicit set_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }
};

} // namespace caors::meta

#endif
