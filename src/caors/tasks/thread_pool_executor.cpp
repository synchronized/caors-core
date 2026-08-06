// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/thread_pool_executor.h"

#include "caors/config.h"
#include "caors/core/containers.h"
#include "executor_control_block.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace caors::tasks {

namespace {

struct pool_control final : detail::executor_control_block {
  using task_queue = std::deque<task, allocator<task>>;
  using worker_list = core::vector<std::jthread>;

  pool_control(memory_resource* owner, std::size_t count)
      : tasks(typename task_queue::allocator_type{owner}),
        workers(typename worker_list::allocator_type{owner}) {
    resource = owner;
    post = &post_task;
    valid = &is_valid;
    destroy = &destroy_control;
    start_workers(count);
  }

  ~pool_control() noexcept { shutdown(shutdown_mode::drain); }

  static error post_task(detail::executor_control_block* base, task next) {
    auto* control = static_cast<pool_control*>(base);
    {
      std::lock_guard lock{control->mutex};
      if (control->is_stopping) {
        return {error_domain::core, errc::frozen};
      }
      control->tasks.push_back(std::move(next));
    }
    control->condition.notify_one();
    return {};
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

  void start_workers(std::size_t count) {
    workers.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      workers.emplace_back([this](std::stop_token stop) { worker_loop(stop); });
    }
  }

  void worker_loop(std::stop_token stop) noexcept {
    while (true) {
      task current;
      {
        std::unique_lock lock{mutex};
        condition.wait(lock, stop, [this] { return is_stopping || !tasks.empty(); });
        if (tasks.empty()) {
          if (is_stopping || stop.stop_requested()) {
            return;
          }
          continue;
        }
        current = std::move(tasks.front());
        tasks.pop_front();
      }

#if CAORS_HAS_EXCEPTIONS
      try {
        (void)current.run();
      } catch (...) {
      }
#else
      (void)current.run();
#endif
    }
  }

  void shutdown(shutdown_mode mode) noexcept {
    {
      std::lock_guard lock{mutex};
      is_stopping = true;
      if (mode == shutdown_mode::cancel) {
        tasks.clear();
      }
    }
    condition.notify_all();
    const std::thread::id current = std::this_thread::get_id();
    for (std::jthread& worker : workers) {
      worker.request_stop();
    }
    for (std::jthread& worker : workers) {
      if (worker.joinable() && worker.get_id() != current) {
        worker.join();
      }
    }
  }

  mutable std::mutex mutex;
  std::condition_variable_any condition;
  bool is_stopping = false;
  task_queue tasks;
  worker_list workers;
};

[[nodiscard]] pool_control* as_pool(detail::executor_control_block* control) noexcept {
  return static_cast<pool_control*>(control);
}

} // namespace

std::size_t thread_pool_executor::default_thread_count() noexcept {
  const unsigned int count = std::thread::hardware_concurrency();
  return count != 0 ? static_cast<std::size_t>(count) : 1U;
}

thread_pool_executor::thread_pool_executor(std::size_t thread_count, memory_resource* resource) {
  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  const std::size_t resolved_count = thread_count != 0 ? thread_count : default_thread_count();
  control_ = caors::make<pool_control>(resolved, resolved, resolved_count);
  executor_ = executor{control_, true};
}

thread_pool_executor::thread_pool_executor(memory_resource* resource)
    : thread_pool_executor(default_thread_count(), resource) {}

thread_pool_executor::~thread_pool_executor() noexcept {
  shutdown(shutdown_mode::drain);
  detail::release_executor_control(control_);
}

executor thread_pool_executor::get_executor() const noexcept { return executor_; }

memory_resource* thread_pool_executor::resource() const noexcept { return control_->resource; }

std::size_t thread_pool_executor::thread_count() const noexcept {
  return as_pool(control_)->workers.size();
}

std::size_t thread_pool_executor::pending() const {
  const auto* control = static_cast<const pool_control*>(control_);
  std::lock_guard lock{control->mutex};
  return control->tasks.size();
}

bool thread_pool_executor::stopping() const {
  const auto* control = static_cast<const pool_control*>(control_);
  std::lock_guard lock{control->mutex};
  return control->is_stopping;
}

error thread_pool_executor::submit(task next) { return get_executor().post(std::move(next)); }

void thread_pool_executor::shutdown(shutdown_mode mode) noexcept {
  as_pool(control_)->shutdown(mode);
}

} // namespace caors::tasks
