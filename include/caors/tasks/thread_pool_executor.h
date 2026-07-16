// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_THREAD_POOL_EXECUTOR_H_
#define CAORS_TASKS_THREAD_POOL_EXECUTOR_H_

#include "caors/config.h"
#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/tasks/executor.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace caors::tasks {

/** @brief 线程池关闭策略。 */
enum class shutdown_mode {
  /** @brief 停止接收新任务，执行完已经入队的任务后退出 worker。 */
  drain,
  /** @brief 停止接收新任务，丢弃尚未开始执行的任务，当前正在执行的任务会继续完成。 */
  cancel,
};

/**
 * @brief 固定线程数后台执行器。
 *
 * `thread_pool_executor` 在构造时启动固定数量 worker。`submit()` 把任务加入共享队列，worker
 * 后台取出并执行。析构时默认使用
 * `shutdown_mode::drain`，即不再接收新任务，但会执行完已经入队的任务。
 *
 * 第一版线程池只提供最小执行语义：不暴露任务结果、不传播取消、不保存异常。任务 callable
 * 不应抛异常；当启用异常时，worker 会捕获所有逃逸异常以避免后台线程终止进程，后续 request API
 * 再定义诊断保存方式。
 */
class thread_pool_executor final : public executor {
public:
  /** @brief 返回默认 worker 数量。当前使用硬件并发数，无法获取时退回 1。 */
  [[nodiscard]] static std::size_t default_thread_count() noexcept {
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    return hardware_threads != 0 ? static_cast<std::size_t>(hardware_threads) : 1U;
  }

  explicit thread_pool_executor(std::size_t thread_count = default_thread_count(),
                                memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        tasks_(typename task_queue::allocator_type{resource_}),
        workers_(typename worker_list::allocator_type{resource_}) {
    start_workers(thread_count != 0 ? thread_count : default_thread_count());
  }

  explicit thread_pool_executor(memory_resource* resource)
      : thread_pool_executor(default_thread_count(), resource) {}

  thread_pool_executor(const thread_pool_executor&) = delete;
  thread_pool_executor& operator=(const thread_pool_executor&) = delete;
  thread_pool_executor(thread_pool_executor&&) = delete;
  thread_pool_executor& operator=(thread_pool_executor&&) = delete;

  ~thread_pool_executor() noexcept override { shutdown(shutdown_mode::drain); }

  /** @brief 查询线程池内部容器使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询 worker 数量。 */
  [[nodiscard]] std::size_t thread_count() const noexcept { return workers_.size(); }

  /** @brief 查询当前尚未开始执行的任务数量。 */
  [[nodiscard]] std::size_t pending() const {
    std::lock_guard lock{mutex_};
    return tasks_.size();
  }

  /** @brief 查询线程池是否已经开始关闭。 */
  [[nodiscard]] bool stopping() const {
    std::lock_guard lock{mutex_};
    return stopping_;
  }

  /**
   * @brief 提交任务。
   *
   * 空任务返回 `errc::invalid_argument`；线程池关闭后提交任务返回 `errc::frozen`。
   */
  [[nodiscard]] error submit(task next) override {
    if (!next) {
      return {error_domain::core, errc::invalid_argument};
    }

    {
      std::lock_guard lock{mutex_};
      if (stopping_) {
        return {error_domain::core, errc::frozen};
      }

      tasks_.push_back(std::move(next));
    }

    condition_.notify_one();
    return {};
  }

  /**
   * @brief 关闭线程池并等待 worker 退出。
   *
   * `drain` 会执行完已入队任务；`cancel`
   * 会丢弃尚未开始执行的任务。已经开始执行的任务不会被强制中断。多次调用是安全的。
   */
  void shutdown(shutdown_mode mode = shutdown_mode::drain) noexcept {
    {
      std::lock_guard lock{mutex_};
      stopping_ = true;
      if (mode == shutdown_mode::cancel) {
        tasks_.clear();
      }
    }

    condition_.notify_all();

    const std::thread::id current_thread = std::this_thread::get_id();
    for (std::jthread& worker : workers_) {
      worker.request_stop();
    }
    for (std::jthread& worker : workers_) {
      if (worker.joinable() && worker.get_id() != current_thread) {
        worker.join();
      }
    }
  }

private:
  using task_queue = std::deque<task, allocator<task>>;
  using worker_list = core::vector<std::jthread>;

  void start_workers(std::size_t thread_count) {
    workers_.reserve(thread_count);
    for (std::size_t index = 0; index < thread_count; ++index) {
      workers_.emplace_back([this](std::stop_token stop) { worker_loop(stop); });
    }
  }

  void worker_loop(std::stop_token stop) noexcept {
    while (true) {
      task current;
      {
        std::unique_lock lock{mutex_};
        condition_.wait(lock, stop, [this] { return stopping_ || !tasks_.empty(); });

        if (tasks_.empty()) {
          if (stopping_ || stop.stop_requested()) {
            return;
          }

          continue;
        }

        current = std::move(tasks_.front());
        tasks_.pop_front();
      }

      run_worker_task(current);
    }
  }

  static void run_worker_task(task& current) noexcept {
#if CAORS_HAS_EXCEPTIONS
    try {
      (void)current.run();
    } catch (...) {
    }
#else
    (void)current.run();
#endif
  }

  memory_resource* resource_ = nullptr;
  mutable std::mutex mutex_;
  std::condition_variable_any condition_;
  bool stopping_ = false;
  task_queue tasks_;
  worker_list workers_;
};

} // namespace caors::tasks

#endif
