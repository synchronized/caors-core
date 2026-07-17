// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_ENUM_CAPABILITY_H_
#define CAORS_META_VALUE_CAPABILITIES_ENUM_CAPABILITY_H_

#include "caors/meta/type_ops.h"
#include "caors/meta/value/capabilities/bind_helpers.h"
#include "caors/meta/value/capability_facade.h"

#include <cstdint>
#include <optional>

namespace caors::meta {

/** @brief 枚举类型的类型级能力 facade。 */
class enum_capability final : public type_capability_base<enum_ops> {
public:
  using base_type = type_capability_base<enum_ops>;

  enum_capability() noexcept = default;
  explicit enum_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] bool is_signed() const noexcept { return valid() && ops_->is_signed; }

  [[nodiscard]] std::optional<std::int64_t> to_i64(any_ref source) const noexcept {
    if (!detail::can_bind_capability_source(type_, source) || ops_ == nullptr ||
        ops_->to_i64 == nullptr) {
      return std::nullopt;
    }

    return ops_->to_i64(source.data());
  }

  [[nodiscard]] std::optional<std::uint64_t> to_u64(any_ref source) const noexcept {
    if (!detail::can_bind_capability_source(type_, source) || ops_ == nullptr ||
        ops_->to_u64 == nullptr) {
      return std::nullopt;
    }

    return ops_->to_u64(source.data());
  }

  bool from_i64(any_ref source, std::int64_t value) const noexcept {
    if (!detail::can_bind_capability_source(type_, source) || !source.writable() ||
        ops_ == nullptr || ops_->from_i64 == nullptr) {
      return false;
    }

    return ops_->from_i64(source.mutable_data(), value);
  }

  bool from_u64(any_ref source, std::uint64_t value) const noexcept {
    if (!detail::can_bind_capability_source(type_, source) || !source.writable() ||
        ops_ == nullptr || ops_->from_u64 == nullptr) {
      return false;
    }

    return ops_->from_u64(source.mutable_data(), value);
  }
};

} // namespace caors::meta

#endif
