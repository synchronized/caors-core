// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CAPABILITY_FACADE_H_
#define CAORS_META_CAPABILITY_FACADE_H_

#include "caors/meta/type.h"
#include "caors/meta/value_ops.h"

#include <cstddef>
#include <span>
#include <type_traits>

namespace caors::meta {

class any;
class any_ref;
class owned_value;

/**
 * @brief 类型级 capability facade 的通用基类。
 *
 * `Ops` 是类型擦除函数表。派生类型只需要继承该基类并实现自己的类型级行为，即可复用
 * `type`、`ops` 保存、有效性判断和 `get_type()`。
 * 需要绑定实例 view 时使用 `type_capability_base<Ops, View>`，基类会自动提供 `view_type` 和
 * `bind(any_ref)`。
 */
template <typename Ops, typename View = void> class type_capability_base;

template <typename Ops> class type_capability_base<Ops, void> {
public:
  using ops_type = std::remove_cv_t<std::remove_reference_t<Ops>>;

  type_capability_base() noexcept = default;

  explicit type_capability_base(meta::type type) noexcept
      : type_(type), ops_(type.get_raw_ops<ops_type>()) {}

  [[nodiscard]] bool valid() const noexcept { return type_ && detail::capability_ops_valid(ops_); }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type get_type() const noexcept { return valid() ? type_ : meta::type{}; }

protected:
  meta::type type_{};
  const ops_type* ops_ = nullptr;
};

template <typename Ops, typename View>
class type_capability_base : public type_capability_base<Ops, void> {
public:
  using base_type = type_capability_base<Ops, void>;
  using ops_type = typename base_type::ops_type;
  using view_type = std::remove_cv_t<std::remove_reference_t<View>>;

  using base_type::base_type;

  [[nodiscard]] view_type bind(any_ref source) const;
};

namespace detail {

template <typename Capability, typename = void> struct is_auto_type_capability : std::false_type {};

template <typename Capability>
struct is_auto_type_capability<
    Capability,
    std::void_t<typename std::remove_cv_t<std::remove_reference_t<Capability>>::ops_type>>
    : std::bool_constant<std::is_constructible_v<
                             std::remove_cv_t<std::remove_reference_t<Capability>>, meta::type> &&
                         std::is_base_of_v<type_capability_base<typename std::remove_cv_t<
                                               std::remove_reference_t<Capability>>::ops_type>,
                                           std::remove_cv_t<std::remove_reference_t<Capability>>>> {
};

template <typename Capability>
inline constexpr bool is_auto_type_capability_v = is_auto_type_capability<Capability>::value;

} // namespace detail

template <typename Capability>
struct type_capability_traits<Capability,
                              std::enable_if_t<detail::is_auto_type_capability_v<Capability>>> {
  using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
  using result_type = capability_type;
  using ops_type = typename capability_type::ops_type;

  [[nodiscard]] static result_type get(const type& source) noexcept { return result_type{source}; }

  [[nodiscard]] static const ops_type* get_raw(const type& source) noexcept {
    return type_capability_traits<ops_type>::get_raw(source);
  }

  [[nodiscard]] static bool has(const type& source) noexcept { return get(source).valid(); }
};

/** @brief 类型运行期创建能力的公开 facade。 */
class factory_capability final : public type_capability_base<factory_ops> {
public:
  using base_type = type_capability_base<factory_ops>;

  factory_capability() noexcept = default;
  explicit factory_capability(meta::type type) noexcept : base_type(type) {}

  /** @brief 使用注册工厂构造并返回拥有对象生命周期的 `any`。 */
  [[nodiscard]] any construct(memory::memory_resource* resource = nullptr) const;

  /** @brief 使用注册工厂构造并返回可显式转移所有权的运行期值。 */
  [[nodiscard]] owned_value construct_owned(memory::memory_resource* resource = nullptr) const;
};

/** @brief 类型运行期带参构造能力的公开 facade。 */
class constructor_capability final : public type_capability_base<constructor_ops> {
public:
  using base_type = type_capability_base<constructor_ops>;

  constructor_capability() noexcept = default;
  explicit constructor_capability(meta::type type) noexcept : base_type(type) {}

  /** @brief 返回当前构造函数接受的参数数量。 */
  [[nodiscard]] std::size_t argument_count() const noexcept {
    return valid() ? ops_->argument_count : 0;
  }

  /** @brief 判断给定参数数量是否匹配当前构造函数。 */
  [[nodiscard]] bool matches(std::size_t count) const noexcept {
    return valid() && ops_->argument_count == count;
  }

  /** @brief 使用注册构造函数构造并返回拥有对象生命周期的 `any`。 */
  [[nodiscard]] any construct(std::span<const any_ref> arguments,
                              memory::memory_resource* resource = nullptr) const;

  /** @brief 使用注册构造函数构造并返回可显式转移所有权的运行期值。 */
  [[nodiscard]] owned_value construct_owned(std::span<const any_ref> arguments,
                                            memory::memory_resource* resource = nullptr) const;
};

} // namespace caors::meta

#endif
