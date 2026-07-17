// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_CAPABILITIES_ENUM_CAPABILITY_H_
#define CAORS_META_VALUE_CAPABILITIES_ENUM_CAPABILITY_H_

#include "caors/meta/capability_bind.h"
#include "caors/meta/enum_value.h"
#include "caors/meta/type_ops.h"
#include "caors/meta/value/capabilities/bind_helpers.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::meta {

/** @brief 枚举类型的类型级能力 facade。 */
class enum_capability final : public type_capability_base<enum_ops> {
public:
  using base_type = type_capability_base<enum_ops>;

  enum_capability() noexcept = default;
  explicit enum_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] bool is_signed() const noexcept { return valid() && ops_->is_signed; }

  [[nodiscard]] std::size_t value_count() const noexcept {
    return valid() ? detail::enum_value_count(type_) : 0;
  }

  [[nodiscard]] enum_value_view value_at(std::size_t index) const {
    return valid() ? detail::enum_value_at(type_, index) : enum_value_view{};
  }

  [[nodiscard]] enum_value_view find_value(string_name name) const {
    return valid() ? detail::find_enum_value(type_, name) : enum_value_view{};
  }

  [[nodiscard]] enum_value_view find_value(std::string_view name) const {
    return valid() ? detail::find_enum_value(type_, name) : enum_value_view{};
  }

  template <typename Enum>
    requires std::is_enum_v<std::remove_cv_t<std::remove_reference_t<Enum>>>
  [[nodiscard]] enum_value_view find_value(Enum value) const noexcept {
    using enum_type = std::remove_cv_t<std::remove_reference_t<Enum>>;
    using underlying_type = std::underlying_type_t<enum_type>;
    if constexpr (std::is_signed_v<underlying_type>) {
      return valid() ? detail::find_enum_value_by_i64(
                           type_, static_cast<std::int64_t>(static_cast<underlying_type>(value)))
                     : enum_value_view{};
    } else {
      return valid() ? detail::find_enum_value_by_u64(
                           type_, static_cast<std::uint64_t>(static_cast<underlying_type>(value)))
                     : enum_value_view{};
    }
  }

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
