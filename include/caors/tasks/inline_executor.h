// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_INLINE_EXECUTOR_H_
#define CAORS_TASKS_INLINE_EXECUTOR_H_

#include "caors/tasks/executor.h"

namespace caors::tasks {

/** @brief 返回在调用线程立即执行任务的轻量 executor。 */
[[nodiscard]] CAORS_API executor inline_executor() noexcept;

} // namespace caors::tasks

#endif
