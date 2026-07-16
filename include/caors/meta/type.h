// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_H_
#define CAORS_META_TYPE_H_

#include "caors/core/string_name.h"
#include "caors/export.h"
#include "caors/meta/capability.h"
#include "caors/meta/type_id.h"
#include "caors/meta/value_ops.h"
#include "caors/traits/wrapper.h"

#include <span>
#include <type_traits>

namespace caors::meta {

class registry;
class type;
class any;
class any_ref;
class type_registration_data;

[[nodiscard]] CAORS_API registry& get_registry() noexcept;

namespace detail {

struct type_descriptor;

[[nodiscard]] CAORS_API type make_type_view(type_id id, const type_descriptor* descriptor) noexcept;

} // namespace detail

/** @brief 类型运行期形态。 */
enum class type_kind {
  value,
  enumeration,
  raw_pointer,
  wrapper,
  sequence,
  set,
  associative,
  pair,
  tuple,
  variant,
};

using wrapper_semantics = traits::wrapper_semantics;

/**
 * @brief 元类型只读视图。
 *
 * `meta::type` 不拥有类型数据，只引用编译期生成的 type descriptor，并在类型已注册时额外引用
 * 全局 `meta::registry` 中 append-only 保存的注册数据。视图复制成本低，适合作为对外查询结果。
 */
class CAORS_API type {
public:
  constexpr type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool registered() const noexcept;

  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] type_id parent_id() const noexcept;
  [[nodiscard]] type parent() const noexcept;
  [[nodiscard]] type_kind kind() const noexcept;
  [[nodiscard]] wrapper_semantics wrapper() const noexcept;
  [[nodiscard]] bool indirect() const noexcept;
  [[nodiscard]] bool nullable() const noexcept;
  [[nodiscard]] type get_value_type() const noexcept;
  [[nodiscard]] type get_key_type() const noexcept;

  /**
   * @brief 查询该类型视图具备的能力。
   *
   * 默认返回 `const Capability*`；公开 facade 可通过 `type_capability_traits`
   * 特化返回轻量能力对象。
   */
  template <typename Capability> [[nodiscard]] auto get_capability() const noexcept {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    return type_capability_traits<capability_type>::get(*this);
  }

  /** @brief 判断该类型视图是否具备指定能力。 */
  template <typename Capability> [[nodiscard]] bool has_capability() const noexcept {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    return type_capability_traits<capability_type>::has(*this);
  }

  /**
   * @brief 查询该类型视图具备的 raw ops 函数表。
   *
   * `*_ops` 是 CAORS 内部和高级扩展使用的类型擦除函数表。普通实例访问应优先通过
   * `any_ref::get_capability<Capability>()` 获取绑定后的 view。
   */
  template <typename Ops> [[nodiscard]] const Ops* get_raw_ops() const noexcept {
    using ops_type = std::remove_cv_t<std::remove_reference_t<Ops>>;
    return type_capability_traits<ops_type>::get_raw(*this);
  }

  /** @brief 判断该类型视图是否具备指定 raw ops 函数表。 */
  template <typename Ops> [[nodiscard]] bool has_raw_ops() const noexcept {
    return get_raw_ops<Ops>() != nullptr;
  }

private:
  friend class registry;
  friend type detail::make_type_view(type_id, const detail::type_descriptor*) noexcept;
  friend const void* detail::find_type_capability(const type&, capability_key) noexcept;

  constexpr type(const registry* registry, const type_registration_data* data,
                 const detail::type_descriptor* descriptor) noexcept
      : registry_(registry), data_(data), descriptor_(descriptor) {}

  const registry* registry_ = nullptr;
  const type_registration_data* data_ = nullptr;
  const detail::type_descriptor* descriptor_ = nullptr;
};

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

class factory_capability;
class constructor_capability;

namespace detail {

[[nodiscard]] void* factory_construct(const factory_capability& capability,
                                      memory::memory_resource* resource);

[[nodiscard]] factory_ops::destroy_fn
factory_destroy_fn(const factory_capability& capability) noexcept;

[[nodiscard]] void* constructor_construct(const constructor_capability& capability,
                                          memory::memory_resource* resource,
                                          std::span<const any_ref> arguments);

[[nodiscard]] constructor_ops::destroy_fn
constructor_destroy_fn(const constructor_capability& capability) noexcept;

} // namespace detail

/** @brief 类型运行期创建能力的公开 facade。 */
class factory_capability final : public type_capability_base<factory_ops> {
public:
  using base_type = type_capability_base<factory_ops>;

  factory_capability() noexcept = default;
  explicit factory_capability(meta::type type) noexcept : base_type(type) {}

  /** @brief 使用注册工厂构造并返回拥有对象生命周期的 `any`。 */
  [[nodiscard]] any construct_any(memory::memory_resource* resource = nullptr) const;

private:
  friend void* detail::factory_construct(const factory_capability&, memory::memory_resource*);
  friend factory_ops::destroy_fn detail::factory_destroy_fn(const factory_capability&) noexcept;
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
  [[nodiscard]] any construct_any(std::span<const any_ref> arguments,
                                  memory::memory_resource* resource = nullptr) const;

private:
  friend void* detail::constructor_construct(const constructor_capability&,
                                             memory::memory_resource*, std::span<const any_ref>);
  friend constructor_ops::destroy_fn
  detail::constructor_destroy_fn(const constructor_capability&) noexcept;
};

namespace detail {

/** @brief 使用注册 factory 构造裸对象，仅供内部所有权接管路径使用。 */
[[nodiscard]] inline void* factory_construct(const factory_capability& capability,
                                             memory::memory_resource* resource) {
  return capability.valid() ? capability.ops_->construct(resource) : nullptr;
}

/** @brief 查询注册 factory 的销毁策略，仅供内部所有权接管路径使用。 */
[[nodiscard]] inline factory_ops::destroy_fn
factory_destroy_fn(const factory_capability& capability) noexcept {
  return capability.valid() ? capability.ops_->destroy : nullptr;
}

/** @brief 使用注册 constructor 构造裸对象，仅供内部所有权接管路径使用。 */
[[nodiscard]] inline void* constructor_construct(const constructor_capability& capability,
                                                 memory::memory_resource* resource,
                                                 std::span<const any_ref> arguments) {
  return capability.matches(arguments.size()) ? capability.ops_->construct(resource, arguments)
                                              : nullptr;
}

/** @brief 查询注册 constructor 的销毁策略，仅供内部所有权接管路径使用。 */
[[nodiscard]] inline constructor_ops::destroy_fn
constructor_destroy_fn(const constructor_capability& capability) noexcept {
  return capability.valid() ? capability.ops_->destroy : nullptr;
}

} // namespace detail

} // namespace caors::meta

#endif
