// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/thread_pool.h"

#include "caors/config.h"
#include "executor_control_block.h"

#include <asio/post.hpp>
#include <asio/thread_pool.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>

namespace caors::tasks {

namespace {

struct pool_control final : detail::executor_control_block {
  pool_control(memory_resource* owner, std::size_t count) : thread_count(count) {
    resource = owner;
    post = &post_task;
    valid = &is_valid;
    destroy = &destroy_control;
    backend = caors::make<asio::thread_pool>(resource, count);
  }

  ~pool_control() noexcept { shutdown(shutdown_mode::drain); }

  struct scheduled_task final {
    scheduled_task(pool_control* owner, task next_task)
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
#if CAORS_HAS_EXCEPTIONS
      try {
        (void)next.run();
      } catch (...) {
      }
#else
      (void)next.run();
#endif
    }

    pool_control* control = nullptr;
    task next;
    bool started = false;
  };

  static error post_task(detail::executor_control_block* base, task next) {
    auto* control = static_cast<pool_control*>(base);
    std::lock_guard lock{control->mutex};
    if (control->backend == nullptr || control->is_stopping) {
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
    const auto* control = static_cast<const pool_control*>(base);
    std::lock_guard lock{control->mutex};
    return control->backend != nullptr && !control->is_stopping;
  }

  static void destroy_control(detail::executor_control_block* base) noexcept {
    auto* control = static_cast<pool_control*>(base);
    memory_resource* owner = control->resource;
    caors::destroy(control, owner);
  }

  void shutdown(shutdown_mode mode) noexcept {
    asio::thread_pool* current = nullptr;
    {
      std::lock_guard lock{mutex};
      if (backend == nullptr) {
        return;
      }
      is_stopping = true;
      current = std::exchange(backend, nullptr);
    }

    if (mode == shutdown_mode::cancel) {
      current->stop();
    }
    current->join();
    caors::destroy(current, resource);
  }

  mutable std::mutex mutex;
  asio::thread_pool* backend = nullptr;
  std::atomic_size_t pending{0};
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
  return as_pool(control_)->pending.load(std::memory_order_relaxed);
}

bool thread_pool::stopping() const noexcept {
  const auto* control = static_cast<const pool_control*>(control_);
  std::lock_guard lock{control->mutex};
  return control->is_stopping;
}

void thread_pool::shutdown(shutdown_mode mode) noexcept { as_pool(control_)->shutdown(mode); }

} // namespace caors::tasks
