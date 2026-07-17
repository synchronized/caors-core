// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_OPS_H_
#define CAORS_META_VALUE_OPS_H_

#include "caors/memory/resource.h"

#include <cstddef>
#include <span>

namespace caors::meta {

class type;
class any_ref;

/** @brief 普通堆对象销毁函数。 */
using destroy_fn = void (*)(void*) noexcept;

/** @brief 使用指定内存资源销毁对象的函数。 */
using resource_destroy_fn = void (*)(memory::memory_resource*, void*) noexcept;

/** @brief C++ 类型布局信息。 */
struct type_layout {
  std::size_t size = 0;
  std::size_t alignment = 0;

  [[nodiscard]] bool valid() const noexcept { return size > 0 && alignment > 0; }
};

/** @brief C++ 值的类型擦除操作函数表。 */
struct value_ops {
  using default_construct_fn = void* (*)();
  using clone_fn = void* (*)(const void*);
  using destroy_fn = meta::destroy_fn;
  using assign_fn = bool (*)(void*, const void*);
  using resource_construct_fn = void* (*)(memory::memory_resource*);
  using resource_destroy_fn = meta::resource_destroy_fn;

  default_construct_fn default_construct = nullptr;
  clone_fn clone = nullptr;
  destroy_fn destroy = nullptr;
  assign_fn assign = nullptr;
  resource_construct_fn resource_construct = nullptr;
  resource_destroy_fn resource_destroy = nullptr;
};

/** @brief C++ 值的编译期特征摘要。 */
struct value_traits {
  bool trivially_copyable = false;
  bool trivially_destructible = false;
  bool trivially_default_constructible = false;
};

/** @brief C++ 值的布局、生命周期和存储相关操作。 */
struct value_storage_ops {
  type_layout layout{};
  value_ops ops{};
  value_traits traits{};

  [[nodiscard]] bool valid() const noexcept { return layout.valid(); }
};

/** @brief 类型运行期创建能力，负责按资源构造并销毁对象。 */
struct factory_ops {
  using construct_fn = value_ops::resource_construct_fn;
  using destroy_fn = meta::resource_destroy_fn;

  construct_fn construct = nullptr;
  destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return construct != nullptr && destroy != nullptr;
  }
};

/** @brief 带参数运行期构造能力，负责按 `any_ref` 参数列表构造并销毁对象。 */
struct constructor_ops {
  using construct_fn = void* (*)(memory::memory_resource*, std::span<const any_ref>);
  using destroy_fn = meta::resource_destroy_fn;

  construct_fn construct = nullptr;
  destroy_fn destroy = nullptr;
  std::size_t argument_count = 0;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return construct != nullptr && destroy != nullptr;
  }
};

} // namespace caors::meta

#endif
