// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESULT_H_
#define CAORS_RESULT_H_

#include "caors/error.h"

namespace caors {

/**
 * @brief 携带值的轻量返回结果。
 *
 * `result<T>` 保存一个错误状态和一个值。布尔上下文中 `true` 表示有可用值，`false` 表示失败。
 * 失败时 `status` 保存错误原因，`value` 保持默认构造状态。
 */
template <typename T, typename E = error> struct result {
  using value_type = T;
  using error_type = E;

  E status{};
  T value{};

  [[nodiscard]] bool succeeded() const noexcept { return !static_cast<bool>(status); }
  [[nodiscard]] bool failed() const noexcept { return static_cast<bool>(status); }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
};

} // namespace caors

#endif
