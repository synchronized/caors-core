// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_DETAIL_FACTORY_OPS_H_
#define CAORS_OBJECT_DETAIL_FACTORY_OPS_H_

#include "caors/memory/resource.h"

namespace caors::objects {

class object;

/** @brief 对象默认构造、创建和销毁的类型擦除操作表。 */
struct object_factory_ops {
  using try_create_fn = object* (*)(memory_resource*) noexcept;
  using create_fn = object* (*)(memory_resource*);
  using destroy_fn = void (*)(object* instance, memory_resource* resource) noexcept;

  try_create_fn try_create = nullptr;
  create_fn create = nullptr;
  destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool constructible() const noexcept {
    return try_create != nullptr && create != nullptr;
  }
};

namespace detail {

using objects::object_factory_ops;

} // namespace detail

} // namespace caors::objects

#endif
