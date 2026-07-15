// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_FACTORY_CAPABILITY_H_
#define CAORS_OBJECT_FACTORY_CAPABILITY_H_

#include "caors/meta/type.h"
#include "caors/object/detail/factory_ops.h"

namespace caors::objects {

/** @brief 对象默认创建和销毁能力的公开 facade。 */
class factory_capability final {
public:
  constexpr factory_capability() noexcept = default;
  explicit constexpr factory_capability(const object_factory_ops* ops) noexcept : ops_(ops) {}

  [[nodiscard]] constexpr bool valid() const noexcept { return ops_ != nullptr; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

  /** @brief 判断该对象类型是否支持默认构造。 */
  [[nodiscard]] constexpr bool constructible() const noexcept {
    return ops_ != nullptr && ops_->constructible();
  }

  /** @brief 尝试创建对象；能力无效、类型不可默认构造或分配失败时返回空指针。 */
  [[nodiscard]] object* try_create(memory_resource* resource = nullptr) const noexcept {
    return constructible() && ops_->try_create != nullptr ? ops_->try_create(resource) : nullptr;
  }

  /** @brief 创建对象；能力无效或类型不可默认构造时返回空指针。 */
  [[nodiscard]] object* create(memory_resource* resource = nullptr) const {
    return constructible() && ops_->create != nullptr ? ops_->create(resource) : nullptr;
  }

  /** @brief 销毁对象；能力无效、对象为空或类型没有销毁函数时返回 false。 */
  bool destroy(object* instance, memory_resource* resource = nullptr) const noexcept {
    if (!valid() || instance == nullptr || ops_->destroy == nullptr) {
      return false;
    }

    ops_->destroy(instance, resource);
    return true;
  }

private:
  const object_factory_ops* ops_ = nullptr;
};

} // namespace caors::objects

namespace caors::meta {

template <> struct type_capability_traits<objects::factory_capability> {
  using result_type = objects::factory_capability;

  [[nodiscard]] static result_type get(const type& source) noexcept {
    return objects::factory_capability{source.get_ops<objects::object_factory_ops>()};
  }

  [[nodiscard]] static const objects::object_factory_ops* get_raw(const type& source) noexcept {
    return type_capability_traits<objects::object_factory_ops>::get_raw(source);
  }

  [[nodiscard]] static bool has(const type& source) noexcept { return get(source).valid(); }
};

} // namespace caors::meta

#endif
