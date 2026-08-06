// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_INLINE_EXECUTOR_H_
#define CAORS_TASKS_INLINE_EXECUTOR_H_

#include "caors/tasks/executor.h"

namespace caors::tasks {

/**
 * @brief 在调用线程立即执行任务的迁移期包装器。
 *
 * 新代码可通过 `get_executor()` 取得统一轻量句柄。
 */
class CAORS_API inline_executor final {
public:
  inline_executor() noexcept;

  [[nodiscard]] executor get_executor() const noexcept { return executor_; }
  [[nodiscard]] error submit(task next) { return executor_.post(std::move(next)); }

  operator executor() const noexcept { return executor_; }

private:
  executor executor_;
};

} // namespace caors::tasks

#endif
