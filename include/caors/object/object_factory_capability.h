// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_OBJECT_FACTORY_CAPABILITY_H_
#define CAORS_OBJECT_OBJECT_FACTORY_CAPABILITY_H_

#include "caors/meta/type.h"
#include "caors/object/detail/factory_ops.h"

namespace caors::objects {

/** @brief 对象默认创建和销毁能力的公开 facade。 */
class object_factory_capability final : public meta::type_capability_base<object_factory_ops> {
public:
  using base_type = meta::type_capability_base<object_factory_ops>;

  object_factory_capability() noexcept = default;
  explicit object_factory_capability(meta::type type) noexcept : base_type(type) {}

  /** @brief 判断该对象类型是否支持默认构造。 */
  [[nodiscard]] bool constructible() const noexcept {
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
};

} // namespace caors::objects

#endif
