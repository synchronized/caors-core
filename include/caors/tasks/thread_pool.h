// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_THREAD_POOL_H_
#define CAORS_TASKS_THREAD_POOL_H_

#include "caors/export.h"
#include "caors/memory/resource.h"
#include "caors/tasks/executor.h"

#include <cstddef>

namespace caors::tasks {

/** @brief 线程池关闭策略。 */
enum class shutdown_mode {
  /** @brief 执行完已经投递的任务。 */
  drain,
  /** @brief 丢弃尚未开始的任务，已经开始的任务继续完成。 */
  cancel,
};

/** @brief 固定线程数的 Asio 后台执行环境。 */
class CAORS_API thread_pool final {
public:
  [[nodiscard]] static std::size_t default_thread_count() noexcept;

  explicit thread_pool(std::size_t thread_count = default_thread_count(),
                       memory_resource* resource = nullptr);
  explicit thread_pool(memory_resource* resource);
  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;
  ~thread_pool() noexcept;

  /** @brief 获取可复制的轻量任务提交句柄。 */
  [[nodiscard]] executor get_executor() const noexcept;
  [[nodiscard]] memory_resource* resource() const noexcept;
  [[nodiscard]] std::size_t thread_count() const noexcept;
  /** @brief 返回已经投递但尚未开始执行的任务数量。 */
  [[nodiscard]] std::size_t pending() const noexcept;
  [[nodiscard]] bool stopping() const noexcept;

  /**
   * @brief 关闭线程池；多次调用是安全的。
   * @note 不得从该线程池正在执行的任务内部调用；等待自身 worker 退出会造成死锁。
   */
  void shutdown(shutdown_mode mode = shutdown_mode::drain) noexcept;

private:
  detail::executor_control_block* control_ = nullptr;
  executor executor_;
};

} // namespace caors::tasks

#endif
