// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_ASIO_H_
#define CAORS_TASKS_ASIO_H_

#include "caors/tasks/request.h"

#include <atomic>
#include <coroutine>
#include <memory>
#include <utility>

namespace caors::tasks {

namespace detail {

enum class schedule_phase { initiating, suspended, completed, abandoned };

struct schedule_state final {
  std::atomic<schedule_phase> phase{schedule_phase::initiating};
  std::coroutine_handle<> continuation{};
  caors::error status{};
};

inline void complete_schedule(const std::shared_ptr<schedule_state>& state,
                              caors::error status) noexcept {
  state->status = status;
  const schedule_phase previous =
      state->phase.exchange(schedule_phase::completed, std::memory_order_acq_rel);
  if (previous == schedule_phase::suspended) {
    state->continuation.resume();
  }
}

struct schedule_handler final {
  explicit schedule_handler(std::shared_ptr<schedule_state> owner) : state(std::move(owner)) {}

  schedule_handler(const schedule_handler&) = delete;
  schedule_handler& operator=(const schedule_handler&) = delete;

  schedule_handler(schedule_handler&& other) noexcept
      : state(std::move(other.state)), active(std::exchange(other.active, false)) {}

  ~schedule_handler() noexcept {
    if (active) {
      complete_schedule(state, {error_domain::core, errc::frozen});
    }
  }

  void operator()() noexcept {
    active = false;
    complete_schedule(state, {});
  }

  std::shared_ptr<schedule_state> state;
  bool active = true;
};

} // namespace detail

/** @brief 将当前 coroutine 切换到指定 executor，并以 `caors::error` 报告调度结果。 */
class schedule_awaiter final {
public:
  explicit schedule_awaiter(executor target)
      : target_(std::move(target)), state_(std::make_shared<detail::schedule_state>()) {}

  schedule_awaiter(const schedule_awaiter&) = delete;
  schedule_awaiter& operator=(const schedule_awaiter&) = delete;
  schedule_awaiter(schedule_awaiter&& other) noexcept
      : target_(std::move(other.target_)), state_(std::move(other.state_)) {}
  schedule_awaiter& operator=(schedule_awaiter&&) = delete;

  ~schedule_awaiter() noexcept {
    if (state_ == nullptr) {
      return;
    }
    detail::schedule_phase current = state_->phase.load(std::memory_order_acquire);
    while (current != detail::schedule_phase::completed &&
           current != detail::schedule_phase::abandoned &&
           !state_->phase.compare_exchange_weak(current, detail::schedule_phase::abandoned,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
    }
  }

  [[nodiscard]] bool await_ready() noexcept {
    if (!target_.valid()) {
      state_->status = {error_domain::core, errc::frozen};
      state_->phase.store(detail::schedule_phase::completed, std::memory_order_relaxed);
      return true;
    }
    return false;
  }

  [[nodiscard]] bool await_suspend(std::coroutine_handle<> continuation) {
    std::shared_ptr<detail::schedule_state> state = state_;
    state->continuation = continuation;
    const caors::error status = target_.post(task{detail::schedule_handler{state}});
    if (status) {
      state->status = status;
      state->phase.store(detail::schedule_phase::completed, std::memory_order_release);
      return false;
    }

    const detail::schedule_phase previous =
        state->phase.exchange(detail::schedule_phase::suspended, std::memory_order_acq_rel);
    return previous != detail::schedule_phase::completed;
  }

  [[nodiscard]] caors::error await_resume() const noexcept { return state_->status; }

private:
  executor target_;
  std::shared_ptr<detail::schedule_state> state_;
};

/** @brief 创建一个只负责切换执行目标的 coroutine awaiter。 */
[[nodiscard]] inline schedule_awaiter schedule(executor target) {
  return schedule_awaiter{std::move(target)};
}

class async_operation;

/** @brief 在指定 executor 启动 coroutine，并返回共享请求状态。 */
[[nodiscard]] request spawn(executor target, async_operation operation,
                            memory_resource* resource = nullptr);

/**
 * @brief `spawn()` 使用的最小 error-only coroutine 返回类型。
 *
 * coroutine 初始处于挂起状态，frame 在成功交给 `spawn()` 后由运行时持有，最终完成时自动销毁。
 */
class async_operation final {
public:
  struct promise_type {
    [[nodiscard]] async_operation get_return_object() noexcept {
      return async_operation{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }
    [[nodiscard]] std::suspend_never final_suspend() const noexcept { return {}; }

    void return_value(caors::error value) noexcept {
      if (value) {
        detail::request_access::fail(state, value);
      } else {
        (void)detail::request_access::complete(state);
      }
    }

    void unhandled_exception() noexcept {
      detail::request_access::fail(state, {error_domain::core, errc::backend_failure});
    }

    std::shared_ptr<detail::request_state> state;
  };

  async_operation() noexcept = default;
  async_operation(const async_operation&) = delete;
  async_operation& operator=(const async_operation&) = delete;
  async_operation(async_operation&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  async_operation& operator=(async_operation&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }
  ~async_operation() noexcept { reset(); }

  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr; }

private:
  using handle_type = std::coroutine_handle<promise_type>;

  explicit async_operation(handle_type handle) noexcept : handle_(handle) {}

  void reset() noexcept {
    if (handle_ != nullptr) {
      handle_.destroy();
      handle_ = nullptr;
    }
  }

  [[nodiscard]] handle_type release(std::shared_ptr<detail::request_state> state) noexcept {
    handle_type released = std::exchange(handle_, {});
    released.promise().state = std::move(state);
    return released;
  }

  handle_type handle_{};

  friend request spawn(executor, async_operation, memory_resource*);
};

namespace detail {

struct spawn_handler final {
  using handle_type = std::coroutine_handle<async_operation::promise_type>;

  spawn_handler(handle_type operation, std::shared_ptr<request_state> request_state)
      : handle(operation), state(std::move(request_state)) {}

  spawn_handler(const spawn_handler&) = delete;
  spawn_handler& operator=(const spawn_handler&) = delete;
  spawn_handler(spawn_handler&& other) noexcept
      : handle(std::exchange(other.handle, {})), state(std::move(other.state)) {}

  ~spawn_handler() noexcept {
    if (handle != nullptr) {
      request_access::fail(state, {error_domain::core, errc::frozen});
      handle.destroy();
    }
  }

  void operator()() noexcept {
    handle_type operation = std::exchange(handle, {});
    if (!request_access::start(state)) {
      operation.destroy();
      return;
    }
    operation.resume();
  }

  handle_type handle{};
  std::shared_ptr<request_state> state;
};

} // namespace detail

inline request spawn(executor target, async_operation operation, memory_resource* resource) {
  if (!operation) {
    return request::make_failed({error_domain::core, errc::invalid_argument}, resource);
  }

  request current = request::make(resource);
  std::shared_ptr<detail::request_state> state = detail::request_access::state(current);
  auto handle = operation.release(state);
  task start{resource, detail::spawn_handler{handle, state}};
  const caors::error status = target.post(std::move(start));
  if (status) {
    detail::request_access::fail(state, status);
  }
  return current;
}

} // namespace caors::tasks

#endif
