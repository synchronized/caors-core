// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TEMPORARY_MEMORY_H_
#define CAORS_META_DETAIL_TEMPORARY_MEMORY_H_

#include "caors/memory/stack.h"

#include <optional>

namespace caors::meta::detail {

/** @brief 为一次 meta 操作解析临时资源，并在栈资源上建立自动回滚作用域。 */
class temporary_memory_scope final {
public:
  explicit temporary_memory_scope(memory::memory_resource* resource)
      : resource_(resource != nullptr ? resource : &memory::library_temporary_resource()) {
    if (auto* stack = dynamic_cast<memory::stack_resource*>(resource_); stack != nullptr) {
      stack_scope_.emplace(*stack);
    }
  }

  temporary_memory_scope(const temporary_memory_scope&) = delete;
  temporary_memory_scope& operator=(const temporary_memory_scope&) = delete;

  [[nodiscard]] memory::memory_resource* resource() const noexcept { return resource_; }

private:
  memory::memory_resource* resource_ = nullptr;
  std::optional<memory::stack_scope> stack_scope_{};
};

} // namespace caors::meta::detail

#endif
