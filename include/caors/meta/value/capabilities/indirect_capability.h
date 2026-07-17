// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_INDIRECT_CAPABILITY_H_
#define CAORS_META_VALUE_CAPABILITIES_INDIRECT_CAPABILITY_H_

#include "caors/meta/capability_bind.h"
#include "caors/meta/type_ops.h"
#include "caors/meta/value/view_factory.h"

namespace caors::meta {

template <> struct capability_bind_traits<indirect_view> {
  using view_type = indirect_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const indirect_ops*, any_ref source) noexcept {
    return make_indirect_view(source);
  }
};

/** @brief 指针和 wrapper 的类型级能力 facade。 */
class indirect_capability final : public type_capability_base<indirect_ops, indirect_view> {
public:
  using base_type = type_capability_base<indirect_ops, indirect_view>;

  indirect_capability() noexcept = default;
  explicit indirect_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }
};

} // namespace caors::meta

#endif
