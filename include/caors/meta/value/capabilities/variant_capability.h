// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_VARIANT_CAPABILITY_H_
#define CAORS_META_VALUE_CAPABILITIES_VARIANT_CAPABILITY_H_

#include "caors/meta/type_ops.h"
#include "caors/meta/value/capability_facade.h"
#include "caors/meta/value/view_factory.h"

namespace caors::meta {

template <> struct capability_bind_traits<variant_view> {
  using view_type = variant_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const variant_ops*, any_ref source) noexcept {
    return make_variant_view(source);
  }
};

/** @brief variant-like 类型的类型级能力 facade。 */
class variant_capability final : public type_capability_base<variant_ops, variant_view> {
public:
  using base_type = type_capability_base<variant_ops, variant_view>;

  variant_capability() noexcept = default;
  explicit variant_capability(meta::type type) noexcept : base_type(type) {}
};

} // namespace caors::meta

#endif
