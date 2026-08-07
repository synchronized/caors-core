// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/config.h"
#include "caors/resources/async.h"

#include <atomic>
#include <coroutine>
#include <memory>
#include <utility>

namespace caors::resources {

namespace detail {

enum class coroutine_wait_phase : unsigned char { initiating, suspended, completed, abandoned };

struct coroutine_wait_state final {
  std::atomic<coroutine_wait_phase> phase{coroutine_wait_phase::initiating};
  std::coroutine_handle<> continuation{};
  caors::error registration_error{};
};

inline void complete_coroutine_wait(const std::shared_ptr<void>& erased) noexcept {
  const auto state = std::static_pointer_cast<coroutine_wait_state>(erased);
  const coroutine_wait_phase previous =
      state->phase.exchange(coroutine_wait_phase::completed, std::memory_order_acq_rel);
  if (previous == coroutine_wait_phase::suspended) {
    state->continuation.resume();
  }
}

inline void abandon_coroutine_wait(const std::shared_ptr<coroutine_wait_state>& state) noexcept {
  if (state == nullptr) {
    return;
  }

  coroutine_wait_phase current = state->phase.load(std::memory_order_acquire);
  while (current != coroutine_wait_phase::completed && current != coroutine_wait_phase::abandoned &&
         !state->phase.compare_exchange_weak(current, coroutine_wait_phase::abandoned,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
  }
}

} // namespace detail

/** @brief 非阻塞等待单个资源加载请求进入终态。 */
template <typename T> class load_request_awaiter final {
public:
  explicit load_request_awaiter(load_request<T> operation) : operation_(std::move(operation)) {}

  load_request_awaiter(const load_request_awaiter&) = delete;
  load_request_awaiter& operator=(const load_request_awaiter&) = delete;
  load_request_awaiter(load_request_awaiter&& other) noexcept
      : operation_(std::move(other.operation_)), state_(std::move(other.state_)),
        registration_error_(other.registration_error_) {}
  load_request_awaiter& operator=(load_request_awaiter&&) = delete;

  ~load_request_awaiter() noexcept { detail::abandon_coroutine_wait(state_); }

  [[nodiscard]] bool await_ready() const { return operation_.done(); }

  [[nodiscard]] bool await_suspend(std::coroutine_handle<> continuation) {
    bool registered = false;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      state_ = std::allocate_shared<detail::coroutine_wait_state>(
          allocator<detail::coroutine_wait_state>{operation_.resource()});
      state_->continuation = continuation;
      registered = detail::load_request_access<T>::add_waiter(
          detail::load_request_access<T>::state(operation_),
          detail::request_waiter{state_, &detail::complete_coroutine_wait});
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      if (state_ != nullptr) {
        state_->registration_error = {error_domain::core, errc::out_of_memory};
        state_->phase.store(detail::coroutine_wait_phase::completed, std::memory_order_release);
      } else {
        registration_error_ = {error_domain::core, errc::out_of_memory};
      }
      return false;
    }
#endif
    if (!registered) {
      state_->phase.store(detail::coroutine_wait_phase::completed, std::memory_order_release);
      return false;
    }

    const detail::coroutine_wait_phase previous =
        state_->phase.exchange(detail::coroutine_wait_phase::suspended, std::memory_order_acq_rel);
    return previous != detail::coroutine_wait_phase::completed;
  }

  [[nodiscard]] handle_result<T> await_resume() const {
    if (registration_error_) {
      return {registration_error_, {}};
    }
    if (state_ != nullptr && state_->registration_error) {
      return {state_->registration_error, {}};
    }
    return operation_.result();
  }

private:
  load_request<T> operation_;
  std::shared_ptr<detail::coroutine_wait_state> state_;
  caors::error registration_error_{};
};

/** @brief 创建等待单个资源加载请求的 coroutine awaiter。 */
template <typename T> [[nodiscard]] load_request_awaiter<T> async_wait(load_request<T> operation) {
  return load_request_awaiter<T>{std::move(operation)};
}

/** @brief 非阻塞等待聚合预加载请求进入终态。 */
class preload_request_awaiter final {
public:
  explicit preload_request_awaiter(preload_request operation) : operation_(std::move(operation)) {}

  preload_request_awaiter(const preload_request_awaiter&) = delete;
  preload_request_awaiter& operator=(const preload_request_awaiter&) = delete;
  preload_request_awaiter(preload_request_awaiter&& other) noexcept
      : operation_(std::move(other.operation_)), state_(std::move(other.state_)),
        registration_error_(other.registration_error_) {}
  preload_request_awaiter& operator=(preload_request_awaiter&&) = delete;

  ~preload_request_awaiter() noexcept { detail::abandon_coroutine_wait(state_); }

  [[nodiscard]] bool await_ready() const { return operation_.done(); }

  [[nodiscard]] bool await_suspend(std::coroutine_handle<> continuation) {
    bool registered = false;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      state_ = std::allocate_shared<detail::coroutine_wait_state>(
          allocator<detail::coroutine_wait_state>{operation_.resource()});
      state_->continuation = continuation;
      registered = detail::preload_request_access::add_waiter(
          detail::preload_request_access::state(operation_),
          detail::request_waiter{state_, &detail::complete_coroutine_wait});
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      if (state_ != nullptr) {
        state_->registration_error = {error_domain::core, errc::out_of_memory};
        state_->phase.store(detail::coroutine_wait_phase::completed, std::memory_order_release);
      } else {
        registration_error_ = {error_domain::core, errc::out_of_memory};
      }
      return false;
    }
#endif
    if (!registered) {
      state_->phase.store(detail::coroutine_wait_phase::completed, std::memory_order_release);
      return false;
    }

    const detail::coroutine_wait_phase previous =
        state_->phase.exchange(detail::coroutine_wait_phase::suspended, std::memory_order_acq_rel);
    return previous != detail::coroutine_wait_phase::completed;
  }

  [[nodiscard]] preload_result await_resume() const {
    if (registration_error_ || (state_ != nullptr && state_->registration_error)) {
      preload_result result;
      result.status = registration_error_ ? registration_error_ : state_->registration_error;
      return result;
    }
    return operation_.result();
  }

private:
  preload_request operation_;
  std::shared_ptr<detail::coroutine_wait_state> state_;
  caors::error registration_error_{};
};

/** @brief 创建等待聚合预加载请求的 coroutine awaiter。 */
[[nodiscard]] inline preload_request_awaiter async_wait(preload_request operation) {
  return preload_request_awaiter{std::move(operation)};
}

} // namespace caors::resources
