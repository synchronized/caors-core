// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_COMPONENT_STORAGE_H_
#define CAORS_META_COMPONENT_STORAGE_H_

namespace caors::meta {

/** @brief 指定类型上的元数据组件挂载状态。 */
enum class component_status {
  invalid_type,
  missing_type,
  missing_component,
  present,
};

} // namespace caors::meta

#endif
