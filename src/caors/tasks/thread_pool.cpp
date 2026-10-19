// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/thread_pool.h"

#include "caors/config.h"
#include "executor_control_block.h"
#include "task_queue.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace caors::tasks {
namespace {

struct pool_control final : detail::executor_control_block {
  using task_queue = detail::task_queue;

  pool_control(memory_resource* owner, std::size_t count)
      : queue(owner), discarded(owner), workers(caors::allocator<std::thread>{owner}),
        thread_count(count) {
    resource = owner;
    post = &post_task;
    valid = &is_valid;
    destroy = &destroy_control;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      workers.reserve(count);
      for (std::size_t index = 0; index < count; ++index) {
        workers.emplace_back([this] { run_worker(); });
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      shutdown(shutdown_mode::cancel);
      throw;
    }
#endif
  }

  ~pool_control() noexcept { shutdown(shutdown_mode::drain); }

  void run_worker() noexcept {
    for (;;) {
      task next;
      {
        std::unique_lock lock{mutex};
        condition.wait(lock, [this] { return is_stopping || !queue.empty(); });
        if (queue.empty()) {
          return;
        }
        next = queue.take_front();
      }
#if CAORS_HAS_EXCEPTIONS
      try {
#endif
        (void)next.run();
#if CAORS_HAS_EXCEPTIONS
      } catch (...) {
        // 单个任务抛出异常不能终止 worker 或遗留队列中的其他请求。
      }
#endif
    }
  }

  static error post_task(detail::executor_control_block* base, task next) {
    auto* control = static_cast<pool_control*>(base);
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      {
        std::lock_guard lock{control->mutex};
        if (control->is_stopping) {
          return {error_domain::core, errc::frozen};
        }
        control->queue.push_back(std::move(next));
      }
      control->condition.notify_one();
      return {};
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      return {error_domain::core, errc::out_of_memory};
    }
#endif
  }

  static bool is_valid(const detail::executor_control_block* base) noexcept {
    const auto* control = static_cast<const pool_control*>(base);
    std::lock_guard lock{control->mutex};
    return !control->is_stopping;
  }

  static void destroy_control(detail::executor_control_block* base) noexcept {
    auto* control = static_cast<pool_control*>(base);
    memory_resource* owner = control->resource;
    caors::destroy(control, owner);
  }

  void shutdown(shutdown_mode mode) noexcept {
    // 串行化关闭；停止标志和队列使用另一把锁，worker 退出不依赖此锁。
    std::lock_guard shutdown_lock{shutdown_mutex};
    {
      std::lock_guard lock{mutex};
      is_stopping = true;
      if (mode == shutdown_mode::cancel) {
        discarded.swap(queue);
      }
    }
    condition.notify_all();
    // 捕获对象析构可能重新投递或完成请求，必须在队列锁外销毁。
    discarded.clear();
    for (auto& worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  mutable std::mutex mutex;
  std::mutex shutdown_mutex;
  std::condition_variable condition;
  task_queue queue;
  task_queue discarded;
  std::vector<std::thread, caors::allocator<std::thread>> workers;
  std::size_t thread_count = 0;
  bool is_stopping = false;
};

[[nodiscard]] pool_control* as_pool(detail::executor_control_block* control) noexcept {
  return static_cast<pool_control*>(control);
}

} // namespace

std::size_t thread_pool::default_thread_count() noexcept {
  const unsigned int count = std::thread::hardware_concurrency();
  return count != 0 ? static_cast<std::size_t>(count) : 1U;
}

thread_pool::thread_pool(std::size_t count, memory_resource* resource) {
  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  const std::size_t resolved_count = count != 0 ? count : default_thread_count();
  control_ = caors::make<pool_control>(resolved, resolved, resolved_count);
  executor_ = executor{control_, true};
}

thread_pool::thread_pool(memory_resource* resource)
    : thread_pool(default_thread_count(), resource) {}

thread_pool::~thread_pool() noexcept {
  shutdown(shutdown_mode::drain);
  detail::release_executor_control(control_);
}

executor thread_pool::get_executor() const noexcept { return executor_; }

memory_resource* thread_pool::resource() const noexcept { return control_->resource; }

std::size_t thread_pool::thread_count() const noexcept { return as_pool(control_)->thread_count; }

std::size_t thread_pool::pending() const noexcept {
  const auto* control = static_cast<const pool_control*>(control_);
  std::lock_guard lock{control->mutex};
  return control->queue.size();
}

bool thread_pool::stopping() const noexcept {
  const auto* control = static_cast<const pool_control*>(control_);
  std::lock_guard lock{control->mutex};
  return control->is_stopping;
}

void thread_pool::shutdown(shutdown_mode mode) noexcept { as_pool(control_)->shutdown(mode); }

} // namespace caors::tasks
