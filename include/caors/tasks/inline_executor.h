// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_INLINE_EXECUTOR_H_
#define CAORS_TASKS_INLINE_EXECUTOR_H_

#include "caors/tasks/executor.h"

namespace caors::tasks {

/**
 * @brief 在调用线程立即执行任务的 executor。
 *
 * `inline_executor` 主要用于测试、确定性执行和暂未接入后台线程时的同步 fallback。
 */
class inline_executor final : public executor {
public:
  [[nodiscard]] error submit(task next) override { return next.run(); }
};

} // namespace caors::tasks

namespace caors {

using inline_task_executor = tasks::inline_executor;

} // namespace caors

#endif
