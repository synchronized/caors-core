// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_OPS_TUPLE_OPS_H_
#define CAORS_META_OPS_TUPLE_OPS_H_

#include "caors/core/containers.h"
#include "caors/meta/type.h"

namespace caors::meta {

/** @brief tuple-like 单个元素运行期访问操作表。 */
struct tuple_element_ops {
  using get_value_type_fn = type (*)() noexcept;
  using const_element_fn = const void* (*)(const void*) noexcept;
  using mutable_element_fn = void* (*)(void*) noexcept;

  const_element_fn element = nullptr;
  mutable_element_fn mutable_element = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_value_type != nullptr && element != nullptr;
  }
};

/** @brief tuple-like 运行期访问操作表。 */
struct tuple_ops {
  vector<tuple_element_ops> elements{};

  [[nodiscard]] bool valid() const noexcept {
    for (const tuple_element_ops& element : elements) {
      if (!element.valid()) {
        return false;
      }
    }

    return true;
  }
};

} // namespace caors::meta

#endif
