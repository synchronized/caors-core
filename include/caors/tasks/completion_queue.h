// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_COMPLETION_QUEUE_H_
#define CAORS_TASKS_COMPLETION_QUEUE_H_

#include "caors/tasks/execution_context.h"

#include <cstddef>
#include <utility>

namespace caors::tasks {

/**
 * @brief 主线程完成回调队列。
 *
 * 该类型是 `execution_context` 的兼容包装；后台线程可以通过 `push()` 投递，拥有线程通过
 * `drain()` 执行调用开始时已经等待的任务。本轮执行期间新投递的任务留到下一次 drain。
 */
class completion_queue final {
public:
  explicit completion_queue(memory_resource* resource = nullptr)
      : context_(resource), executor_(context_.get_executor()) {}

  completion_queue(const completion_queue&) = delete;
  completion_queue& operator=(const completion_queue&) = delete;
  completion_queue(completion_queue&&) = delete;
  completion_queue& operator=(completion_queue&&) = delete;

  /** @brief 查询内部执行环境使用的长期内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return context_.resource(); }

  /** @brief 获取兼容包装内部的轻量执行句柄。 */
  [[nodiscard]] executor get_executor() const noexcept { return executor_; }

  /** @brief 提交一个完成回调。空任务返回 `errc::invalid_argument`。 */
  [[nodiscard]] error push(task next) { return executor_.post(std::move(next)); }

  /** @brief 返回已经投递但尚未开始执行的任务数量。 */
  [[nodiscard]] std::size_t size() const noexcept { return context_.pending(); }

  /** @brief 判断当前队列是否为空。 */
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /** @brief 丢弃全部未执行任务；调用时不得正在执行 `drain()`。 */
  void clear() { context_.clear(); }

  /**
   * @brief 执行当前已经入队的全部任务，并返回执行数量。
   *
   * 任务执行期间新增的任务不会在本轮 drain 中执行。任务自身返回的错误不会被聚合；需要结果语义时，
   * 调用方应把错误写入自己的请求状态。
   */
  [[nodiscard]] std::size_t drain() {
    if (context_.stopped()) {
      context_.restart();
    }
    return context_.poll();
  }

private:
  execution_context context_;
  executor executor_;
};

} // namespace caors::tasks

#endif
