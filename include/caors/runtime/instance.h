// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RUNTIME_INSTANCE_H_
#define CAORS_RUNTIME_INSTANCE_H_

#include "caors/meta/instance.h"

namespace caors::runtime {

using instance = meta::instance;

/** @brief 从运行期引用创建实例视图。 */
[[nodiscard]] inline instance view(meta::any_ref value) noexcept { return meta::view(value); }

/** @brief 从 C++ 对象创建实例视图。 */
template <typename T> [[nodiscard]] instance view(T& value) { return meta::view(value); }

/** @brief 从只读 C++ 对象创建实例视图。 */
template <typename T> [[nodiscard]] instance view_const(const T& value) {
  return meta::view_const(value);
}

} // namespace caors::runtime

#endif
