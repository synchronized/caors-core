// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_ASYNC_REQUEST_H_
#define CAORS_IO_ASYNC_REQUEST_H_

#include "caors/io/types.h"
#include "caors/tasks/detail/abandon_guard.h"
#include "caors/tasks/executor.h"

#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>

#include <cstdint>

namespace caors::io {
/** @brief IO 失败分类；后端原始诊断独立保留，不要求分配错误字符串。 */
enum class io_errc {
  success,
  invalid_argument,
  not_found,
  unsupported,
  cancelled,
  closed,
  out_of_range,
  limit_exceeded,
  short_read,
  permission_denied,
  backend_failure,
  completion_rejected,
  out_of_memory,
  pending
};
/** @brief 不借用外部字符串的 IO 诊断；backend_id 由宿主分配进程内稳定标识。 */
struct io_error {
  io_errc value = io_errc::success;
  std::uint64_t backend_id = 0;
  std::int64_t native_code = 0;
  [[nodiscard]] explicit operator bool() const noexcept { return value != io_errc::success; }
};
/** @brief 带拥有式结果与 IO 诊断的返回值。 */
template <typename T> using io_result = result<T, io_error>;
/** @brief 构造 IO 失败结果。 */
template <typename T> io_result<T> io_failure(io_error error) { return {error, T{}}; }

template <typename T> class io_completion;
template <typename T> class io_request;
namespace detail {
template <typename T> struct io_state : std::enable_shared_from_this<io_state<T>> {
  // 此字段最先构造、最后释放，覆盖回调、结果和 executor 的析构代码。
  std::shared_ptr<void> lifetime;
  std::function<void(io_result<T>&)> prepare;
  std::function<void(const io_result<T>&)> callback;
  tasks::executor executor;
  std::stop_source stop;
  mutable std::mutex mutex;
  bool accepted = false;
  bool refused = false;
  bool submitted = false;
  bool scheduled = false;
  bool terminal = false;
  std::optional<io_result<T>> value;

  bool submit(io_result<T> next) noexcept {
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      if (prepare)
        prepare(next);
#if CAORS_HAS_EXCEPTIONS
    } catch (const std::bad_alloc&) {
      next = io_failure<T>({io_errc::out_of_memory});
    } catch (...) {
      next = io_failure<T>({io_errc::backend_failure});
    }
#endif
    {
      std::lock_guard lock{mutex};
      if (submitted || refused)
        return false;
      submitted = true;
      value.emplace(std::move(next));
    }
    dispatch();
    return true;
  }
  void fail_delivery(io_errc reason, std::int64_t native_code = 0) noexcept {
    std::optional<io_result<T>> discarded;
    std::function<void(const io_result<T>&)> abandoned;
    {
      std::lock_guard lock{mutex};
      if (terminal || refused)
        return;
      discarded = std::move(value);
      value.emplace(io_failure<T>({reason, 0, native_code}));
      terminal = true;
      // std::function 的移动不保证清空源；交换到空槽以解除回调拥有的循环引用。
      abandoned.swap(callback);
    }
  }
  void deliver() {
    std::optional<io_result<T>> discarded;
    std::function<void(const io_result<T>&)> notify;
    {
      std::lock_guard lock{mutex};
      if (terminal || refused)
        return;
      if (stop.stop_requested()) {
        discarded = std::move(value);
        value.emplace(io_failure<T>({io_errc::cancelled}));
      }
      terminal = true;
      notify.swap(callback);
    }
    if (notify)
      notify(*value);
  }
  void dispatch() noexcept {
    {
      std::lock_guard lock{mutex};
      if (!accepted || refused || !submitted || scheduled)
        return;
      scheduled = true;
    }
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      auto self = this->shared_from_this();
      auto abandonment = std::allocate_shared<tasks::detail::abandonment>(
          allocator<tasks::detail::abandonment>{executor.resource()},
          tasks::task{executor.resource(),
                      [self]() noexcept { self->fail_delivery(io_errc::completion_rejected); }});
      auto status = executor.post(tasks::task{
          executor.resource(), [self, guard = tasks::detail::abandon_guard{abandonment}]() mutable {
            guard.release();
            self->deliver();
          }});
      if (status)
        fail_delivery(status.value() == errc::out_of_memory ? io_errc::out_of_memory
                                                            : io_errc::completion_rejected,
                      static_cast<std::int64_t>(status.value()));
      else
        abandonment->arm();
#if CAORS_HAS_EXCEPTIONS
    } catch (const std::bad_alloc&) {
      fail_delivery(io_errc::out_of_memory);
    } catch (...) {
      fail_delivery(io_errc::backend_failure);
    }
#endif
  }
};
}

/**
 * @brief move-only 的已接受请求；析构只请求停止，不等待真实 I/O 清理。
 * @note 取消是协作式；底层必须完成或销毁凭证。完成通知可早于发起函数返回。
 */
