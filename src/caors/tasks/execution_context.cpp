// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/execution_context.h"

#include "caors/config.h"
#include "executor_control_block.h"

#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include <atomic>
#include <mutex>
#include <utility>

namespace caors::tasks {

namespace {

struct context_control final : detail::executor_control_block {
  explicit context_control(memory_resource* owner) {
    resource = owner;
    post = &post_task;
    valid = &is_valid;
    destroy = &destroy_control;
    backend = caors::make<asio::io_context>(resource);
  }

  static error post_task(detail::executor_control_block* base, task next) {
    auto* control = static_cast<context_control*>(base);
    std::lock_guard lock{control->mutex};
    if (!control->accepting || control->backend == nullptr) {
      return {error_domain::core, errc::frozen};
    }

    control->pending.fetch_add(1, std::memory_order_relaxed);
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      asio::post(*control->backend, scheduled_task{control, std::move(next)});
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
    return control->accepting && control->backend != nullptr;
  }

  static void destroy_control(detail::executor_control_block* base) noexcept {
    auto* control = static_cast<context_control*>(base);
    memory_resource* owner = control->resource;
    caors::destroy(control, owner);
  }

  struct scheduled_task final {
    scheduled_task(context_control* owner, task next_task)
        : control(owner), next(std::move(next_task)) {
      detail::retain_executor_control(control);
    }

    scheduled_task(const scheduled_task&) = delete;
    scheduled_task& operator=(const scheduled_task&) = delete;

    scheduled_task(scheduled_task&& other) noexcept
        : control(std::exchange(other.control, nullptr)), next(std::move(other.next)),
          started(std::exchange(other.started, true)) {}

    ~scheduled_task() noexcept {
      if (control != nullptr) {
        if (!started) {
          control->pending.fetch_sub(1, std::memory_order_relaxed);
        }
        detail::release_executor_control(control);
      }
    }

    void operator()() {
      started = true;
      control->pending.fetch_sub(1, std::memory_order_relaxed);
      (void)next.run();
    }

    context_control* control = nullptr;
    task next;
    bool started = false;
  };

  ~context_control() noexcept {
    if (backend != nullptr) {
      backend->stop();
      caors::destroy(backend, resource);
    }
  }

  mutable std::mutex mutex;
  asio::io_context* backend = nullptr;
  std::atomic_size_t pending{0};
  bool accepting = true;
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
  context_control* control = as_context(control_);
  asio::io_context* backend = nullptr;
  {
    std::lock_guard lock{control->mutex};
    control->accepting = false;
    backend = std::exchange(control->backend, nullptr);
  }

  if (backend != nullptr) {
    backend->stop();
    caors::destroy(backend, control->resource);
  }
  detail::release_executor_control(control_);
}

executor execution_context::get_executor() const noexcept { return executor{control_, true}; }

std::size_t execution_context::run() { return as_context(control_)->backend->run(); }

std::size_t execution_context::run_one() { return as_context(control_)->backend->run_one(); }

std::size_t execution_context::poll() {
  context_control* control = as_context(control_);
  const std::size_t snapshot = control->pending.load(std::memory_order_acquire);
  std::size_t executed = 0;
  while (executed < snapshot) {
    const std::size_t current = control->backend->poll_one();
    if (current == 0) {
      break;
    }
    executed += current;
  }
  return executed;
}

std::size_t execution_context::poll_one() { return as_context(control_)->backend->poll_one(); }

std::size_t execution_context::pending() const noexcept {
  return as_context(control_)->pending.load(std::memory_order_relaxed);
}

void execution_context::clear() {
  context_control* control = as_context(control_);
  asio::io_context* replacement = caors::make<asio::io_context>(control->resource);
  asio::io_context* previous = nullptr;
  {
    std::lock_guard lock{control->mutex};
    previous = std::exchange(control->backend, replacement);
  }

  previous->stop();
  caors::destroy(previous, control->resource);
}

void execution_context::stop() noexcept { as_context(control_)->backend->stop(); }

void execution_context::restart() { as_context(control_)->backend->restart(); }

bool execution_context::stopped() const noexcept {
  return as_context(control_)->backend->stopped();
}

memory_resource* execution_context::resource() const noexcept { return control_->resource; }

} // namespace caors::tasks
