// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_ASIO_H_
#define CAORS_TASKS_ASIO_H_

#include "caors/tasks/executor.h"

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

} // namespace caors::tasks

#endif
