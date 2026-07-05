// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_DETAIL_INTERNAL_RESOURCE_H_
#define CAORS_MEMORY_DETAIL_INTERNAL_RESOURCE_H_

#include "caors/config.h"
#include "caors/memory/resource.h"

namespace caors::memory::detail {

/**
 * @brief CAORS 内部全局元数据使用的内存资源。
 *
 * 默认固定使用 system resource，避免静态初始化阶段尚未设置默认资源时，类型注册、
 * `string_name` 等全局表捕获到不可预期资源。需要调试内部元数据分配时，可以在编译期定义
 * `CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE=1`，让内部表跟随当前默认资源。
 */
[[nodiscard]] inline memory_resource& internal_resource() noexcept {
#if CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE
  return *get_default_memory_resource();
#else
  return get_system_memory_resource();
#endif
}

} // namespace caors::memory::detail

#endif
