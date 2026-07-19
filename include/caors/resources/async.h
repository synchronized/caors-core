// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_ASYNC_H_
#define CAORS_RESOURCES_ASYNC_H_

#include "caors/config.h"
#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/resources/io.h"
#include "caors/resources/preload.h"
#include "caors/tasks.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>
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

/** @brief request poll/drain 辅助函数的结果。 */
struct request_poll_result {
  /** @brief 本次从 completion queue 执行的完成回调数量。 */
  std::size_t drained = 0;
  /** @brief 目标请求或请求集合是否已经进入终态。 */
  bool done = false;
};

template <typename T> class load_request;
template <typename T> class load_request_table;
class preload_request;

/**
 * @brief drain 一次 completion queue，并返回单个请求是否完成。
 *
 * 该函数不阻塞等待后台线程，只执行调用时已经进入 completion queue 的回调。
 */
template <typename Request>
[[nodiscard]] request_poll_result poll_request(tasks::completion_queue& completions,
                                               const Request& request) {
  request_poll_result result;
  if (!request.done()) {
    result.drained = completions.drain();
  }
  result.done = request.done();
  return result;
}

/**
 * @brief 持续 drain completion queue，直到单个请求完成或当前队列没有更多回调。
 *
 * `max_drains` 限制最多执行多少轮 drain，用于测试或上层主循环避免一次处理过多回调。该函数不阻塞等待
 * 尚未入队的后台结果。
 */
template <typename Request>
[[nodiscard]] request_poll_result
drain_until_done(tasks::completion_queue& completions, const Request& request,
                 std::size_t max_drains = std::numeric_limits<std::size_t>::max()) {
  request_poll_result result;
  while (!request.done() && max_drains > 0) {
    --max_drains;
    const std::size_t drained = completions.drain();
    result.drained += drained;
    if (drained == 0) {
      break;
    }
  }

  result.done = request.done();
  return result;
}

/** @brief 判断一个请求集合是否都已进入终态。 */
template <typename Requests> [[nodiscard]] bool all_requests_done(const Requests& requests) {
  for (const auto& request : requests) {
    if (!request.done()) {
      return false;
    }
  }

  return true;
}

/**
 * @brief drain 一次 completion queue，并返回请求集合是否全部完成。
 *
 * 该函数适合跨帧轮询一批 `load_request<T>` 或 `preload_request`。
 */
template <typename Requests>
[[nodiscard]] request_poll_result poll_requests(tasks::completion_queue& completions,
                                                const Requests& requests) {
  request_poll_result result;
  if (!all_requests_done(requests)) {
    result.drained = completions.drain();
  }
  result.done = all_requests_done(requests);
  return result;
}

/**
 * @brief 持续 drain completion queue，直到请求集合全部完成或当前队列没有更多回调。
 *
 * 该函数不等待后台线程把新回调放入队列；如果返回 `done == false`，调用方应在后续帧再次调用。
 */
template <typename Requests>
[[nodiscard]] request_poll_result
drain_until_all_done(tasks::completion_queue& completions, const Requests& requests,
                     std::size_t max_drains = std::numeric_limits<std::size_t>::max()) {
  request_poll_result result;
  while (!all_requests_done(requests) && max_drains > 0) {
    --max_drains;
    const std::size_t drained = completions.drain();
    result.drained += drained;
    if (drained == 0) {
      break;
    }
  }

  result.done = all_requests_done(requests);
  return result;
}

namespace detail {

template <typename T> class load_request_access;
class preload_request_access;

[[nodiscard]] inline double clamp_progress(double value) noexcept {
  if (!(value >= 0.0)) {
    return 0.0;
  }
  if (value > 1.0) {
    return 1.0;
  }
  return value;
}

struct preload_request_state {
  explicit preload_request_state(memory_resource* memory)
      : resource(memory != nullptr ? memory : get_default_memory_resource()), result(resource),
        item_progress(allocator<double>{resource}), progress_stage(allocator<char>{resource}) {}

