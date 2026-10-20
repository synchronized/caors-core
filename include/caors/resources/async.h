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
#include "caors/tasks/detail/abandon_guard.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
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
  /** @brief 本次从完成执行环境执行的回调数量。 */
  std::size_t drained = 0;
  /** @brief 目标请求或请求集合是否已经进入终态。 */
  bool done = false;
};

template <typename T> class load_request;
template <typename T> class load_request_table;
class preload_request;

/**
 * @brief drain 一次完成执行环境，并返回单个请求是否完成。
 *
 * 该函数不阻塞等待后台线程，只执行调用时已经进入执行环境的回调。
 */
template <typename Request>
[[nodiscard]] request_poll_result poll_request(tasks::execution_context& completions,
                                               const Request& request) {
  request_poll_result result;
  if (!request.done()) {
    result.drained = completions.drain();
  }
  result.done = request.done();
  return result;
}

/**
 * @brief 持续 drain 完成执行环境，直到单个请求完成或当前没有更多回调。
 *
 * `max_drains` 限制最多执行多少轮 drain，用于测试或上层主循环避免一次处理过多回调。该函数不阻塞等待
 * 尚未入队的后台结果。
 */
template <typename Request>
[[nodiscard]] request_poll_result
drain_until_done(tasks::execution_context& completions, const Request& request,
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
 * @brief drain 一次完成执行环境，并返回请求集合是否全部完成。
 *
 * 该函数适合跨帧轮询一批 `load_request<T>` 或 `preload_request`。
 */
template <typename Requests>
[[nodiscard]] request_poll_result poll_requests(tasks::execution_context& completions,
                                                const Requests& requests) {
  request_poll_result result;
  if (!all_requests_done(requests)) {
    result.drained = completions.drain();
  }
  result.done = all_requests_done(requests);
  return result;
}

/**
 * @brief 持续 drain 完成执行环境，直到请求集合全部完成或当前没有更多回调。
 *
 * 该函数不等待后台线程把新回调放入队列；如果返回 `done == false`，调用方应在后续帧再次调用。
 */
template <typename Requests>
[[nodiscard]] request_poll_result
drain_until_all_done(tasks::execution_context& completions, const Requests& requests,
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

struct request_waiter {
  std::shared_ptr<void> state;
  void (*notify)(const std::shared_ptr<void>&) noexcept = nullptr;
};

inline void notify_request_waiters(core::vector<request_waiter>& waiters) noexcept {
  for (request_waiter& waiter : waiters) {
    if (waiter.notify != nullptr) {
      waiter.notify(waiter.state);
    }
  }
}

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
        item_progress(allocator<double>{resource}), progress_stage(allocator<char>{resource}),
        waiters(allocator<request_waiter>{resource}), demands(allocator<request_waiter>{resource}) {
  }

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
  core::vector<request_waiter> waiters;
  // 批次只拥有自己的订阅；订阅反向弱引用批次，避免持有环。
  core::vector<request_waiter> demands;
  ~preload_request_state() { notify_request_waiters(demands); }
  void release_demands() {
    std::optional<core::vector<request_waiter>> detached;
    {
      std::lock_guard lock{mutex};
      if (status == request_status::queued || status == request_status::loading) {
        return;
      }
      detached.emplace(std::move(demands));
    }
    notify_request_waiters(*detached);
  }
};

template <typename T>
[[nodiscard]] error commit_loaded_resource(cache<T>& target, resource_guid guid,
                                           memory::unique_ptr<T> loaded, bool replace,
                                           handle<T>& committed, bool publish = true) {
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
      !publish || (replace ? target.replace(loaded_handle) : target.insert(loaded_handle));
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
 * 指定 completion executor，调用方运行该执行环境后再通过 `status()`、`error()` 和 `try_get()`
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

  /** @brief 返回请求共享状态使用的长期内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept {
    return state_ != nullptr ? state_->resource : get_default_memory_resource();
  }

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
   * 只取消本次订阅及其普通副本，不影响其他独立请求。全部订阅退出且缓存提交尚未开始时，
   * 底层加载会合作取消；不强制中断 loader。提交已开始时不回滚 cache。
   * 终态返回 false。等待者在状态锁释放后由终态发生线程通知，协程可另选恢复 executor。
   */
  [[nodiscard]] bool cancel() const {
    if (state_ == nullptr) {
      return false;
    }

    return detail::load_request_access<T>::cancel(*this);
  }

  /** @brief 显式取消同一底层加载的全部订阅；普通消费者应使用 cancel()。 */
  [[nodiscard]] bool cancel_shared() const {
    if (!state_) {
      return false;
    }
    auto parent = state_->operation.lock();
    return detail::load_request_access<T>::cancel(parent ? parent : state_);
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
          preload_observers(allocator<preload_observer>{resource}),
          waiters(allocator<detail::request_waiter>{resource}),
          subscribers(allocator<std::shared_ptr<state_type>>{resource}) {}

    ~state_type() {
      // 不让 cache 中的内部弱引用把请求控制块及其模块代码保留到 cache 析构。
      if (refresh && publication) {
        auto selection = std::static_pointer_cast<detail::refresh_selection>(publication);
        std::lock_guard lock{selection->mutex};
        selection->request.reset();
      }
    }
    std::weak_ptr<state_type> operation;
    std::shared_ptr<void> publication;
    bool refresh = false;
    bool subscription_operation = false;
    bool abandoned = false;
    bool committing = false;
    std::stop_source stop_source;
    memory_resource* resource = nullptr;
    mutable std::mutex mutex{};
    request_status status = request_status::queued;
    caors::error status_error{};
    handle<T> value{};
    double progress = 0.0;
    core::string progress_stage;
    bool commit_unconditionally = false;
    core::vector<preload_observer> preload_observers;
    core::vector<detail::request_waiter> waiters;
    core::vector<std::shared_ptr<state_type>> subscribers;
  };

  explicit load_request(std::shared_ptr<state_type> state) noexcept : state_(std::move(state)) {}

  std::shared_ptr<state_type> state_{};
};

/**
 * @brief 按 GUID 复用进行中的底层异步加载操作。
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

  /** @brief 为进行中的加载创建独立订阅；不存在进行中操作时返回空请求。 */
  [[nodiscard]] load_request<T> find_pending(resource_guid guid) {
    auto operation = find_pending_operation(guid);
    return operation ? detail::load_request_access<T>::subscribe(operation, true)
                     : load_request<T>{};
  }

private:
  friend class detail::load_request_access<T>;

  [[nodiscard]] load_request<T> find_pending_operation(resource_guid guid) {
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

    if (detail::load_request_access<T>::accepts_subscriptions(request)) {
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

public:
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

  /** @brief 返回请求共享状态使用的长期内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept {
    return state_ != nullptr ? state_->resource : get_default_memory_resource();
  }

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
   * 取消不会强制中断已经开始执行的 loader，但会阻止后续 completion executor commit 写入 cache。
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

  // 与 cache 的查询/修改一样，以下操作须在其所属线程执行，或由调用方统一串行化。
  static std::shared_ptr<void> publication(cache<T>& target, resource_guid guid) {
    const auto found = target.publications_.find(guid);
    return found == target.publications_.end() ? std::shared_ptr<void>{}
                                               : std::shared_ptr<void>{found->second};
  }
  static bool may_publish(const std::shared_ptr<state_type>& state, cache<T>& target,
                          resource_guid guid) {
    return state->publication == publication(target, guid);
  }
  static request_type current_refresh(cache<T>& target, resource_guid guid) {
    const auto found = target.publications_.find(guid);
    if (found == target.publications_.end())
      return {};
    std::shared_ptr<void> selected;
    {
      std::lock_guard lock{found->second->mutex};
      selected = found->second->request.lock();
    }
    auto state = std::static_pointer_cast<state_type>(std::move(selected));
    request_type request{std::move(state)};
    return request && accepts_subscriptions(request) ? request : request_type{};
  }
  static void select_refresh(cache<T>& target, resource_guid guid,
                             const std::shared_ptr<state_type>& state) {
    auto token = std::allocate_shared<detail::refresh_selection>(
        allocator<detail::refresh_selection>{target.resource()});
    token->request = state;
    target.publications_[guid] = token;
    state->publication = std::move(token);
    state->refresh = true;
  }

  [[nodiscard]] static request_type make(memory_resource* resource = nullptr) {
    memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
    return request_type{
        std::allocate_shared<state_type>(allocator<state_type>{resolved}, resolved)};
  }

  [[nodiscard]] static std::shared_ptr<state_type> state(const request_type& request) noexcept {
    return request.state_;
  }

  [[nodiscard]] static bool accepts_subscriptions(const request_type& operation) {
    const auto parent = state(operation);
    std::lock_guard lock{parent->mutex};
    return !parent->abandoned &&
           (parent->status == request_status::queued || parent->status == request_status::loading);
  }

  [[nodiscard]] static request_type subscribe(const request_type& operation, bool unconditional,
                                              preload_item item = {},
                                              std::shared_ptr<preload_request_state> preload = {}) {
    auto parent = state(operation);
    auto child = make(parent->resource);
    child.state_->operation = parent;
    set_commit_unconditionally(child.state_, unconditional);
    observe_preload(child, item, preload);
    request_status status;
    caors::error failure;
    handle<T> value;
    {
      std::lock_guard lock{parent->mutex};
      // 与最后订阅取消共用登记锁，放弃后的操作不能再吸收新消费者。
      if (parent->abandoned || parent->status == request_status::cancelled) {
        return {};
      }
      status = parent->status;
      if (status == request_status::queued || status == request_status::loading) {
        child.state_->status = status;
        child.state_->progress = parent->progress;
        child.state_->progress_stage = parent->progress_stage;
      }
      parent->subscription_operation = true;
      if (status == request_status::queued || status == request_status::loading) {
        parent->subscribers.emplace_back(child.state_);
      }
      failure = parent->status_error;
      value = parent->value;
    }
    if (preload) {
      bool active;
      {
        std::lock_guard lock{preload->mutex};
        active =
            preload->status == request_status::queued || preload->status == request_status::loading;
        if (active) {
          preload->demands.push_back(request_waiter{
              child.state_, [](const std::shared_ptr<void>& raw) noexcept {
                (void)load_request_access<T>::cancel(std::static_pointer_cast<state_type>(raw));
              }});
        }
      }
      if (!active) {
        (void)cancel(child);
      }
    }
    if (status == request_status::loaded) {
      complete(child, std::move(value));
      notify_preload_observers(child.state_);
    } else if (status == request_status::failed) {
      fail(child, failure);
    } else if (status == request_status::cancelled) {
      (void)cancel(child);
    }
    return child;
  }

  static void publish_subscribers(std::shared_ptr<state_type> state) {
    core::vector<std::shared_ptr<state_type>> children{
        allocator<std::shared_ptr<state_type>>{state->resource}};
    request_status status;
    caors::error failure;
    handle<T> value;
    {
      std::lock_guard lock{state->mutex};
      status = state->status;
      failure = state->status_error;
      value = state->value;
      children.swap(state->subscribers);
    }
    for (auto& child : children) {
      if (status == request_status::loaded) {
        (void)complete(child, value);
      } else if (status == request_status::failed) {
        fail(child, failure);
      } else if (status == request_status::cancelled) {
        (void)cancel(child);
      }
      notify_preload_observers(child);
    }
  }

  static void cancel_if_unused(const std::shared_ptr<state_type>& parent) {
    if (!parent) {
      return;
    }
    {
      std::lock_guard lock{parent->mutex};
      if (parent->committing || !parent->subscription_operation) {
        return;
      }
      for (auto& child : parent->subscribers) {
        if (should_commit(child)) {
          return;
        }
      }
      parent->abandoned = true;
    }
    (void)cancel(parent);
  }

  [[nodiscard]] static bool begin_commit(const std::shared_ptr<state_type>& state) {
    std::lock_guard lock{state->mutex};
    if (state->abandoned || state->status == request_status::cancelled ||
        state->status == request_status::failed) {
      return false;
    }
    if (state->subscription_operation) {
      bool needed = false;
      for (auto& child : state->subscribers) {
        if (should_commit(child)) {
          needed = true;
          break;
        }
      }
      if (!needed) {
        return false;
      }
    }
    state->committing = true;
    return true;
  }

  [[nodiscard]] static request_type find_pending_operation(load_request_table<T>& table,
                                                           resource_guid guid) {
    return table.find_pending_operation(guid);
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

    std::unique_lock lock{state->mutex};
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
    auto children = state->subscribers;
    lock.unlock();
    for (auto& child : children) {
      (void)loading(child);
    }
    return true;
  }

  static void complete(const request_type& request, handle<T> value) {
    (void)complete(request.state_, std::move(value));
  }

  [[nodiscard]] static bool complete(std::shared_ptr<state_type> state, handle<T> value) {
    if (state == nullptr) {
      return false;
    }

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
      std::lock_guard lock{state->mutex};
      if (state->status == request_status::cancelled || state->status == request_status::failed ||
          state->status == request_status::loaded) {
        return false;
      }

      state->status = request_status::loaded;
      state->status_error = {};
      state->value = std::move(value);
      state->progress = 1.0;
      waiters.swap(state->waiters);
    }

    notify_request_waiters(waiters);
    publish_subscribers(state);
    return true;
  }

  static void fail(const request_type& request, caors::error status) {
    fail(request.state_, status);
  }

  static void fail(std::shared_ptr<state_type> state, caors::error status) {
    if (state == nullptr) {
      return;
    }

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
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
      waiters.swap(state->waiters);
    }

    notify_request_waiters(waiters);
    publish_subscribers(state);
    notify_preload_observers(state);
  }

  [[nodiscard]] static bool cancel(const request_type& request) { return cancel(request.state_); }

  [[nodiscard]] static bool cancel(std::shared_ptr<state_type> state) {
    if (state == nullptr) {
      return false;
    }

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
      std::lock_guard lock{state->mutex};
      if (state->status != request_status::queued && state->status != request_status::loading) {
        return false;
      }

      state->status = request_status::cancelled;
      state->status_error = {error_domain::resources, errc::cancelled};
      state->value = {};
      state->progress = 1.0;
      waiters.swap(state->waiters);
    }

    state->stop_source.request_stop();
    notify_request_waiters(waiters);
    notify_preload_observers(state);
    publish_subscribers(state);
    cancel_if_unused(state->operation.lock());
    return true;
  }

  [[nodiscard]] static bool add_waiter(const std::shared_ptr<state_type>& state,
                                       request_waiter waiter) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::loaded || state->status == request_status::failed ||
        state->status == request_status::cancelled) {
      return false;
    }
    state->waiters.push_back(std::move(waiter));
    return true;
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

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
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
        waiters.swap(state->waiters);
      }
    }

    state->release_demands();
    notify_request_waiters(waiters);
  }

  static void fail(const request_type& request, caors::error status) {
    fail(request.state_, status);
  }

  static void fail(const std::shared_ptr<state_type>& state, caors::error status) {
    if (state == nullptr) {
      return;
    }

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
      std::lock_guard lock{state->mutex};
      if (state->status == request_status::cancelled || state->status == request_status::failed ||
          state->status == request_status::loaded) {
        return;
      }

      state->status = request_status::failed;
      state->status_error =
          status ? status : caors::error{error_domain::resources, errc::backend_failure};
      state->result.status = state->status_error;
      waiters.swap(state->waiters);
    }

    state->release_demands();
    notify_request_waiters(waiters);
  }

  static void fail_all_items(const std::shared_ptr<state_type>& state, caors::error status) {
    if (state == nullptr) {
      return;
    }

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
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
      waiters.swap(state->waiters);
    }

    state->release_demands();
    notify_request_waiters(waiters);
  }

  static void fail_item(const std::shared_ptr<state_type>& state, preload_item item,
                        caors::error status) {
    if (state == nullptr) {
      return;
    }

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
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
      waiters.swap(state->waiters);
    }

    state->release_demands();
    notify_request_waiters(waiters);
  }

  [[nodiscard]] static bool cancel(const request_type& request) { return cancel(request.state_); }

  [[nodiscard]] static bool cancel(std::shared_ptr<state_type> state) {
    if (state == nullptr) {
      return false;
    }

    core::vector<request_waiter> waiters{allocator<request_waiter>{state->resource}};
    {
      std::lock_guard lock{state->mutex};
      if (state->status != request_status::queued && state->status != request_status::loading) {
        return false;
      }

      state->status = request_status::cancelled;
      state->status_error = {error_domain::resources, errc::cancelled};
      state->result.status = state->status_error;
      waiters.swap(state->waiters);
    }

    state->release_demands();
    notify_request_waiters(waiters);
    return true;
  }

  [[nodiscard]] static bool add_waiter(const std::shared_ptr<state_type>& state,
                                       request_waiter waiter) {
    if (state == nullptr) {
      return false;
    }

    std::lock_guard lock{state->mutex};
    if (state->status == request_status::loaded || state->status == request_status::failed ||
        state->status == request_status::cancelled) {
      return false;
    }
    state->waiters.push_back(std::move(waiter));
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
  core::vector<std::shared_ptr<state_type>> children{
      allocator<std::shared_ptr<state_type>>{state->resource}};
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
    children = state->subscribers;
  }

  for (auto& child : children) {
    report_progress(child, update);
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
  if (state->subscription_operation) {
    for (auto& child : state->subscribers) {
      if (should_commit(child)) {
        return true;
      }
    }
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
void finish_resource_load(const StatePtr& state, cache<T>& target, resource_guid guid,
                          const tasks::executor& completion_executor, bool replace,
                          load_request_table<T>* requests, load_request<T> request,
                          load_result<T> loaded, std::shared_ptr<void> activity = {}) noexcept {
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
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

    auto abandonment = std::allocate_shared<tasks::detail::abandonment>(
        allocator<tasks::detail::abandonment>{target.resource()},
        tasks::task{target.resource(), [state]() noexcept {
                      load_request_access<T>::fail(state,
                                                   {error_domain::resources, errc::cancelled});
                      load_request_access<T>::notify_preload_observers(state);
                    }});
    error push_status = completion_executor.post(tasks::task{
        target.resource(), [state, &target, guid, replace, requests, request,
                            loaded_resource = std::move(loaded.value), activity,
                            abandoned = tasks::detail::abandon_guard{abandonment}]() mutable {
          abandoned.release();
          if (!load_request_access<T>::begin_commit(state)) {
            (void)load_request_access<T>::cancel(state);
            if (requests != nullptr) {
              requests->erase(guid, request);
            }
            return error{};
          }

          handle<T> committed;
          error status;
#if CAORS_HAS_EXCEPTIONS
          try {
#endif
            const bool publish = load_request_access<T>::may_publish(state, target, guid);
            status = commit_loaded_resource(target, guid, std::move(loaded_resource),
                                            state->refresh ? target.contains(guid) : replace,
                                            committed, publish);
#if CAORS_HAS_EXCEPTIONS
          } catch (const std::bad_alloc&) {
            status = {error_domain::resources, errc::out_of_memory};
          } catch (...) {
            status = {error_domain::resources, errc::backend_failure};
          }
#endif
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
    if (!push_status) {
      abandonment->arm();
    }
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

template <typename StatePtr> struct async_progress_state {
  StatePtr state;
  std::shared_ptr<void> activity;
};

template <typename T, typename StatePtr>
void load_resource_in_background(const StatePtr& state, cache<T>& target, loader<T>& source,
                                 resource_guid guid, const tasks::executor& worker,
                                 const tasks::executor& completion_executor, bool replace,
                                 load_request_table<T>* requests = nullptr,
                                 load_request<T> request = {}, std::shared_ptr<void> activity = {},
                                 std::shared_ptr<const provider_snapshot> sources = {}) {
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    if (!load_request_access<T>::loading(state) || !load_request_access<T>::should_commit(state)) {
      (void)load_request_access<T>::cancel(state);
      if (requests) {
        requests->erase(guid, request);
      }
      return;
    }
    auto completion = load_completion_access<T>::make(
        target.resource(), [state, &target, guid, completion_executor, replace, requests, request,
                            activity](load_result<T> loaded) noexcept {
          finish_resource_load(state, target, guid, completion_executor, replace, requests, request,
                               std::move(loaded), activity);
        });
    auto receipt = load_completion_access<T>::receipt(completion);
    using progress_state = async_progress_state<StatePtr>;
    auto progress_owner = std::allocate_shared<progress_state>(
        allocator<progress_state>{target.resource()}, state, activity);
    async_load_context context{
        guid, target.resource(), worker, state->stop_source.get_token(),
        load_progress{progress_owner, [](void* value, progress_update update) noexcept {
                        load_request_access<T>::report_progress(
                            static_cast<progress_state*>(value)->state, update);
                      }}};
    context.sources = std::move(sources);
    error status;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      status = source.start(std::move(context), std::move(completion));
#if CAORS_HAS_EXCEPTIONS
    } catch (const std::bad_alloc&) {
      status = {error_domain::resources, errc::out_of_memory};
    } catch (...) {
      status = {error_domain::resources, errc::backend_failure};
    }
#endif
    load_completion_access<T>::resolve(receipt, status);
#if CAORS_HAS_EXCEPTIONS
  } catch (const std::bad_alloc&) {
    load_request_access<T>::fail(state, {error_domain::resources, errc::out_of_memory});
    if (requests) {
      requests->erase(guid, request);
    }
  } catch (...) {
    load_request_access<T>::fail(state, {error_domain::resources, errc::backend_failure});
    if (requests) {
      requests->erase(guid, request);
    }
  }
#endif
}

template <typename T>
[[nodiscard]] load_request<T>
request_load_into_impl(cache<T>& target, loader<T>& source, resource_guid guid,
                       const tasks::executor& executor, const tasks::executor& completion_executor,
                       load_request_table<T>* requests, bool commit_unconditionally = true,
                       std::shared_ptr<preload_request_state> preload_state = {},
                       preload_item preload_observer_item = {}, std::shared_ptr<void> activity = {},
                       std::shared_ptr<const provider_snapshot> sources = {},
                       const provider* snapshot_provider = nullptr) {
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

  if (auto refresh = load_request_access<T>::current_refresh(target, guid)) {
    if (auto subscriber = load_request_access<T>::subscribe(refresh, commit_unconditionally,
                                                            preload_observer_item, preload_state))
      return subscriber;
  }
  if (requests != nullptr) {
    if (load_request<T> pending = load_request_access<T>::find_pending_operation(*requests, guid);
        pending &&
        load_request_access<T>::may_publish(load_request_access<T>::state(pending), target, guid)) {
      if (auto subscriber = load_request_access<T>::subscribe(
              pending, commit_unconditionally, preload_observer_item, preload_state)) {
        return subscriber;
      }
    }
  }

  if (!sources && snapshot_provider)
    sources = snapshot_provider->snapshot();

  load_request<T> request = load_request_access<T>::make(target.resource());
  auto state = load_request_access<T>::state(request);
  state->publication = load_request_access<T>::publication(target, guid);
  auto subscriber = load_request_access<T>::subscribe(
      request, commit_unconditionally, preload_observer_item, std::move(preload_state));
  if (requests != nullptr) {
    requests->store(guid, request);
  }

  auto abandonment = std::allocate_shared<tasks::detail::abandonment>(
      allocator<tasks::detail::abandonment>{target.resource()},
      tasks::task{target.resource(), [state]() noexcept {
                    load_request_access<T>::fail(state, {error_domain::resources, errc::cancelled});
                    load_request_access<T>::notify_preload_observers(state);
                  }});
  error submit_status = executor.post(tasks::task{
      target.resource(),
      [state, request, &target, &source, guid, executor, completion_executor, requests, activity,
       sources, abandoned = tasks::detail::abandon_guard{abandonment}]() mutable {
        load_resource_in_background(state, target, source, guid, executor, completion_executor,
                                    false, requests, request, activity, sources);
        abandoned.release();
      }});
  if (!submit_status) {
    abandonment->arm();
  }
  if (submit_status) {
    load_request_access<T>::fail(state, submit_status);
    if (requests != nullptr) {
      requests->erase(guid, request);
    }
    load_request_access<T>::notify_preload_observers(state);
  }

  return subscriber;
}

template <typename T>
[[nodiscard]] load_request<T> request_reload_into_impl(
    cache<T>& target, loader<T>& source, resource_guid guid, const tasks::executor& executor,
    const tasks::executor& completion_executor, load_request_table<T>* requests,
    std::shared_ptr<void> activity = {}, std::shared_ptr<const provider_snapshot> sources = {},
    const provider* snapshot_provider = nullptr, bool refresh = false) {
  if (!guid) {
    return load_request<T>::failed({error_domain::resources, errc::invalid_argument},
                                   target.resource());
  }

  if (!refresh && !target.contains(guid)) {
    return load_request<T>::failed({error_domain::resources, errc::not_found}, target.resource());
  }

  if (!refresh) {
    if (auto selected = load_request_access<T>::current_refresh(target, guid)) {
      if (auto subscriber = load_request_access<T>::subscribe(selected, true))
        return subscriber;
    }
  }
  if (!refresh && requests != nullptr) {
    if (load_request<T> pending = load_request_access<T>::find_pending_operation(*requests, guid);
        pending &&
        load_request_access<T>::may_publish(load_request_access<T>::state(pending), target, guid)) {
      if (auto subscriber = load_request_access<T>::subscribe(pending, true)) {
        return subscriber;
      }
    }
  }

  if (!sources && snapshot_provider)
    sources = snapshot_provider->snapshot();

  load_request<T> request = load_request_access<T>::make(target.resource());
  auto state = load_request_access<T>::state(request);
  state->publication = load_request_access<T>::publication(target, guid);
  auto subscriber = load_request_access<T>::subscribe(request, true);
  if (requests != nullptr) {
    requests->store(guid, request);
  }

  auto abandonment = std::allocate_shared<tasks::detail::abandonment>(
      allocator<tasks::detail::abandonment>{target.resource()},
      tasks::task{target.resource(), [state]() noexcept {
                    load_request_access<T>::fail(state, {error_domain::resources, errc::cancelled});
                    load_request_access<T>::notify_preload_observers(state);
                  }});
  error submit_status;
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    if (refresh)
      load_request_access<T>::select_refresh(target, guid, state);
    submit_status = executor.post(tasks::task{
        target.resource(),
        [state, request, &target, &source, guid, executor, completion_executor, requests, activity,
         sources, abandoned = tasks::detail::abandon_guard{abandonment}]() mutable {
          load_resource_in_background(state, target, source, guid, executor, completion_executor,
                                      true, requests, request, activity, sources);
          abandoned.release();
        }});
#if CAORS_HAS_EXCEPTIONS
  } catch (const std::bad_alloc&) {
    submit_status = {error_domain::resources, errc::out_of_memory};
  } catch (...) {
    submit_status = {error_domain::resources, errc::backend_failure};
  }
#endif
  if (!submit_status) {
    abandonment->arm();
  }
  if (submit_status) {
    load_request_access<T>::fail(state, submit_status);
    if (requests != nullptr) {
      requests->erase(guid, request);
    }
  }

  return subscriber;
}

} // namespace detail

/**
 * @brief 异步加载资源并通过指定执行目标提交到 cache。
 *
 * 已缓存资源会立即返回 loaded 请求。未缓存资源会在 executor 中调用 loader，随后把 cache
 * 插入动作投递到 completion executor；调用方负责在拥有该执行环境的线程运行它。
 */
template <typename T>
[[nodiscard]] load_request<T> request_load_into(cache<T>& target, loader<T>& source,
                                                resource_guid guid, tasks::executor executor,
                                                tasks::executor completion_executor) {
  return detail::request_load_into_impl<T>(target, source, guid, executor, completion_executor,
                                           static_cast<load_request_table<T>*>(nullptr));
}

/**
 * @brief 异步加载资源并复用同 GUID 的进行中请求。
 */
template <typename T>
[[nodiscard]] load_request<T>
request_load_into(cache<T>& target, loader<T>& source, resource_guid guid, tasks::executor executor,
                  tasks::executor completion_executor, load_request_table<T>& requests) {
  return detail::request_load_into_impl(target, source, guid, executor, completion_executor,
                                        &requests);
}

/**
 * @brief 异步重载资源并通过指定执行目标替换 cache 绑定。
 *
 * cache 未包含该 GUID 时立即失败。加载或 commit 失败时旧资源保持不变。
 */
template <typename T>
[[nodiscard]] load_request<T> request_reload_into(cache<T>& target, loader<T>& source,
                                                  resource_guid guid, tasks::executor executor,
                                                  tasks::executor completion_executor) {
  return detail::request_reload_into_impl<T>(target, source, guid, executor, completion_executor,
                                             static_cast<load_request_table<T>*>(nullptr));
}

/**
 * @brief 异步重载资源并复用同 GUID 的进行中重载请求。
 */
template <typename T>
[[nodiscard]] load_request<T> request_reload_into(cache<T>& target, loader<T>& source,
                                                  resource_guid guid, tasks::executor executor,
                                                  tasks::executor completion_executor,
                                                  load_request_table<T>& requests) {
  return detail::request_reload_into_impl(target, source, guid, executor, completion_executor,
                                          &requests);
}

/**
 * @brief 显式选择新来源并发起独立加载，成功后发布为默认绑定；不要求已有缓存。
 * @note 最新登记的刷新意图拥有发布资格；失败保留旧缓存，不恢复更早操作的资格。
 * 旧请求仍可返回自己的结果。仅刷新单资源；调用与 cache 提交须在同一所属线程或统一串行化。
 */
template <typename T>
[[nodiscard]] load_request<T> request_refresh_into(cache<T>& target, loader<T>& source,
                                                   resource_guid guid, tasks::executor worker,
                                                   tasks::executor completion,
                                                   load_request_table<T>& requests) {
  return detail::request_reload_into_impl(target, source, guid, worker, completion, &requests, {},
                                          {}, nullptr, true);
}

} // namespace caors::resources

#endif
