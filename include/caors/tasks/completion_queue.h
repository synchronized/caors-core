// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_COMPLETION_QUEUE_H_
#define CAORS_TASKS_COMPLETION_QUEUE_H_

#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/tasks/task.h"

#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace caors::tasks {

/**
 * @brief 主线程完成回调队列。
 *
 * `completion_queue` 允许后台逻辑把完成回调投递到一个队列，再由主线程在明确时机调用 `drain()`。
 * 队列本身的 push、size、empty、clear 和 drain
 * 受互斥锁保护；任务执行期间不持锁，因此任务内部可以再次 push，新任务会留到下一次 drain。
 */
class completion_queue final {
public:
  explicit completion_queue(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        tasks_(typename task_queue::allocator_type{resource_}) {}

  completion_queue(const completion_queue&) = delete;
  completion_queue& operator=(const completion_queue&) = delete;
  completion_queue(completion_queue&&) = delete;
  completion_queue& operator=(completion_queue&&) = delete;

  /** @brief 查询内部队列使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 提交一个完成回调。空任务返回 `errc::invalid_argument`。 */
  [[nodiscard]] error push(task next) {
    if (!next) {
      return {error_domain::core, errc::invalid_argument};
    }

    std::lock_guard lock{mutex_};
    tasks_.push_back(std::move(next));
    return {};
  }

  /** @brief 返回当前队列长度。 */
  [[nodiscard]] std::size_t size() const {
    std::lock_guard lock{mutex_};
    return tasks_.size();
  }

  /** @brief 判断当前队列是否为空。 */
  [[nodiscard]] bool empty() const { return size() == 0; }

  /** @brief 丢弃全部未执行任务。 */
  void clear() {
    std::lock_guard lock{mutex_};
    tasks_.clear();
  }

  /**
   * @brief 执行当前已经入队的全部任务，并返回执行数量。
   *
   * 任务执行期间新增的任务不会在本轮 drain 中执行。`drain()`
   * 不聚合任务自身返回的错误；需要结果语义时， 调用方应把错误写入自己的请求状态，后续线程池 request
   * API 会在这一层之上定义。
   */
  [[nodiscard]] std::size_t drain() {
    task_queue pending{typename task_queue::allocator_type{resource_}};
    {
      std::lock_guard lock{mutex_};
      pending.swap(tasks_);
    }

    std::size_t executed = 0;
    for (task& current : pending) {
      (void)current.run();
      ++executed;
    }

    return executed;
  }

private:
  using task_queue = std::deque<task, allocator<task>>;

  memory_resource* resource_ = nullptr;
  mutable std::mutex mutex_;
  task_queue tasks_;
};

} // namespace caors::tasks

namespace caors {

using task_completion_queue = tasks::completion_queue;

} // namespace caors

#endif