  memory_resource* resource = nullptr;
  mutable std::mutex mutex{};
  request_status status = request_status::queued;
  caors::error status_error{};
  preload_result result;
  std::size_t total = 0;
  std::size_t loaded = 0;
  std::size_t failed = 0;
  core::vector<double> item_progress;
  core::string progress_stage;
};

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
 * `tasks::completion_queue`，调用方在主线程 drain 后再通过 `status()`、`error()` 和 `try_get()`
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

  /** @brief 非阻塞尝试返回加载完成后的强 handle。未完成或失败时返回空 handle。 */
  [[nodiscard]] handle<T> try_get() const {
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

  /** @brief 返回 0.0 到 1.0 的非阻塞加载进度。终态请求返回 1.0。 */
  [[nodiscard]] double progress() const {
    if (state_ == nullptr) {
      return 0.0;
    }

    std::lock_guard lock{state_->mutex};
    if (state_->status == request_status::loaded || state_->status == request_status::failed ||
        state_->status == request_status::cancelled) {
      return 1.0;
    }

    return state_->progress;
  }

  /** @brief 返回最近一次 loader 上报的阶段文本。 */
  [[nodiscard]] core::string progress_stage() const {
    core::string result{allocator<char>{state_ != nullptr ? state_->resource : nullptr}};
    if (state_ == nullptr) {
      return result;
    }

    std::lock_guard lock{state_->mutex};
    result = state_->progress_stage;
    return result;
  }

private:
  friend class detail::load_request_access<T>;
  friend class load_request_table<T>;

  [[nodiscard]] bool shares_state(const load_request& other) const noexcept {
    return state_ == other.state_;
  }

  struct state_type {
    struct preload_observer {
      preload_item item{};
      std::weak_ptr<detail::preload_request_state> state{};
    };

    explicit state_type(memory_resource* resource)
        : resource(resource), progress_stage(allocator<char>{resource}),
          preload_observers(allocator<preload_observer>{resource}) {}

    memory_resource* resource = nullptr;
    mutable std::mutex mutex{};
    request_status status = request_status::queued;
    caors::error status_error{};
    handle<T> value{};
    double progress = 0.0;
    core::string progress_stage;
    bool commit_unconditionally = false;
    core::vector<preload_observer> preload_observers;
  };

  explicit load_request(std::shared_ptr<state_type> state) noexcept : state_(std::move(state)) {}

  std::shared_ptr<state_type> state_{};
};

/**
 * @brief 按 GUID 复用进行中的异步加载请求。
 *
 * 该表只保存 queued/loading 请求。完成、失败或取消的请求会在下一次查询或提交阶段被移除，
 * 因此不会阻止后续重新加载同一个 GUID。
 */
template <typename T> class load_request_table {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  explicit load_request_table(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        requests_(typename request_map::allocator_type{resource_}) {}

  load_request_table(const load_request_table&) = delete;
  load_request_table& operator=(const load_request_table&) = delete;
  load_request_table(load_request_table&&) = delete;
  load_request_table& operator=(load_request_table&&) = delete;

  /** @brief 查询进行中的请求；如果旧请求已经结束，会顺手清理并返回空请求。 */
  [[nodiscard]] load_request<T> find_pending(resource_guid guid) {
    if (!guid) {
      return {};
    }

    load_request<T> request;
    {
      std::lock_guard lock{mutex_};
      const auto position = requests_.find(guid);
      if (position == requests_.end()) {
        return {};
      }

      request = position->second;
    }

    const request_status status = request.status();
    if (status == request_status::queued || status == request_status::loading) {
      return request;
    }

    {
      std::lock_guard lock{mutex_};
      const auto position = requests_.find(guid);
      if (position != requests_.end() && position->second.shares_state(request)) {
        requests_.erase(position);
      }
    }

    return {};
  }

  /** @brief 保存进行中的请求。 */
  void store(resource_guid guid, load_request<T> request) {
    if (!guid || !request) {
      return;
    }

    std::lock_guard lock{mutex_};
    requests_[guid] = std::move(request);
  }

  /** @brief 移除 GUID 对应请求。 */
  void erase(resource_guid guid, const load_request<T>& request) {
    std::lock_guard lock{mutex_};
    const auto position = requests_.find(guid);
    if (position != requests_.end() && position->second.shares_state(request)) {
      requests_.erase(position);
    }
  }

  /** @brief 当前保存的进行中请求数量。 */
  [[nodiscard]] std::size_t size() const {
    std::lock_guard lock{mutex_};
    return requests_.size();
  }

private:
  using request_map = core::hash_map<resource_guid, load_request<T>, resource_guid_hash>;

  memory_resource* resource_ = nullptr;
  mutable std::mutex mutex_{};
  request_map requests_;
};

/**
 * @brief 异步依赖预加载请求视图。
 *
 * `preload_request` 是跨资源类型的聚合请求。它保存一次 `preload_plan`、已完成项数量和最终状态；
 * 具体资源 handle 仍由各自 `cache<T>` 持有，调用方通过 cache 或 database 查询已加载资源。
 */
class preload_request {
public:
  preload_request() noexcept = default;

  /** @brief 创建一个立即失败的预加载请求。 */
  [[nodiscard]] static preload_request failed(caors::error status,
                                              memory_resource* resource = nullptr);

  /** @brief 根据预加载计划创建请求；失败计划会直接生成 failed 请求。 */
  [[nodiscard]] static preload_request from_plan(preload_plan plan,
                                                 memory_resource* resource = nullptr);

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
   * 取消不会强制中断已经开始执行的 loader，但会阻止后续 completion queue commit 写入 cache。
   */
  [[nodiscard]] bool cancel() const;

  /** @brief 返回本次预加载计划和当前已完成项。未完成时 `status` 为 `resources / pending`。 */
  [[nodiscard]] preload_result result() const {
    if (state_ == nullptr) {
      preload_result result;
      result.status = {error_domain::resources, errc::invalid_argument};
      return result;
    }

    std::lock_guard lock{state_->mutex};
    preload_result result = state_->result;
    switch (state_->status) {
    case request_status::loaded:
      result.status = {};
      break;
    case request_status::failed:
      result.status = state_->status_error
                          ? state_->status_error
                          : caors::error{error_domain::resources, errc::backend_failure};
      break;
    case request_status::cancelled:
      result.status = {error_domain::resources, errc::cancelled};
      break;
    case request_status::queued:
    case request_status::loading:
      result.status = {error_domain::resources, errc::pending};
      break;
    }

    return result;
  }

  /** @brief 计划中需要实际提交异步加载的资源数量。 */
  [[nodiscard]] std::size_t total_count() const {
    if (state_ == nullptr) {
      return 0;
    }

    std::lock_guard lock{state_->mutex};
    return state_->total;
  }

  /** @brief 已成功提交到 cache 的资源数量。 */
  [[nodiscard]] std::size_t loaded_count() const {
    if (state_ == nullptr) {
      return 0;
    }

    std::lock_guard lock{state_->mutex};
    return state_->loaded;
  }

  /** @brief 已失败的资源数量。 */
  [[nodiscard]] std::size_t failed_count() const {
    if (state_ == nullptr) {
      return 0;
    }

    std::lock_guard lock{state_->mutex};
    return state_->failed;
  }

  /** @brief 尚未完成的资源数量。取消后返回 0。 */
  [[nodiscard]] std::size_t pending_count() const {
    if (state_ == nullptr) {
      return 0;
    }

    std::lock_guard lock{state_->mutex};
    if (state_->status == request_status::loaded || state_->status == request_status::failed ||
        state_->status == request_status::cancelled ||
        state_->loaded + state_->failed >= state_->total) {
      return 0;
    }

    return state_->total - state_->loaded - state_->failed;
  }

  /** @brief 返回 0.0 到 1.0 的非阻塞聚合进度。 */
  [[nodiscard]] double progress() const {
    if (state_ == nullptr) {
      return 0.0;
    }

    std::lock_guard lock{state_->mutex};
    if (state_->total == 0 || state_->status == request_status::loaded ||
        state_->status == request_status::failed || state_->status == request_status::cancelled) {
      return 1.0;
    }

    double progress = 0.0;
    for (double item_progress : state_->item_progress) {
      progress += detail::clamp_progress(item_progress);
    }

    return progress / static_cast<double>(state_->total);
  }

  /** @brief 返回最近一次底层 loader 上报的阶段文本。 */
  [[nodiscard]] core::string progress_stage() const {
    core::string result{allocator<char>{state_ != nullptr ? state_->resource : nullptr}};
    if (state_ == nullptr) {
      return result;
    }

    std::lock_guard lock{state_->mutex};
    result = state_->progress_stage;
    return result;
  }

private:
  friend class detail::preload_request_access;

  explicit preload_request(std::shared_ptr<detail::preload_request_state> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::preload_request_state> state_{};
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

  static void set_commit_unconditionally(const std::shared_ptr<state_type>& state, bool value) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    state->commit_unconditionally = state->commit_unconditionally || value;
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
    state->progress = 0.0;
    state->progress_stage.clear();
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
    state->progress = 1.0;
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
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return;
    }

    state->status = request_status::failed;
    state->status_error =
        status ? status : caors::error{error_domain::resources, errc::backend_failure};
    state->value = {};
    state->progress = 1.0;
  }

  [[nodiscard]] static bool cancel(const request_type& request) { return cancel(request.state_); }

  [[nodiscard]] static bool cancel(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    {
      std::lock_guard lock{state->mutex};
      if (state->status != request_status::queued && state->status != request_status::loading) {
        return false;
      }

      state->status = request_status::cancelled;
      state->status_error = {error_domain::resources, errc::cancelled};
      state->value = {};
      state->progress = 1.0;
    }

    notify_preload_observers(state);
    return true;
  }

  [[nodiscard]] static bool is_cancelled(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return true;
    }

    std::lock_guard lock{state->mutex};
    return state->status == request_status::cancelled;
  }

