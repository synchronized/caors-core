// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_DETAIL_TYPE_DATA_H_
#define CAORS_OBJECT_DETAIL_TYPE_DATA_H_

#include "caors/memory/resource.h"

namespace caors::objects {

class object;

namespace detail {

using try_create_fn = object* (*)(memory_resource * resource) noexcept;
using create_fn = object* (*)(memory_resource * resource);
using destroy_fn = void (*)(object* instance, memory_resource* resource) noexcept;

/** @brief 对象类型内部工厂数据。 */
struct type_data {
  try_create_fn try_create = nullptr;
  create_fn create = nullptr;
  destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool constructible() const noexcept {
    return try_create != nullptr && create != nullptr;
  }
};

} // namespace detail

} // namespace caors::objects

#endif
