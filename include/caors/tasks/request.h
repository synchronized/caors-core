// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_REQUEST_H_
#define CAORS_TASKS_REQUEST_H_

#include "caors/config.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/tasks/executor.h"

#include <memory>
#include <mutex>
#include <utility>

namespace caors::tasks {

/** @brief 任务请求状态。 */
enum class request_status {
  /** @brief 请求已创建，任务尚未开始执行。 */
  queued,
  /** @brief 任务正在执行。 */
  running,
  /** @brief 任务已成功完成。 */
  completed,
  /** @brief 任务执行失败，`error()` 保存失败原因。 */
  failed,
  /** @brief 请求已取消。已经开始的任务不会被强制中断。 */
  cancelled,
};

namespace detail {

struct request_state {
  explicit request_state(memory_resource* memory) noexcept
      : resource(memory != nullptr ? memory : get_default_memory_resource()) {}

  memory_resource* resource = nullptr;
  mutable std::mutex mutex{};
  request_status status = request_status::queued;
  caors::error status_error{};
};

class request_access;

} // namespace detail

/**
 * @brief 任务提交后的共享状态句柄。
 *
 * `request` 不保存任务返回值，只保存状态和错误。多个 `request` 副本共享同一状态，可跨线程查询。
 * 取消请求不会强制中断已经开始执行的任务，只会阻止后续完成状态覆盖 cancelled。
 */
class request final {
public:
  request() noexcept = default;

  /** @brief 创建一个 queued 请求状态。 */
  [[nodiscard]] static request make(memory_resource* resource = nullptr);

  /** @brief 创建一个立即成功的请求。 */
  [[nodiscard]] static request make_completed(memory_resource* resource = nullptr);

  /** @brief 创建一个立即失败的请求。 */
  [[nodiscard]] static request make_failed(caors::error status,
                                           memory_resource* resource = nullptr);

  /** @brief 判断当前句柄是否引用一个请求状态。 */
  [[nodiscard]] bool valid() const noexcept { return state_ != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  /** @brief 查询请求状态。空请求返回 cancelled。 */
  [[nodiscard]] request_status status() const;

  /** @brief 查询失败原因。非 failed 请求返回成功状态；空请求返回 invalid_argument。 */
  [[nodiscard]] caors::error error() const;

  /** @brief 按当前状态返回结果错误。queued/running 返回 pending。 */
  [[nodiscard]] caors::error result() const;

  /** @brief 查询请求是否已经结束。 */
  [[nodiscard]] bool done() const;

  /** @brief 查询任务是否成功完成。 */
  [[nodiscard]] bool completed() const { return status() == request_status::completed; }

  /** @brief 查询任务是否失败。 */
  [[nodiscard]] bool failed() const { return status() == request_status::failed; }

  /** @brief 查询请求是否取消。 */
  [[nodiscard]] bool cancelled() const { return status() == request_status::cancelled; }

  /** @brief 取消 queued/running 请求。已结束请求返回 false。 */
  [[nodiscard]] bool cancel() const;

private:
  friend class detail::request_access;

  explicit request(std::shared_ptr<detail::request_state> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::request_state> state_{};
};

namespace detail {

class request_access {
public:
  using state_type = request_state;

  [[nodiscard]] static request make(memory_resource* resource = nullptr) {
    memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
    return request{std::allocate_shared<state_type>(allocator<state_type>{resolved}, resolved)};
  }

  [[nodiscard]] static std::shared_ptr<state_type> state(const request& current) noexcept {
    return current.state_;
  }

  [[nodiscard]] static bool start(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status != request_status::queued) {
      return false;
    }

    state->status = request_status::running;
    state->status_error = {};
    return true;
  }

  [[nodiscard]] static bool complete(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::completed ||
        state->status == request_status::failed) {
      return false;
    }

    state->status = request_status::completed;
    state->status_error = {};
    return true;
  }

  static void fail(const std::shared_ptr<state_type>& state, caors::error status) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::completed) {
      return;
    }

    state->status = request_status::failed;
    state->status_error = status ? status : caors::error{error_domain::core, errc::backend_failure};
  }

  [[nodiscard]] static bool cancel(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status != request_status::queued && state->status != request_status::running) {
      return false;
    }

    state->status = request_status::cancelled;
    state->status_error = {error_domain::core, errc::cancelled};
    return true;
  }
};

} // namespace detail

inline request request::make(memory_resource* resource) {
  return detail::request_access::make(resource);
}

inline request request::make_completed(memory_resource* resource) {
  request current = detail::request_access::make(resource);
  (void)detail::request_access::complete(detail::request_access::state(current));
  return current;
}

inline request request::make_failed(caors::error status, memory_resource* resource) {
  request current = detail::request_access::make(resource);
  detail::request_access::fail(detail::request_access::state(current), status);
  return current;
}

inline request_status request::status() const {
  if (state_ == nullptr) {
    return request_status::cancelled;
  }

  std::lock_guard lock{state_->mutex};
  return state_->status;
}

inline caors::error request::error() const {
  if (state_ == nullptr) {
    return {error_domain::core, errc::invalid_argument};
  }

  std::lock_guard lock{state_->mutex};
  return state_->status_error;
}

inline caors::error request::result() const {
  if (state_ == nullptr) {
    return {error_domain::core, errc::invalid_argument};
  }

  std::lock_guard lock{state_->mutex};
  switch (state_->status) {
  case request_status::completed:
    return {};
  case request_status::failed:
    return state_->status_error ? state_->status_error
                                : caors::error{error_domain::core, errc::backend_failure};
  case request_status::cancelled:
    return {error_domain::core, errc::cancelled};
  case request_status::queued:
  case request_status::running:
    return {error_domain::core, errc::pending};
  }

  return {error_domain::core, errc::backend_failure};
}

inline bool request::done() const {
  const request_status current = status();
  return current == request_status::completed || current == request_status::failed ||
         current == request_status::cancelled;
}

inline bool request::cancel() const { return detail::request_access::cancel(state_); }

/**
 * @brief 提交一个带状态的任务请求。
 *
 * 成功提交后返回 queued/running/completed 之一，具体取决于 executor 是否同步执行。提交失败时返回
 * failed request，错误来自 executor。任务返回错误时请求进入 failed；返回成功时进入 completed。
 */
[[nodiscard]] inline request submit_request(executor& target, task next,
                                            memory_resource* resource = nullptr) {
  if (!next) {
    return request::make_failed({error_domain::core, errc::invalid_argument}, resource);
  }

  request current = request::make(resource != nullptr ? resource : next.resource());
  std::shared_ptr<detail::request_state> state = detail::request_access::state(current);
  task wrapped{resource != nullptr ? resource : next.resource(),
               [state, next = std::move(next)]() mutable -> error {
                 if (!detail::request_access::start(state)) {
                   return {};
                 }

#if CAORS_HAS_EXCEPTIONS
                 try {
#endif
                   const error status = next.run();
                   if (status) {
                     detail::request_access::fail(state, status);
                     return status;
                   }

                   (void)detail::request_access::complete(state);
                   return {};
#if CAORS_HAS_EXCEPTIONS
                 } catch (...) {
                   const error status{error_domain::core, errc::backend_failure};
                   detail::request_access::fail(state, status);
                   return status;
                 }
#endif
               }};

  const error submit_status = target.submit(std::move(wrapped));
  if (submit_status) {
    detail::request_access::fail(state, submit_status);
  }

  return current;
}

} // namespace caors::tasks

#endif