  static void observe_preload(const request_type& request, preload_item item,
                              const std::shared_ptr<preload_request_state>& preload_state);

  static void report_progress(const std::shared_ptr<state_type>& state, progress_update update);

  static void notify_preload_observers(const std::shared_ptr<state_type>& state);

  [[nodiscard]] static bool should_commit(const std::shared_ptr<state_type>& state);

private:
  using preload_observer = typename state_type::preload_observer;

  static void notify_preload_observer(const preload_observer& observer, request_status status,
                                      caors::error status_error);
};

class preload_request_access {
public:
  using request_type = preload_request;
  using state_type = preload_request_state;

  [[nodiscard]] static request_type make(memory_resource* resource = nullptr) {
    memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
    return request_type{
        std::allocate_shared<state_type>(allocator<state_type>{resolved}, resolved)};
  }

  [[nodiscard]] static std::shared_ptr<state_type> state(const request_type& request) noexcept {
    return request.state_;
  }

  static void set_plan(const request_type& request, preload_plan plan) {
    if (request.state_ == nullptr) {
      return;
    }

    std::lock_guard lock{request.state_->mutex};
    request.state_->result.plan = std::move(plan);
    request.state_->result.loaded.clear();
    request.state_->result.failed_items.clear();
    request.state_->result.status = {};
    request.state_->total = request.state_->result.plan.load_order.size();
    request.state_->loaded = 0;
    request.state_->failed = 0;
    request.state_->item_progress.assign(request.state_->total, 0.0);
    request.state_->progress_stage.clear();
    request.state_->status_error = {};

    if (request.state_->result.plan.failed()) {
      request.state_->status = request_status::failed;
      request.state_->status_error = request.state_->result.plan.status;
      request.state_->result.status = request.state_->status_error;
      append_preload_plan_failures(request.state_->result);
      request.state_->failed = request.state_->result.failed_items.size();
      return;
    }

    request.state_->status =
        request.state_->total == 0 ? request_status::loaded : request_status::queued;
  }

