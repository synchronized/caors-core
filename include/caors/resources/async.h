// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_ASYNC_H_
#define CAORS_RESOURCES_ASYNC_H_

#include "caors/config.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/resources/io.h"
#include "caors/tasks.h"

#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace caors::resources {

/** @brief 异步资源请求状态。 */
enum class request_status {
  /** @brief 请求已创建，但后台任务尚未开始。 */
  queued,
  /** @brief 后台任务正在加载或等待主线程 commit。 */
  loading,
  /** @brief 请求已完成，结果 handle 可用。 */
  loaded,
  /** @brief 请求失败，`error()` 保存失败原因。 */
  failed,
  /** @brief 请求被取消。已经开始的后台任务不会被强制中断，但不会再提交 cache。 */
  cancelled,
};

template <typename T> class load_request;

namespace detail {

template <typename T> class load_request_access;

template <typename T>
[[nodiscard]] error commit_loaded_resource(cache<T>& target, resource_guid guid,
                                           memory::unique_ptr<T> loaded, bool replace,
                                           handle<T>& committed) {
  if (loaded == nullptr) {
    return {error_domain::resources, errc::backend_failure};
  }

  handle<T> loaded_handle = adopt_handle(std::move(loaded), target.resource());
  if (!loaded_handle) {
    return {error_domain::resources, errc::out_of_memory};
  }

  if (!loaded_handle->has_guid()) {
    loaded_handle->set_guid(guid);
  }
  if (loaded_handle->guid() != guid) {
    return {error_domain::resources, errc::inconsistent_type};
  }

  const bool committed_to_cache =
      replace ? target.replace(loaded_handle) : target.insert(loaded_handle);
  if (!committed_to_cache) {
    return {error_domain::resources, replace ? errc::backend_failure : errc::already_exists};
  }

  committed = std::move(loaded_handle);
  return {};
}

} // namespace detail

/**
 * @brief 异步资源加载请求视图。
 *
 * `load_request<T>` 是可拷贝的轻量句柄，多个副本共享同一个线程安全状态。后台任务会把加载结果提交到
 * `tasks::completion_queue`，调用方在主线程 drain 后再通过 `status()`、`error()` 和 `get()`
 * 查询结果。
 */