template <typename T> class io_request {
public:
  /** @brief 无法启动或完成调度不可用时构造可查询失败请求，不调用用户回调。 */
  [[nodiscard]] static io_request failed(io_error status) {
    io_request request;
    request.state_ = std::make_shared<detail::io_state<T>>();
    request.state_->terminal = true;
    request.state_->value.emplace(io_failure<T>(status));
    return request;
  }
  /** @brief 终态结果已经可查询；取消请求不等同于后端完成清理。 */
  [[nodiscard]] bool done() const noexcept { return static_cast<bool>(result()); }
  io_request() = default;
  io_request(const io_request&) = delete;
  io_request& operator=(const io_request&) = delete;
  io_request(io_request&&) noexcept = default;
  io_request& operator=(io_request&& other) noexcept {
    if (this != &other) {
      cancel();
      state_ = std::move(other.state_);
    }
    return *this;
  }
  ~io_request() { cancel(); }
  /** @brief 请求协作停止；不会撤销已经确定的终态。 */
  void cancel() noexcept {
    if (state_)
      (void)state_->stop.request_stop();
  }
  /** @brief 查询完成结果；pending 返回空指针，终态结果保持不可变并拥有寿命。 */
  [[nodiscard]] std::shared_ptr<const io_result<T>> result() const noexcept {
    if (!state_)
      return {};
    std::lock_guard lock{state_->mutex};
    if (!state_->terminal)
      return {};
    return {state_, &*state_->value};
  }
  /** @brief 请求是否有效。 */
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(state_); }

private:
  template <typename U, typename Starter>
  friend io_result<io_request<U>>
  start_io(tasks::executor, std::function<void(const io_result<U>&)>, Starter&&,
           std::shared_ptr<void>, std::function<void(io_result<U>&)>);
  std::shared_ptr<detail::io_state<T>> state_;
};

/** @brief 后端持有的唯一完成凭证；销毁未完成凭证按取消收尾，重复完成返回 false。 */
template <typename T> class io_completion {
public:
  io_completion() = default;
  io_completion(const io_completion&) = delete;
  io_completion& operator=(const io_completion&) = delete;
  io_completion(io_completion&&) noexcept = default;
  io_completion& operator=(io_completion&& other) noexcept {
    if (this != &other) {
      abandon();
      state_ = std::move(other.state_);
    }
    return *this;
  }
  ~io_completion() { abandon(); }
  /** @brief 交付结果；线程安全，允许在后端启动函数返回之前交付。 */
  [[nodiscard]] bool complete(io_result<T> value) const noexcept {
    auto state = state_;
    return state && state->submit(std::move(value));
  }
  /** @brief 观察最后请求句柄的停止要求。 */
  [[nodiscard]] std::stop_token stop_token() const noexcept {
    return state_ ? state_->stop.get_token() : std::stop_token{};
  }

private:
  template <typename U, typename Starter>
  friend io_result<io_request<U>>
  start_io(tasks::executor, std::function<void(const io_result<U>&)>, Starter&&,
           std::shared_ptr<void>, std::function<void(io_result<U>&)>);
  void abandon() noexcept {
    auto state = std::move(state_);
    if (state)
      (void)state->submit(io_failure<T>({io_errc::cancelled}));
  }
  std::shared_ptr<detail::io_state<T>> state_;
};

/**
 * @brief 调用后端启动协议；拒绝直接返回错误，无用户通知；接受后通知只经过 completion。
 * @note starter 返回 io_error，接收 io_completion<T>；拒绝时不得继续操作或保留凭证。
 * 所有 allocator、后端和宿主 loop 须覆盖真实完成及凭证清理，不仅是请求取消时刻。
 */
template <typename T, typename Starter>
[[nodiscard]] io_result<io_request<T>>
start_io(tasks::executor completion, std::function<void(const io_result<T>&)> callback,
         Starter&& starter, std::shared_ptr<void> lifetime = {},
         std::function<void(io_result<T>&)> prepare = {}) {
  if (!completion)
    return io_failure<io_request<T>>({io_errc::completion_rejected});
  auto state = std::make_shared<detail::io_state<T>>();
  state->lifetime = std::move(lifetime);
  state->prepare = std::move(prepare);
  state->executor = std::move(completion);
  state->callback = std::move(callback);
  io_completion<T> receipt;
  receipt.state_ = state;
  io_error status;
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    status = std::forward<Starter>(starter)(std::move(receipt));
#if CAORS_HAS_EXCEPTIONS
  } catch (const std::bad_alloc&) {
    status = {io_errc::out_of_memory};
  } catch (...) {
    status = {io_errc::backend_failure};
  }
#endif
  {
    std::lock_guard lock{state->mutex};
    state->refused = static_cast<bool>(status);
    state->accepted = !status;
  }
  if (status)
    return io_failure<io_request<T>>(status);
  state->dispatch();
  io_request<T> request;
  request.state_ = std::move(state);
  return {{}, std::move(request)};
}
} // namespace caors::io
#endif
