// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_DETAIL_INTERNAL_RESOURCE_H_
#define CAORS_MEMORY_DETAIL_INTERNAL_RESOURCE_H_

#include "caors/memory/resource.h"

namespace caors::memory::detail {

/**
 * @brief CAORS 内部全局元数据使用的内存资源。
 *
 * @deprecated 跨模块代码应使用 `memory::library_resource()`。
 */
[[nodiscard]] inline memory_resource& internal_resource() noexcept { return library_resource(); }

} // namespace caors::memory::detail

#endif
