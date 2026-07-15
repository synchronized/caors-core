// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_TYPE_CAPABILITY_H_
#define CAORS_META_VALUE_TYPE_CAPABILITY_H_

#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/view_factory.h"

#include <cstdint>
#include <optional>

namespace caors::meta {

namespace detail {

[[nodiscard]] inline bool can_bind_capability_source(meta::type expected, any_ref source) noexcept {
  return expected && source.valid() && source.get_type().id() == expected.id();
}

template <typename Ops> class type_capability_base {
public:
  type_capability_base() noexcept = default;
  explicit type_capability_base(meta::type type) noexcept
      : type_(type), ops_(type.get_raw_ops<Ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }

protected:
  meta::type type_{};
  const Ops* ops_ = nullptr;
};

template <typename Capability> struct facade_type_capability_traits {
  using result_type = Capability;

  [[nodiscard]] static result_type get(const type& source) noexcept { return Capability{source}; }

  [[nodiscard]] static bool has(const type& source) noexcept { return Capability{source}.valid(); }
};

} // namespace detail

/** @brief 枚举类型的类型级能力 facade。 */
class enum_capability final : public detail::type_capability_base<enum_ops> {
public:
  using base_type = detail::type_capability_base<enum_ops>;

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
class indirect_capability final : public detail::type_capability_base<indirect_ops> {
public:
  using base_type = detail::type_capability_base<indirect_ops>;

  indirect_capability() noexcept = default;
  explicit indirect_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] indirect_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? make_indirect_view(source)
                                                             : indirect_view{};
  }
};

/** @brief 顺序容器的类型级能力 facade。 */
class sequence_capability final : public detail::type_capability_base<sequence_ops> {
public:
  using base_type = detail::type_capability_base<sequence_ops>;

  sequence_capability() noexcept = default;
  explicit sequence_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] sequence_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? make_sequence_view(source)
                                                             : sequence_view{};
  }
};

/** @brief 集合容器的类型级能力 facade。 */
class set_capability final : public detail::type_capability_base<set_ops> {
public:
  using base_type = detail::type_capability_base<set_ops>;

  set_capability() noexcept = default;
  explicit set_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] set_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? make_set_view(source) : set_view{};
  }
};

/** @brief 关联容器的类型级能力 facade。 */
class associative_capability final : public detail::type_capability_base<associative_ops> {
public:
  using base_type = detail::type_capability_base<associative_ops>;

  associative_capability() noexcept = default;
  explicit associative_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_key_type() const noexcept {
    return valid() ? type_.get_key_type() : meta::type{};
  }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] associative_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? make_associative_view(source)
                                                             : associative_view{};
  }
};

/** @brief pair-like 类型的类型级能力 facade。 */
class pair_capability final : public detail::type_capability_base<pair_ops> {
public:
  using base_type = detail::type_capability_base<pair_ops>;

  pair_capability() noexcept = default;
  explicit pair_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] meta::type get_key_type() const noexcept {
    return valid() ? type_.get_key_type() : meta::type{};
  }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] pair_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? make_pair_view(source) : pair_view{};
  }
};

/** @brief tuple-like 类型的类型级能力 facade。 */
class tuple_capability final : public detail::type_capability_base<tuple_ops> {
public:
  using base_type = detail::type_capability_base<tuple_ops>;

  tuple_capability() noexcept = default;
  explicit tuple_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] tuple_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? make_tuple_view(source)
                                                             : tuple_view{};
  }
};

/** @brief variant-like 类型的类型级能力 facade。 */
class variant_capability final : public detail::type_capability_base<variant_ops> {
public:
  using base_type = detail::type_capability_base<variant_ops>;

  variant_capability() noexcept = default;
  explicit variant_capability(meta::type type) noexcept : base_type(type) {}

  [[nodiscard]] variant_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? make_variant_view(source)
                                                             : variant_view{};
  }
};

template <>
struct type_capability_traits<enum_capability>
    : detail::facade_type_capability_traits<enum_capability> {};

template <>
struct type_capability_traits<indirect_capability>
    : detail::facade_type_capability_traits<indirect_capability> {};

template <>
struct type_capability_traits<sequence_capability>
    : detail::facade_type_capability_traits<sequence_capability> {};

template <>
struct type_capability_traits<set_capability>
    : detail::facade_type_capability_traits<set_capability> {};

template <>
struct type_capability_traits<associative_capability>
    : detail::facade_type_capability_traits<associative_capability> {};

template <>
struct type_capability_traits<pair_capability>
    : detail::facade_type_capability_traits<pair_capability> {};

template <>
struct type_capability_traits<tuple_capability>
    : detail::facade_type_capability_traits<tuple_capability> {};

template <>
struct type_capability_traits<variant_capability>
    : detail::facade_type_capability_traits<variant_capability> {};

} // namespace caors::meta

#endif
