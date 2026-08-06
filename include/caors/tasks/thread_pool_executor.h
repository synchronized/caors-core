// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_THREAD_POOL_EXECUTOR_H_
#define CAORS_TASKS_THREAD_POOL_EXECUTOR_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/memory/resource.h"
#include "caors/tasks/executor.h"

#include <cstddef>

namespace caors::tasks {

enum class shutdown_mode { drain, cancel };

/**
 * @brief 固定线程数后台执行环境。
 *
 * 该名称在迁移期保留；新代码应通过 `get_executor()` 获取轻量提交句柄。
 */
class CAORS_API thread_pool_executor final {
public:
  [[nodiscard]] static std::size_t default_thread_count() noexcept;

  explicit thread_pool_executor(std::size_t thread_count = default_thread_count(),
                                memory_resource* resource = nullptr);
  explicit thread_pool_executor(memory_resource* resource);
  thread_pool_executor(const thread_pool_executor&) = delete;
  thread_pool_executor& operator=(const thread_pool_executor&) = delete;
  thread_pool_executor(thread_pool_executor&&) = delete;
  thread_pool_executor& operator=(thread_pool_executor&&) = delete;
  ~thread_pool_executor() noexcept;

  [[nodiscard]] executor get_executor() const noexcept;
  operator executor&() noexcept { return executor_; }
  operator const executor&() const noexcept { return executor_; }
  [[nodiscard]] memory_resource* resource() const noexcept;
  [[nodiscard]] std::size_t thread_count() const noexcept;
  [[nodiscard]] std::size_t pending() const;
  [[nodiscard]] bool stopping() const;

  /** @brief 迁移期转发入口；等价于 `get_executor().post(next)`。 */
  [[nodiscard]] error submit(task next);
  void shutdown(shutdown_mode mode = shutdown_mode::drain) noexcept;

private:
  detail::executor_control_block* control_ = nullptr;
  executor executor_;
};

} // namespace caors::tasks

#endif
