// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_OPS_VARIANT_OPS_H_
#define CAORS_META_OPS_VARIANT_OPS_H_

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/meta/type.h"

#include <cstddef>

namespace caors::meta {

/** @brief variant-like 单个候选类型运行期访问操作表。 */
struct variant_alternative_ops {
  using get_value_type_fn = type (*)() noexcept;
  using const_alternative_fn = const void* (*)(const void*) noexcept;
  using mutable_alternative_fn = void* (*)(void*) noexcept;
  using emplace_fn = bool (*)(void*, memory::memory_resource*);
  using set_fn = bool (*)(void*, const void*);

  const_alternative_fn alternative = nullptr;
  mutable_alternative_fn mutable_alternative = nullptr;
  emplace_fn emplace = nullptr;
  set_fn set = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_value_type != nullptr && alternative != nullptr;
  }
};

/** @brief variant-like 运行期访问操作表。 */
struct variant_ops {
  using index_fn = std::size_t (*)(const void*) noexcept;

  vector<variant_alternative_ops> alternatives{};
  index_fn index = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    if (index == nullptr || alternatives.empty()) {
      return false;
    }

    for (const variant_alternative_ops& alternative : alternatives) {
      if (!alternative.valid()) {
        return false;
      }
    }

    return true;
  }
};

} // namespace caors::meta

#endif
