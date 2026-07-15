// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_TYPE_CAPABILITY_H_
#define CAORS_META_VALUE_TYPE_CAPABILITY_H_

#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/capability_facade.h"
#include "caors/meta/value/view_factory.h"

#include <cstdint>
#include <optional>

namespace caors::meta {

template <> struct capability_bind_traits<indirect_view> {
  using view_type = indirect_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const indirect_ops*, any_ref source) noexcept {
    return make_indirect_view(source);
  }
};

template <> struct capability_bind_traits<sequence_view> {
  using view_type = sequence_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const sequence_ops*, any_ref source) noexcept {
    return make_sequence_view(source);
  }
};

template <> struct capability_bind_traits<set_view> {
  using view_type = set_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const set_ops*, any_ref source) noexcept {
    return make_set_view(source);
  }
};

template <> struct capability_bind_traits<associative_view> {
  using view_type = associative_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const associative_ops*, any_ref source) noexcept {
    return make_associative_view(source);
  }
};

template <> struct capability_bind_traits<pair_view> {
  using view_type = pair_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const pair_ops*, any_ref source) noexcept {
    return make_pair_view(source);
  }
};

template <> struct capability_bind_traits<tuple_view> {
  using view_type = tuple_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const tuple_ops*, any_ref source) noexcept {
    return make_tuple_view(source);
  }
};

template <> struct capability_bind_traits<variant_view> {
  using view_type = variant_view;

  static constexpr bool supported = true;

  [[nodiscard]] static view_type bind(meta::type, const variant_ops*, any_ref source) noexcept {
    return make_variant_view(source);
  }
};

namespace detail {

[[nodiscard]] inline bool can_bind_capability_source(meta::type expected, any_ref source) noexcept {
  return expected && source.valid() && source.get_type().id() == expected.id();
}

} // namespace detail

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

/** @brief 顺序容器的类型级能力 facade。 */
class sequence_capability final : public type_capability_base<sequence_ops, sequence_view> {
public:
  using base_type = type_capability_base<sequence_ops, sequence_view>;

  sequence_capability() noexcept = default;
  explicit sequence_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
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

/** @brief 关联容器的类型级能力 facade。 */
class associative_capability final
    : public type_capability_base<associative_ops, associative_view> {
public:
  using base_type = type_capability_base<associative_ops, associative_view>;

  associative_capability() noexcept = default;
  explicit associative_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_key_type() const noexcept {
    return valid() ? type_.get_key_type() : meta::type{};
  }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
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

/** @brief tuple-like 类型的类型级能力 facade。 */
class tuple_capability final : public type_capability_base<tuple_ops, tuple_view> {
public:
  using base_type = type_capability_base<tuple_ops, tuple_view>;

  tuple_capability() noexcept = default;
  explicit tuple_capability(meta::type type) noexcept : base_type(type) {}
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
