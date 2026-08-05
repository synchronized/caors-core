// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_SCRATCH_MEMORY_H_
#define CAORS_SERIALIZATION_DETAIL_SCRATCH_MEMORY_H_

#include "caors/memory/stack.h"

#include <optional>

namespace caors::serialization::detail {

/**
 * @brief 为一次内部操作解析 scratch 资源，并在 stack_resource 上建立自动回滚作用域。
 *
 * 普通 memory_resource 仍依赖容器逐项释放；stack_resource 则在析构时统一回滚。
 */
class scratch_memory_scope final {
public:
  explicit scratch_memory_scope(memory::memory_resource* resource)
      : resource_(resource != nullptr ? resource : &memory::library_temporary_resource()) {
    if (auto* stack = dynamic_cast<memory::stack_resource*>(resource_); stack != nullptr) {
      stack_scope_.emplace(*stack);
    }
  }

  scratch_memory_scope(const scratch_memory_scope&) = delete;
  scratch_memory_scope& operator=(const scratch_memory_scope&) = delete;

  /** @brief 返回解析后的 scratch 资源。 */
  [[nodiscard]] memory::memory_resource* resource() const noexcept { return resource_; }

private:
  memory::memory_resource* resource_ = nullptr;
  std::optional<memory::stack_scope> stack_scope_{};
};

} // namespace caors::serialization::detail

#endif