  [[nodiscard]] static bool loading(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return false;
    }

    if (state->total == 0) {
      state->status = request_status::loaded;
      state->result.status = {};
      return false;
    }

    state->status = request_status::loading;
    state->status_error = {};
    state->result.status = {error_domain::resources, errc::pending};
    return true;
  }

  static void complete_item(const std::shared_ptr<state_type>& state, preload_item item) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return;
    }

    state->result.loaded.push_back(item);
    ++state->loaded;
    if (std::size_t index = item_index(*state, item); index < state->item_progress.size()) {
      state->item_progress[index] = 1.0;
    }
    if (state->loaded + state->failed >= state->total) {
      state->status = request_status::loaded;
      state->status_error = {};
      state->result.status = {};
    }
  }

  static void fail(const request_type& request, caors::error status) {
    fail(request.state_, status);
  }

  static void fail(const std::shared_ptr<state_type>& state, caors::error status) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return;
    }

    state->status = request_status::failed;
    state->status_error =
        status ? status : caors::error{error_domain::resources, errc::backend_failure};
    state->result.status = state->status_error;
  }

  static void fail_all_items(const std::shared_ptr<state_type>& state, caors::error status) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return;
    }

    const caors::error failure =
        status ? status : caors::error{error_domain::resources, errc::backend_failure};
    for (const preload_item& item : state->result.plan.load_order) {
      append_preload_failure(state->result, item, failure);
    }
    state->failed = state->result.plan.load_order.size();
    for (double& item_progress : state->item_progress) {
      item_progress = 1.0;
    }
    state->status = request_status::failed;
    state->status_error = failure;
    state->result.status = state->status_error;
  }

  static void fail_item(const std::shared_ptr<state_type>& state, preload_item item,
                        caors::error status) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return;
    }

    ++state->failed;
    append_preload_failure(state->result, item,
                           status ? status
                                  : caors::error{error_domain::resources, errc::backend_failure});
    if (std::size_t index = item_index(*state, item); index < state->item_progress.size()) {
      state->item_progress[index] = 1.0;
    }
    state->status = request_status::failed;
    state->status_error =
        status ? status : caors::error{error_domain::resources, errc::backend_failure};
    state->result.status = state->status_error;
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
    state->result.status = state->status_error;
    return true;
  }

  [[nodiscard]] static bool should_continue(const std::shared_ptr<state_type>& state) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    return state->status != request_status::cancelled && state->status != request_status::failed &&
           state->status != request_status::loaded;
  }

  static void report_item_progress(const std::shared_ptr<state_type>& state, preload_item item,
                                   progress_update update) {
    if (state == nullptr) {
      return;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::cancelled || state->status == request_status::failed ||
        state->status == request_status::loaded) {
      return;
    }

    const std::size_t index = item_index(*state, item);
    if (index < state->item_progress.size()) {
      state->item_progress[index] = clamp_progress(update.value);
    }
    if (!update.stage.empty()) {
      state->progress_stage.assign(update.stage.data(), update.stage.size());
    }
  }

