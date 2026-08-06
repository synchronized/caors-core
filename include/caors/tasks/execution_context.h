// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_EXECUTION_CONTEXT_H_
#define CAORS_TASKS_EXECUTION_CONTEXT_H_

#include "caors/export.h"
#include "caors/memory/resource.h"
#include "caors/tasks/executor.h"

#include <cstddef>

namespace caors::tasks {

/** @brief 可由调用线程运行或轮询的任务执行环境。 */
class CAORS_API execution_context final {
public:
  explicit execution_context(memory_resource* resource = nullptr);
  execution_context(const execution_context&) = delete;
  execution_context& operator=(const execution_context&) = delete;
  execution_context(execution_context&&) = delete;
  execution_context& operator=(execution_context&&) = delete;
  ~execution_context() noexcept;

  /** @brief 获取可跨线程复制的任务提交句柄。 */
  [[nodiscard]] executor get_executor() const noexcept;

  /** @brief 运行任务，直到没有工作或 context 被停止。 */
  [[nodiscard]] std::size_t run();
  /** @brief 至多运行一个任务，可以阻塞等待工作。 */
  [[nodiscard]] std::size_t run_one();
  /** @brief 非阻塞运行调用开始时已经等待的任务，不执行本轮重入投递的任务。 */
  [[nodiscard]] std::size_t poll();
  /** @brief 非阻塞运行至多一个任务。 */
  [[nodiscard]] std::size_t poll_one();

  /** @brief 返回已经投递但尚未开始执行的任务数量。 */
  [[nodiscard]] std::size_t pending() const noexcept;
  /** @brief 丢弃尚未开始执行的任务；调用时不得有成员函数正在运行。 */
  void clear();

  /** @brief 停止运行并使当前 run 调用尽快返回。 */
  void stop() noexcept;
  /** @brief 允许停止后的 context 再次运行；调用时不得有成员函数正在运行。 */
  void restart();
  /** @brief 查询 context 是否处于停止状态。 */
  [[nodiscard]] bool stopped() const noexcept;
  /** @brief 查询 control block 使用的长期内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

private:
  detail::executor_control_block* control_ = nullptr;
};

} // namespace caors::tasks

#endif
