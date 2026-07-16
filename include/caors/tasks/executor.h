// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_EXECUTOR_H_
#define CAORS_TASKS_EXECUTOR_H_

#include "caors/error.h"
#include "caors/tasks/task.h"

namespace caors::tasks {

/**
 * @brief 任务提交抽象。
 *
 * executor 只描述“如何提交一个任务”，不规定线程模型、队列策略、取消语义或结果保存方式。
 */
class executor {
public:
  executor() = default;
  executor(const executor&) = delete;
  executor& operator=(const executor&) = delete;
  executor(executor&&) = delete;
  executor& operator=(executor&&) = delete;
  virtual ~executor() = default;

  /** @brief 提交任务。空任务返回 `errc::invalid_argument`。 */
  [[nodiscard]] virtual error submit(task next) = 0;
};

} // namespace caors::tasks

#endif