private:
  [[nodiscard]] static std::size_t item_index(const state_type& state, preload_item item) {
    const auto& values = state.result.plan.load_order;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (values[index] == item) {
        return index;
      }
    }

    return values.size();
  }
};

template <typename T>
void load_request_access<T>::observe_preload(
    const request_type& request, preload_item item,
    const std::shared_ptr<preload_request_state>& preload_state) {
  if (request.state_ == nullptr || preload_state == nullptr) {
    return;
  }

  preload_observer observer{item, preload_state};
  request_status status = request_status::cancelled;
  caors::error status_error{};
  {
    std::lock_guard lock{request.state_->mutex};
    status = request.state_->status;
    status_error = request.state_->status_error;
    if (status == request_status::queued || status == request_status::loading) {
      request.state_->preload_observers.push_back(std::move(observer));
      return;
    }
  }

  notify_preload_observer(observer, status, status_error);
}

template <typename T>
void load_request_access<T>::report_progress(const std::shared_ptr<state_type>& state,
                                             progress_update update) {
  if (state == nullptr) {
    return;
  }

  using observer_type = typename state_type::preload_observer;
  core::vector<observer_type> observers{allocator<observer_type>{state->resource}};
  {
    std::lock_guard lock{state->mutex};
    if (state->status != request_status::queued && state->status != request_status::loading) {
      return;
    }

    state->progress = clamp_progress(update.value);
    if (!update.stage.empty()) {
      state->progress_stage.assign(update.stage.data(), update.stage.size());
    }
    observers = state->preload_observers;
  }

  for (const observer_type& observer : observers) {
    const std::shared_ptr<preload_request_state> preload_state = observer.state.lock();
    preload_request_access::report_item_progress(preload_state, observer.item, update);
  }
}

