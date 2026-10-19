// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_EXTERNAL_EXECUTOR_H_
#define CAORS_TASKS_EXTERNAL_EXECUTOR_H_

#include "caors/tasks/executor.h"

#include <memory>

namespace caors::tasks {

/**
 * @brief 宿主调度上下文的拥有型适配协议。
 *
 * post/close 必须支持并发。post 接管任务，包括拒绝路径；接受后必须执行或销毁任务，不能永久遗留。
 * close 必须停止接受新任务并执行或销毁全部已接受任务；返回前排空正在执行的任务。
 * 不得在其工作任务内部调用阻塞式 close。允许 post 在调用线程同步执行，宿主负责重入安全。
 * 上下文及其模块/allocator 必须活到最后任务销毁；禁止在任务执行中强制卸载实现模块。
 */
class CAORS_API external_executor_context {
public:
  virtual ~external_executor_context() = default;
  /** @brief 接管任务；成功表示接受，失败表示拒绝，拒绝后不得执行该任务。 */
  [[nodiscard]] virtual error post(task next) = 0;
  /** @brief 封闭并排空队列；应为幂等操作，且不得抛异常。 */
  virtual void close() noexcept = 0;
};

/**
 * @brief 将共享拥有的宿主上下文适配为 executor 的不可复制所有者。
 *
 * 析构调用 close；普通 executor 句柄可以晚于适配器存活，此时 post 返回 frozen。
 * 内存资源非拥有，必须活到最后 executor 句柄销毁。close 与句柄投递可并发；所有者自身不得并发析构。
 */
class CAORS_API external_executor final {
public:
  explicit external_executor(std::shared_ptr<external_executor_context> context,
                             memory_resource* resource = nullptr);
  external_executor(const external_executor&) = delete;
  external_executor& operator=(const external_executor&) = delete;
  ~external_executor() noexcept;

  /** @brief 获取可复制的非拥有调度句柄；空上下文返回无效句柄。 */
  [[nodiscard]] executor get_executor() const noexcept;
  /** @brief 停止接受并排空任务；重复调用无效果。 */
  void close() noexcept;

private:
  detail::executor_control_block* control_ = nullptr;
};

} // namespace caors::tasks

#endif
