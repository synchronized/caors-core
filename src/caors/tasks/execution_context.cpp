// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/execution_context.h"

#include "caors/config.h"
#include "executor_control_block.h"
#include "task_queue.h"

#include <mutex>
#include <utility>

namespace caors::tasks {
namespace {

struct context_control final : detail::executor_control_block {
  using task_queue = detail::task_queue;

  explicit context_control(memory_resource* owner) : queue(owner) {
    resource = owner;
    post = &post_task;
    valid = &is_valid;
    destroy = &destroy_control;
  }

  static error post_task(detail::executor_control_block* base, task next) {
    auto* control = static_cast<context_control*>(base);
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      std::lock_guard lock{control->mutex};
      if (!control->accepting) {
        return {error_domain::core, errc::frozen};
      }
      control->queue.push_back(std::move(next));
      return {};
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      return {error_domain::core, errc::out_of_memory};
    }
#endif
  }

  static bool is_valid(const detail::executor_control_block* base) noexcept {
    const auto* control = static_cast<const context_control*>(base);
    std::lock_guard lock{control->mutex};
    return control->accepting;
  }

  static void destroy_control(detail::executor_control_block* base) noexcept {
    auto* control = static_cast<context_control*>(base);
    memory_resource* owner = control->resource;
    caors::destroy(control, owner);
  }

  std::size_t execute_one() {
    task next;
    {
      std::lock_guard lock{mutex};
      if (is_stopped) {
        return 0;
      }
      if (queue.empty()) {
        is_stopped = true;
        return 0;
      }
      next = queue.take_front();
    }
    (void)next.run();
    return 1;
  }

  mutable std::mutex mutex;
  task_queue queue;
  bool accepting = true;
  bool is_stopped = false;
};

[[nodiscard]] context_control* as_context(detail::executor_control_block* control) noexcept {
  return static_cast<context_control*>(control);
}

} // namespace

execution_context::execution_context(memory_resource* resource) {
  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  control_ = caors::make<context_control>(resolved, resolved);
}

execution_context::~execution_context() noexcept {
  auto* control = as_context(control_);
  {
    std::lock_guard lock{control->mutex};
    control->accepting = false;
  }
  clear();
  detail::release_executor_control(control_);
}

executor execution_context::get_executor() const noexcept { return executor{control_, true}; }

std::size_t execution_context::run() {
  std::size_t executed = 0;
  while (run_one() != 0) {
    ++executed;
  }
  return executed;
}

std::size_t execution_context::run_one() { return as_context(control_)->execute_one(); }

std::size_t execution_context::poll() {
  const std::size_t snapshot = pending();
  std::size_t executed = 0;
  while (executed < snapshot && poll_one() != 0) {
    ++executed;
  }
  if (snapshot == 0) {
    auto* control = as_context(control_);
    std::lock_guard lock{control->mutex};
    if (control->queue.empty()) {
      control->is_stopped = true;
    }
  }
  return executed;
}

std::size_t execution_context::poll_one() { return as_context(control_)->execute_one(); }

std::size_t execution_context::drain() {
  restart();
  return poll();
}

std::size_t execution_context::pending() const noexcept {
  auto* control = as_context(control_);
  std::lock_guard lock{control->mutex};
  return control->queue.size();
}

void execution_context::clear() {
  auto* control = as_context(control_);
  context_control::task_queue discarded{control->resource};
  {
    std::lock_guard lock{control->mutex};
    discarded.swap(control->queue);
    control->is_stopped = false;
  }
  // 锁外销毁捕获对象，允许取消回调重新投递。
}

void execution_context::stop() noexcept {
  auto* control = as_context(control_);
  std::lock_guard lock{control->mutex};
  control->is_stopped = true;
}

void execution_context::restart() {
  auto* control = as_context(control_);
  std::lock_guard lock{control->mutex};
  control->is_stopped = false;
}

bool execution_context::stopped() const noexcept {
  auto* control = static_cast<const context_control*>(control_);
  std::lock_guard lock{control->mutex};
  return control->is_stopped;
}

memory_resource* execution_context::resource() const noexcept { return control_->resource; }

} // namespace caors::tasks