template <typename T>
void load_request_access<T>::notify_preload_observers(const std::shared_ptr<state_type>& state) {
  if (state == nullptr) {
    return;
  }

  using observer_type = typename state_type::preload_observer;
  core::vector<observer_type> observers{allocator<observer_type>{state->resource}};
  request_status status = request_status::cancelled;
  caors::error status_error{};
  {
    std::lock_guard lock{state->mutex};
    if (state->status == request_status::queued || state->status == request_status::loading) {
      return;
    }

    status = state->status;
    status_error = state->status_error;
    observers.swap(state->preload_observers);
  }

  for (const observer_type& observer : observers) {
    notify_preload_observer(observer, status, status_error);
  }
}

template <typename T>
bool load_request_access<T>::should_commit(const std::shared_ptr<state_type>& state) {
  if (state == nullptr) {
    return false;
  }

  std::lock_guard lock{state->mutex};
  if (state->status == request_status::cancelled || state->status == request_status::failed) {
    return false;
  }
  if (state->commit_unconditionally) {
    return true;
  }

  for (const preload_observer& observer : state->preload_observers) {
    const std::shared_ptr<preload_request_state> preload_state = observer.state.lock();
    if (preload_state != nullptr && preload_request_access::should_continue(preload_state)) {
      return true;
    }
  }

  return false;
}

template <typename T>
void load_request_access<T>::notify_preload_observer(
    const typename load_request_access<T>::preload_observer& observer, request_status status,
    caors::error status_error) {
  const std::shared_ptr<preload_request_state> preload_state = observer.state.lock();
  if (preload_state == nullptr) {
    return;
  }

  switch (status) {
  case request_status::loaded:
    preload_request_access::complete_item(preload_state, observer.item);
    return;
  case request_status::failed:
    preload_request_access::fail_item(
        preload_state, observer.item,
        status_error ? status_error : caors::error{error_domain::resources, errc::backend_failure});
    return;
  case request_status::cancelled:
    preload_request_access::fail_item(preload_state, observer.item,
                                      {error_domain::resources, errc::cancelled});
    return;
  case request_status::queued:
  case request_status::loading:
    return;
  }
}

} // namespace detail

inline preload_request preload_request::failed(caors::error status, memory_resource* resource) {
  preload_request request = detail::preload_request_access::make(resource);
  detail::preload_request_access::fail(request, status);
  return request;
}

inline preload_request preload_request::from_plan(preload_plan plan, memory_resource* resource) {
  preload_request request =
      detail::preload_request_access::make(resource != nullptr ? resource : plan.resource);
  detail::preload_request_access::set_plan(request, std::move(plan));
  return request;
}