template <typename T> class load_request {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  load_request() noexcept = default;

  /** @brief 创建一个立即失败的请求。 */
  [[nodiscard]] static load_request failed(error status, memory_resource* resource = nullptr) {
    load_request request = detail::load_request_access<T>::make(resource);
    detail::load_request_access<T>::fail(request, status);
    return request;
  }

  /** @brief 创建一个立即完成的请求。 */
  [[nodiscard]] static load_request completed(handle<T> value,
                                              memory_resource* resource = nullptr) {
    load_request request = detail::load_request_access<T>::make(resource);
    detail::load_request_access<T>::complete(request, std::move(value));
    return request;
  }

  /** @brief 判断当前句柄是否引用一个请求状态。 */
  [[nodiscard]] bool valid() const noexcept { return state_ != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  /** @brief 查询请求状态。空请求返回 `cancelled`。 */
  [[nodiscard]] request_status status() const {
    if (state_ == nullptr) {
      return request_status::cancelled;
    }

    std::lock_guard lock{state_->mutex};
    return state_->status;
  }

  /** @brief 查询失败原因。非失败请求返回成功状态；空请求返回 `invalid_argument`。 */
  [[nodiscard]] caors::error error() const {
    if (state_ == nullptr) {
      return {error_domain::resources, errc::invalid_argument};
    }

    std::lock_guard lock{state_->mutex};
    return state_->status_error;
  }

  /** @brief 查询请求是否已经结束。 */
  [[nodiscard]] bool done() const {
    const request_status current = status();
    return current == request_status::loaded || current == request_status::failed ||
           current == request_status::cancelled;
  }

  /** @brief 查询请求是否加载成功。 */
  [[nodiscard]] bool loaded() const { return status() == request_status::loaded; }

  /** @brief 查询请求是否失败。 */
  [[nodiscard]] bool failed() const { return status() == request_status::failed; }

  /** @brief 查询请求是否被取消。 */
  [[nodiscard]] bool cancelled() const { return status() == request_status::cancelled; }

  /**
   * @brief 请求取消。
   *
   * 只有 queued/loading 状态可以取消。取消不会强制中断已经开始执行的 loader，但会阻止后续
   * completion queue commit 写入 cache。请求已经 loaded、failed 或 cancelled 时返回 false。
   */
  [[nodiscard]] bool cancel() const {
    if (state_ == nullptr) {
      return false;
    }

    return detail::load_request_access<T>::cancel(*this);
  }

  /** @brief 返回加载完成后的强 handle。未完成或失败时返回空 handle。 */
  [[nodiscard]] handle<T> get() const {
    if (state_ == nullptr) {
      return {};
    }

    std::lock_guard lock{state_->mutex};
    return state_->value;
  }

  /**
   * @brief 返回当前请求结果。
   *
   * loaded 时返回成功和结果 handle；queued/loading 时返回 `errc::pending`；cancelled 时返回
   * `errc::cancelled`；failed 时返回原始错误。
   */
  [[nodiscard]] handle_result<T> result() const {
    if (state_ == nullptr) {
      return {{error_domain::resources, errc::invalid_argument}, {}};
    }

    std::lock_guard lock{state_->mutex};
    switch (state_->status) {
    case request_status::loaded:
      return {{}, state_->value};
    case request_status::failed:
      return {state_->status_error ? state_->status_error
                                   : caors::error{error_domain::resources, errc::backend_failure},
              {}};
    case request_status::cancelled:
      return {{error_domain::resources, errc::cancelled}, {}};
    case request_status::queued:
    case request_status::loading:
      return {{error_domain::resources, errc::pending}, {}};
    }

    return {{error_domain::resources, errc::backend_failure}, {}};
  }

private:
  friend class detail::load_request_access<T>;

  struct state_type {
    explicit state_type(memory_resource* resource) noexcept : resource(resource) {}

    memory_resource* resource = nullptr;
    mutable std::mutex mutex{};
    request_status status = request_status::queued;
    caors::error status_error{};
    handle<T> value{};
  };

  explicit load_request(std::shared_ptr<state_type> state) noexcept : state_(std::move(state)) {}

  std::shared_ptr<state_type> state_{};
};

namespace detail {

template <typename T> class load_request_access {
public:
  using request_type = load_request<T>;
  using state_type = typename request_type::state_type;

  [[nodiscard]] static request_type make(memory_resource* resource = nullptr) {
    memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
    return request_type{
        std::allocate_shared<state_type>(allocator<state_type>{resolved}, resolved)};
  }

  [[nodiscard]] static std::shared_ptr<state_type> state(const request_type& request) noexcept {
    return request.state_;
  }

  [[nodiscard]] static bool loading(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled) {
      return false;
    }
    if (state->status == request_status::loaded || state->status == request_status::failed) {
      return false;
    }

    state->status = request_status::loading;
    state->status_error = {};
    state->value = {};
    return true;
  }

  static void complete(const request_type& request, handle<T> value) {
    (void)complete(request.state_, std::move(value));
  }

  [[nodiscard]] static bool complete(const std::shared_ptr<state_type>& state, handle<T> value) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return false;
    }

    state->status = request_status::loaded;
    state->status_error = {};
    state->value = std::move(value);
    return true;
  }

  static void fail(const request_type& request, caors::error status) {
    fail(request.state_, status);
  }

  static void fail(const std::shared_ptr<state_type>& state, caors::error status) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::loaded) {
      return;
    }

    state->status = request_status::failed;
    state->status_error =
        status ? status : caors::error{error_domain::resources, errc::backend_failure};
    state->value = {};
  }

  [[nodiscard]] static bool cancel(const request_type& request) { return cancel(request.state_); }

  [[nodiscard]] static bool cancel(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status != request_status::queued && state->status != request_status::loading) {
      return false;
    }

    state->status = request_status::cancelled;
    state->status_error = {error_domain::resources, errc::cancelled};
    state->value = {};
    return true;
  }

  [[nodiscard]] static bool is_cancelled(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return true;
    }

    std::lock_guard lock{state->mutex};
    return state->status == request_status::cancelled;
  }
};

