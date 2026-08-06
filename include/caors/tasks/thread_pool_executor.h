// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_THREAD_POOL_EXECUTOR_H_
#define CAORS_TASKS_THREAD_POOL_EXECUTOR_H_

#include "caors/tasks/thread_pool.h"

namespace caors::tasks {

/** @brief 迁移期兼容名称；新代码使用 `thread_pool`。 */
using thread_pool_executor = thread_pool;

} // namespace caors::tasks

#endif