inline bool preload_request::cancel() const {
  if (state_ == nullptr) {
    return false;
  }

  return detail::preload_request_access::cancel(*this);
}

namespace detail {

template <typename T, typename StatePtr>
void load_resource_in_background(const StatePtr& state, cache<T>& target, loader<T>& source,
                                 resource_guid guid, tasks::completion_queue& completions,
                                 bool replace, load_request_table<T>* requests = nullptr,
                                 load_request<T> request = {}) {
  if (!load_request_access<T>::loading(state)) {
    if (requests != nullptr) {
      requests->erase(guid, request);
    }
    load_request_access<T>::notify_preload_observers(state);
    return;
  }

  if (!load_request_access<T>::should_commit(state)) {
    (void)load_request_access<T>::cancel(state);
    if (requests != nullptr) {
      requests->erase(guid, request);
    }
    return;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    StatePtr progress_state = state;
    progress_reporter reporter{&progress_state, [](void* context, progress_update update) noexcept {
                                 const auto& typed_state = *static_cast<const StatePtr*>(context);
                                 load_request_access<T>::report_progress(typed_state, update);
                               }};
    load_result<T> loaded = source.load(guid, target.resource(), reporter);
    if (loaded.failed()) {
      load_request_access<T>::fail(state, loaded.status);
      if (requests != nullptr) {
        requests->erase(guid, request);
      }
      load_request_access<T>::notify_preload_observers(state);
      return;
    }

    if (!load_request_access<T>::should_commit(state)) {
      (void)load_request_access<T>::cancel(state);
      if (requests != nullptr) {
        requests->erase(guid, request);
      }
      return;
    }

    error push_status = completions.push(tasks::task{
        target.resource(), [state, &target, guid, replace, requests, request,
                            loaded_resource = std::move(loaded.value)]() mutable {
          if (!load_request_access<T>::should_commit(state)) {
            (void)load_request_access<T>::cancel(state);
            if (requests != nullptr) {
              requests->erase(guid, request);
            }
            return error{};
          }

          handle<T> committed;
          const error status =
              commit_loaded_resource(target, guid, std::move(loaded_resource), replace, committed);
          if (status) {
            load_request_access<T>::fail(state, status);
            if (requests != nullptr) {
              requests->erase(guid, request);
            }
            load_request_access<T>::notify_preload_observers(state);
            return status;
          }

          (void)load_request_access<T>::complete(state, std::move(committed));
          if (requests != nullptr) {
            requests->erase(guid, request);
          }
          load_request_access<T>::notify_preload_observers(state);
          return error{};
        }});
    if (push_status) {
      load_request_access<T>::fail(state, push_status);
      if (requests != nullptr) {
        requests->erase(guid, request);
      }
      load_request_access<T>::notify_preload_observers(state);
    }
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    load_request_access<T>::fail(state, {error_domain::resources, errc::backend_failure});
    if (requests != nullptr) {
      requests->erase(guid, request);
    }
    load_request_access<T>::notify_preload_observers(state);
  }
#endif
}

template <typename T>
[[nodiscard]] load_request<T>
request_load_into_impl(cache<T>& target, loader<T>& source, resource_guid guid,
                       tasks::executor& executor, tasks::completion_queue& completions,
                       load_request_table<T>* requests, bool commit_unconditionally = true,
                       std::shared_ptr<preload_request_state> preload_state = {},
                       preload_item preload_observer_item = {}) {
  if (!guid) {
    load_request<T> request = load_request<T>::failed(
        {error_domain::resources, errc::invalid_argument}, target.resource());
    load_request_access<T>::observe_preload(request, preload_observer_item,
                                            std::move(preload_state));
    return request;
  }

  if (handle<T> existing = target.find_handle(guid)) {
    load_request<T> request = load_request<T>::completed(std::move(existing), target.resource());
    load_request_access<T>::observe_preload(request, preload_observer_item,
                                            std::move(preload_state));
    return request;
  }

  if (requests != nullptr) {
    if (load_request<T> pending = requests->find_pending(guid)) {
      load_request_access<T>::set_commit_unconditionally(load_request_access<T>::state(pending),
                                                         commit_unconditionally);
      load_request_access<T>::observe_preload(pending, preload_observer_item,
                                              std::move(preload_state));
      return pending;
    }
  }

  load_request<T> request = load_request_access<T>::make(target.resource());
  auto state = load_request_access<T>::state(request);
  load_request_access<T>::set_commit_unconditionally(state, commit_unconditionally);
  load_request_access<T>::observe_preload(request, preload_observer_item, std::move(preload_state));
  if (requests != nullptr) {
    requests->store(guid, request);
  }

  error submit_status = executor.submit(
      tasks::task{target.resource(),
                  [state, request, &target, &source, guid, &completions, requests]() mutable {
                    load_resource_in_background(state, target, source, guid, completions, false,
                                                requests, request);
                  }});
  if (submit_status) {
    load_request_access<T>::fail(state, submit_status);
    if (requests != nullptr) {
      requests->erase(guid, request);
    }
    load_request_access<T>::notify_preload_observers(state);
  }

  return request;
}

template <typename T>
[[nodiscard]] load_request<T>
request_reload_into_impl(cache<T>& target, loader<T>& source, resource_guid guid,
                         tasks::executor& executor, tasks::completion_queue& completions,
                         load_request_table<T>* requests) {
  if (!guid) {
    return load_request<T>::failed({error_domain::resources, errc::invalid_argument},
                                   target.resource());
  }

  if (!target.contains(guid)) {
    return load_request<T>::failed({error_domain::resources, errc::not_found}, target.resource());
  }

  if (requests != nullptr) {
    if (load_request<T> pending = requests->find_pending(guid)) {
      load_request_access<T>::set_commit_unconditionally(load_request_access<T>::state(pending),
                                                         true);
      return pending;
    }
  }

  load_request<T> request = load_request_access<T>::make(target.resource());
  auto state = load_request_access<T>::state(request);
  load_request_access<T>::set_commit_unconditionally(state, true);
  if (requests != nullptr) {
    requests->store(guid, request);
  }

  error submit_status = executor.submit(
      tasks::task{target.resource(),
                  [state, request, &target, &source, guid, &completions, requests]() mutable {
                    load_resource_in_background(state, target, source, guid, completions, true,
                                                requests, request);
                  }});
  if (submit_status) {
    load_request_access<T>::fail(state, submit_status);
    if (requests != nullptr) {
      requests->erase(guid, request);
    }
  }

  return request;
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
  return detail::request_load_into_impl<T>(target, source, guid, executor, completions,
                                           static_cast<load_request_table<T>*>(nullptr));
}

/**
 * @brief 异步加载资源并复用同 GUID 的进行中请求。
 */
template <typename T>
[[nodiscard]] load_request<T> request_load_into(cache<T>& target, loader<T>& source,
                                                resource_guid guid, tasks::executor& executor,
                                                tasks::completion_queue& completions,
                                                load_request_table<T>& requests) {
  return detail::request_load_into_impl(target, source, guid, executor, completions, &requests);
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
  return detail::request_reload_into_impl<T>(target, source, guid, executor, completions,
                                             static_cast<load_request_table<T>*>(nullptr));
}

/**
 * @brief 异步重载资源并复用同 GUID 的进行中重载请求。
 */
template <typename T>
[[nodiscard]] load_request<T> request_reload_into(cache<T>& target, loader<T>& source,
                                                  resource_guid guid, tasks::executor& executor,
                                                  tasks::completion_queue& completions,
                                                  load_request_table<T>& requests) {
  return detail::request_reload_into_impl(target, source, guid, executor, completions, &requests);
}

} // namespace caors::resources

#endif