template <typename T, typename StatePtr>
void load_resource_in_background(const StatePtr& state, cache<T>& target, loader<T>& source,
                                 resource_guid guid, tasks::completion_queue& completions,
                                 bool replace) {
  if (!load_request_access<T>::loading(state)) {
    return;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    load_result<T> loaded = source.load(guid, target.resource());
    if (loaded.failed()) {
      load_request_access<T>::fail(state, loaded.status);
      return;
    }

    if (load_request_access<T>::is_cancelled(state)) {
      return;
    }

    error push_status = completions.push(tasks::task{
        target.resource(),
        [state, &target, guid, replace, loaded_resource = std::move(loaded.value)]() mutable {
          if (load_request_access<T>::is_cancelled(state)) {
            return error{};
          }

          handle<T> committed;
          const error status =
              commit_loaded_resource(target, guid, std::move(loaded_resource), replace, committed);
          if (status) {
            load_request_access<T>::fail(state, status);
            return status;
          }

          load_request_access<T>::complete(state, std::move(committed));
          return error{};
        }});
    if (push_status) {
      load_request_access<T>::fail(state, push_status);
    }
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    load_request_access<T>::fail(state, {error_domain::resources, errc::backend_failure});
  }
#endif
}

} // namespace detail

/**
 * @brief 异步加载资源并在 completion queue 中提交到 cache。
 *
 * 已缓存资源会立即返回 loaded 请求。未缓存资源会在 executor 中调用 loader，随后把 cache
 * 插入动作投递到 completion queue；调用方应在主线程 drain completion queue。
 */
template <typename T>
[[nodiscard]] load_request<T> request_load_into(cache<T>& target, loader<T>& source,
                                                resource_guid guid, tasks::executor& executor,
                                                tasks::completion_queue& completions) {
  if (!guid) {
    return load_request<T>::failed({error_domain::resources, errc::invalid_argument},
                                   target.resource());
  }

  if (handle<T> existing = target.find_handle(guid)) {
    return load_request<T>::completed(std::move(existing), target.resource());
  }

  load_request<T> request = detail::load_request_access<T>::make(target.resource());
  auto state = detail::load_request_access<T>::state(request);

  error submit_status = executor.submit(tasks::task{
      target.resource(), [state, &target, &source, guid, &completions]() mutable {
        detail::load_resource_in_background(state, target, source, guid, completions, false);
      }});
  if (submit_status) {
    detail::load_request_access<T>::fail(state, submit_status);
  }

  return request;
}

/**
 * @brief 异步重载资源并在 completion queue 中替换 cache 绑定。
 *
 * cache 未包含该 GUID 时立即失败。加载或 commit 失败时旧资源保持不变。
 */
template <typename T>
[[nodiscard]] load_request<T> request_reload_into(cache<T>& target, loader<T>& source,
                                                  resource_guid guid, tasks::executor& executor,
                                                  tasks::completion_queue& completions) {
  if (!guid) {
    return load_request<T>::failed({error_domain::resources, errc::invalid_argument},
                                   target.resource());
  }

  if (!target.contains(guid)) {
    return load_request<T>::failed({error_domain::resources, errc::not_found}, target.resource());
  }

  load_request<T> request = detail::load_request_access<T>::make(target.resource());
  auto state = detail::load_request_access<T>::state(request);

  error submit_status = executor.submit(tasks::task{
      target.resource(), [state, &target, &source, guid, &completions]() mutable {
        detail::load_resource_in_background(state, target, source, guid, completions, true);
      }});
  if (submit_status) {
    detail::load_request_access<T>::fail(state, submit_status);
  }

  return request;
}

} // namespace caors::resources

namespace caors {

using resource_request_status = resources::request_status;
template <typename T> using resource_load_request = resources::load_request<T>;
using resources::request_load_into;
using resources::request_reload_into;

} // namespace caors

#endif
