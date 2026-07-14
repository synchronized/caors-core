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

} // namespace detail

/** @brief 枚举类型的类型级能力 facade。 */
class enum_capability final {
public:
  enum_capability() noexcept = default;
  explicit enum_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_ops<enum_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }
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

private:
  meta::type type_{};
  const enum_ops* ops_ = nullptr;
};

/** @brief 指针和 wrapper 的类型级能力 facade。 */
class indirect_capability final {
public:
  indirect_capability() noexcept = default;
  explicit indirect_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_ops<indirect_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] indirect_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? source.create_indirect_view()
                                                             : indirect_view{};
  }

private:
  meta::type type_{};
  const indirect_ops* ops_ = nullptr;
};

/** @brief 顺序容器的类型级能力 facade。 */
class sequence_capability final {
public:
  sequence_capability() noexcept = default;
  explicit sequence_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_ops<sequence_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] sequence_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? source.create_sequence_view()
                                                             : sequence_view{};
  }

private:
  meta::type type_{};
  const sequence_ops* ops_ = nullptr;
};

/** @brief 集合容器的类型级能力 facade。 */
class set_capability final {
public:
  set_capability() noexcept = default;
  explicit set_capability(meta::type type) noexcept : type_(type), ops_(type.get_ops<set_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] set_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? source.create_set_view()
                                                             : set_view{};
  }

private:
  meta::type type_{};
  const set_ops* ops_ = nullptr;
};

/** @brief 关联容器的类型级能力 facade。 */
class associative_capability final {
public:
  associative_capability() noexcept = default;
  explicit associative_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_ops<associative_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }
  [[nodiscard]] meta::type get_key_type() const noexcept {
    return valid() ? type_.get_key_type() : meta::type{};
  }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] associative_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? source.create_associative_view()
                                                             : associative_view{};
  }

private:
  meta::type type_{};
  const associative_ops* ops_ = nullptr;
};

/** @brief pair-like 类型的类型级能力 facade。 */
class pair_capability final {
public:
  pair_capability() noexcept = default;
  explicit pair_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_ops<pair_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }
  [[nodiscard]] meta::type get_key_type() const noexcept {
    return valid() ? type_.get_key_type() : meta::type{};
  }
  [[nodiscard]] meta::type get_value_type() const noexcept {
    return valid() ? type_.get_value_type() : meta::type{};
  }

  [[nodiscard]] pair_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? source.create_pair_view()
                                                             : pair_view{};
  }

private:
  meta::type type_{};
  const pair_ops* ops_ = nullptr;
};

/** @brief tuple-like 类型的类型级能力 facade。 */
class tuple_capability final {
public:
  tuple_capability() noexcept = default;
  explicit tuple_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_ops<tuple_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }

  [[nodiscard]] tuple_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? source.create_tuple_view()
                                                             : tuple_view{};
  }

private:
  meta::type type_{};
  const tuple_ops* ops_ = nullptr;
};

/** @brief variant-like 类型的类型级能力 facade。 */
class variant_capability final {
public:
  variant_capability() noexcept = default;
  explicit variant_capability(meta::type type) noexcept
      : type_(type), ops_(type.get_ops<variant_ops>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && ops_ != nullptr && ops_->valid(); }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }

  [[nodiscard]] variant_view bind(any_ref source) const {
    return detail::can_bind_capability_source(type_, source) ? source.create_variant_view()
                                                             : variant_view{};
  }

private:
  meta::type type_{};
  const variant_ops* ops_ = nullptr;
};

template <> struct type_capability_traits<enum_capability> {
  using result_type = enum_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return enum_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return enum_capability{source}.valid();
  }
};

template <> struct type_capability_traits<indirect_capability> {
  using result_type = indirect_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return indirect_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return indirect_capability{source}.valid();
  }
};

template <> struct type_capability_traits<sequence_capability> {
  using result_type = sequence_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return sequence_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return sequence_capability{source}.valid();
  }
};

template <> struct type_capability_traits<set_capability> {
  using result_type = set_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return set_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return set_capability{source}.valid();
  }
};

template <> struct type_capability_traits<associative_capability> {
  using result_type = associative_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return associative_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return associative_capability{source}.valid();
  }
};

template <> struct type_capability_traits<pair_capability> {
  using result_type = pair_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return pair_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return pair_capability{source}.valid();
  }
};

template <> struct type_capability_traits<tuple_capability> {
  using result_type = tuple_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return tuple_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return tuple_capability{source}.valid();
  }
};

template <> struct type_capability_traits<variant_capability> {
  using result_type = variant_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return variant_capability{source};
  }

  [[nodiscard]] static bool has(const type& source) noexcept {
    return variant_capability{source}.valid();
  }
};

} // namespace caors::meta

#endif
