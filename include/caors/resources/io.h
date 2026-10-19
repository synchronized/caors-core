// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_IO_H_
#define CAORS_RESOURCES_IO_H_

#include "caors/resources/cache.h"
#include "caors/result.h"
#include "caors/tasks/executor.h"

#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::resources {

template <typename T> using load_result = result<memory::unique_ptr<T>>;
template <typename T> using handle_result = result<handle<T>>;

/** @brief 资源加载进度快照。 */
struct progress_update {
  /** @brief 0.0 到 1.0 的进度值，调用方会按范围裁剪。 */
  double value = 0.0;
  /** @brief 当前加载阶段文本。该视图只在 report 调用期间有效。 */
  std::string_view stage{};
};

/**
 * @brief loader 可选使用的进度上报器。
 *
 * `progress_reporter` 是轻量非拥有回调视图，只在 `loader::load()` 调用期间有效。loader
 * 不应保存它或跨线程延后调用。
 */
class progress_reporter final {
public:
  using report_fn = void (*)(void* context, progress_update update) noexcept;

  progress_reporter() noexcept = default;

  progress_reporter(void* context, report_fn report) noexcept
      : context_(context), report_(report) {}

  /** @brief 判断当前 reporter 是否绑定到请求状态。 */
  [[nodiscard]] bool valid() const noexcept { return report_ != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  /** @brief 上报一次进度快照。 */
  void report(double value, std::string_view stage = {}) const noexcept {
    if (report_ != nullptr) {
      report_(context_, progress_update{value, stage});
    }
  }

  /** @brief 便捷调用形式，等价于 `report(value, stage)`。 */
  void operator()(double value, std::string_view stage = {}) const noexcept {
    report(value, stage);
  }

private:
  void* context_ = nullptr;
  report_fn report_ = nullptr;
};

/** @brief 可跨异步边界保存的进度通道，持有上报所需状态。 */
class load_progress final {
public:
  load_progress() = default;
  /** @brief 持有状态并绑定 noexcept 回调；回调不能反向强持有本通道。 */
  load_progress(std::shared_ptr<void> owner, progress_reporter::report_fn report) noexcept
      : owner_(std::move(owner)), report_(report) {}
  /** @brief 上报进度；允许并发，空通道无效果。 */
  void report(double value, std::string_view stage = {}) const noexcept {
    if (report_) {
      report_(owner_.get(), {value, stage});
    }
  }

private:
  std::shared_ptr<void> owner_;
  progress_reporter::report_fn report_ = nullptr;
};

/**
 * @brief 异步加载的发起上下文，可按值保存到实际操作结束。
 * @note resource 非拥有，必须覆盖结果和所有回调；worker 不拥有执行环境。
 * stop 仅表示协作取消，宿主负责转发到其 loop 线程并清理迟到回调。
 */
struct async_load_context {
  /** @brief 待加载资源的稳定身份。 */
  resource_guid guid{};
  /** @brief 非拥有结果资源，必须覆盖对象、控制块和回调的实际寿命。 */
  memory_resource* resource = nullptr;
  /** @brief 发起及解析任务的调度入口；执行环境由宿主持有。 */
  tasks::executor worker{};
  /** @brief 共享操作的协作取消信号，不表示底层 I/O 已经停止。 */
  std::stop_token stop{};
  /** @brief 可复制、拥有状态的进度通道，不能形成强引用环。 */
  load_progress progress{};
};

namespace detail {
template <typename T> struct load_completion_access;
}

/**
 * @brief move-only 资源完成凭证；成功或错误只交付一次。
 *
 * 可同步完成或移交任意回调线程；同一对象的移动/销毁不得与成员调用并发。
 * 接受操作后未交付即销毁，会报告 cancelled 失败。拒绝原因由启动返回值优先保留。
 * 结果所有权传入 complete；重复交付返回 false，并在调用线程清理多余结果。
 */
template <typename T> class load_completion final {
  struct state {
    virtual ~state() = default;
    virtual void deliver(load_result<T> value) noexcept = 0;
    std::mutex mutex;
    bool accepted = false;
    bool submitted = false;
    bool finished = false;
    std::optional<load_result<T>> pending;

    bool submit(load_result<T> value) noexcept {
      {
        std::lock_guard lock{mutex};
        if (submitted || finished) {
          return false;
        }
        submitted = true;
        if (!accepted) {
          pending.emplace(std::move(value));
          return true;
        }
        finished = true;
      }
      deliver(std::move(value));
      return true;
    }
    void resolve(error status) noexcept {
      std::optional<load_result<T>> value;
      {
        std::lock_guard lock{mutex};
        accepted = true;
        if (finished) {
          return;
        }
        if (status) {
          finished = true;
          value.emplace(load_result<T>{status, {}});
        } else if (pending) {
          finished = true;
          value.emplace(std::move(*pending));
        }
      }
      // pending 的资源析构与交付均在锁外，允许用户回调重入。
      if (value) {
        deliver(std::move(*value));
      }
    }
  };
  template <typename F> struct callback_state final : state {
    explicit callback_state(F fn) : callback(std::move(fn)) {}
    ~callback_state() override { this->pending.reset(); }
    void deliver(load_result<T> value) noexcept override { callback(std::move(value)); }
    F callback;
  };
  friend struct detail::load_completion_access<T>;

public:
  load_completion() = default;
  load_completion(const load_completion&) = delete;
  load_completion& operator=(const load_completion&) = delete;
  load_completion(load_completion&& other) noexcept : state_(std::move(other.state_)) {}
  load_completion& operator=(load_completion&& other) noexcept {
    if (this != &other) {
      abandon();
      state_ = std::move(other.state_);
    }
    return *this;
  }
  ~load_completion() { abandon(); }
  /** @brief 交付资源或错误；只能成功交付一次。空成功结果会归一化为 backend_failure。 */
  [[nodiscard]] bool complete(load_result<T> value) const noexcept {
    auto current = state_;
    if (!current) {
      return false;
    }
    if (!value.status && !value.value) {
      value.status = {error_domain::resources, errc::backend_failure};
    }
    return current->submit(std::move(value));
  }

private:
  void abandon() noexcept {
    auto current = std::move(state_);
    if (current) {
      (void)current->submit({{error_domain::resources, errc::cancelled}, {}});
    }
  }
  std::shared_ptr<state> state_;
};

namespace detail {
template <typename T> struct load_completion_access {
  template <typename F> static load_completion<T> make(memory_resource* resource, F fn) {
    static_assert(std::is_nothrow_invocable_v<F&, load_result<T>>);
    load_completion<T> result;
    result.state_ = std::allocate_shared<typename load_completion<T>::template callback_state<F>>(
        allocator<typename load_completion<T>::template callback_state<F>>{resource},
        std::move(fn));
    return result;
  }
  static auto receipt(const load_completion<T>& value) { return value.state_; }
  template <typename State> static void resolve(const State& value, error status) noexcept {
    value->resolve(status);
  }
};
} // namespace detail

/**
 * @brief 指定资源类型的加载器接口。
 *
 * `loader<T>` 不绑定文件路径、资源包或异步系统。实现类只需要按 GUID 构造资源并返回所有权；
 * 调用方可选择直接持有结果，或通过 `load_handle_into()` 转入 `resources::cache<T>`。
 */
template <typename T> class loader {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  loader() = default;
  virtual ~loader() = default;

  loader(const loader&) = delete;
  loader& operator=(const loader&) = delete;
  loader(loader&&) = delete;
  loader& operator=(loader&&) = delete;

  /** @brief 按 GUID 加载资源，返回资源所有权。 */
  [[nodiscard]] virtual load_result<resource_type> load(resource_guid guid,
                                                        memory_resource* resource = nullptr) = 0;

  /**
   * @brief 按 GUID 加载资源，并可选择上报进度。
   *
   * 默认实现忽略 reporter 并转调旧的两参数 `load()`，因此已有 loader 不需要修改。
   */
  [[nodiscard]] virtual load_result<resource_type>
  load(resource_guid guid, memory_resource* resource, progress_reporter reporter) {
    (void)reporter;
    return load(guid, resource);
  }

  /**
   * @brief 在 worker 中发起加载；默认适配同步 load，已有实现无需修改。
   * @note 覆盖实现接受后必须完成或销毁凭证；失败返回不得保留凭证或继续回调。
   * context 可移动到宿主操作，禁止阻塞等待其 I/O；异常由请求入口转换为失败。
   */
  [[nodiscard]] virtual error start(async_load_context context, load_completion<T> completion) {
    progress_reporter reporter{&context.progress, [](void* value, progress_update update) noexcept {
                                 static_cast<load_progress*>(value)->report(update.value,
                                                                            update.stage);
                               }};
    (void)completion.complete(load(context.guid, context.resource, reporter));
    return {};
  }
};

/**
 * @brief 延迟交付资源的 loader 协议；与同步 loader 共用请求管线。
 * @note 同步调用返回 unsupported。start 可在返回前完成，或将凭证移交宿主 I/O 回调。
 * 此类型不创建线程、不拥有宿主 loop；实现对象和 allocator 必须覆盖全部操作及回调。
 */
template <typename T> class async_loader : public loader<T> {
public:
  /** @brief 原生异步实现不通过阻塞等待提供同步加载。 */
  [[nodiscard]] load_result<T> load(resource_guid, memory_resource* = nullptr) override {
    return {{error_domain::resources, errc::unsupported}, {}};
  }
  /** @brief 接受并异步交付资源，或立即返回拒绝原因；不得遗留未收口操作。 */
  [[nodiscard]] virtual error start(async_load_context context,
                                    load_completion<T> completion) override = 0;
};

/**
 * @brief 指定资源类型的保存器接口。
 *
 * `saver<T>` 只表达“保存一个已有资源”的动作，不规定目标位置、格式或同步策略。
 */
template <typename T> class saver {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  saver() = default;
  virtual ~saver() = default;

  saver(const saver&) = delete;
  saver& operator=(const saver&) = delete;
  saver(saver&&) = delete;
  saver& operator=(saver&&) = delete;

  /** @brief 保存资源。资源应携带非 nil GUID。 */
  [[nodiscard]] virtual error save(const resource_type& value) = 0;
};

/**
 * @brief 使用加载器把资源加载进 cache。
 *
 * 如果 cache 已包含该 GUID，直接返回已有强 handle。否则调用 loader 加载资源并把所有权转入 cache。
 */
template <typename T>
[[nodiscard]] handle_result<T> load_handle_into(cache<T>& target, loader<T>& source,
                                                resource_guid guid) {
  if (!guid) {
    return {{error_domain::resources, errc::invalid_argument}, {}};
  }

  if (handle<T> existing = target.find_handle(guid)) {
    return {{}, existing};
  }

  load_result<T> loaded = source.load(guid, target.resource());
  if (loaded.failed()) {
    return {loaded.status, {}};
  }

  if (loaded.value == nullptr) {
    return {{error_domain::resources, errc::backend_failure}, {}};
  }

  handle<T> loaded_handle = adopt_handle(std::move(loaded.value), target.resource());
  if (!loaded_handle) {
    return {{error_domain::resources, errc::out_of_memory}, {}};
  }

  if (!loaded_handle->has_guid()) {
    loaded_handle->set_guid(guid);
  }
  if (loaded_handle->guid() != guid) {
    return {{error_domain::resources, errc::inconsistent_type}, {}};
  }

  if (!target.insert(loaded_handle)) {
    return {{error_domain::resources, errc::already_exists}, {}};
  }

  return {{}, loaded_handle};
}

/**
 * @brief 重新加载并替换 cache 中已有资源。
 *
 * 该函数只处理已存在资源的同步重载：cache 未包含该 GUID 时返回 `errc::not_found`。加载、解析或
 * 身份校验失败时不会修改 cache 中旧资源；只有新资源完整加载并通过 GUID 校验后才替换旧资源。
 * 外部旧强 handle 会继续持有旧资源；已解析到旧资源对象的外部引用不会自动失效，调用方应在重载后
 * 重新解析相关 `resource_ref<T>`。
 */
template <typename T>
[[nodiscard]] handle_result<T> reload_handle_into(cache<T>& target, loader<T>& source,
                                                  resource_guid guid) {
  if (!guid) {
    return {{error_domain::resources, errc::invalid_argument}, {}};
  }

  if (!target.contains(guid)) {
    return {{error_domain::resources, errc::not_found}, {}};
  }

  load_result<T> loaded = source.load(guid, target.resource());
  if (loaded.failed()) {
    return {loaded.status, {}};
  }

  if (loaded.value == nullptr) {
    return {{error_domain::resources, errc::backend_failure}, {}};
  }

  handle<T> loaded_handle = adopt_handle(std::move(loaded.value), target.resource());
  if (!loaded_handle) {
    return {{error_domain::resources, errc::out_of_memory}, {}};
  }

  if (!loaded_handle->has_guid()) {
    loaded_handle->set_guid(guid);
  }
  if (loaded_handle->guid() != guid) {
    return {{error_domain::resources, errc::inconsistent_type}, {}};
  }

  if (!target.replace(loaded_handle)) {
    return {{error_domain::resources, errc::backend_failure}, {}};
  }

  return {{}, loaded_handle};
}

/**
 * @brief 使用保存器保存 cache 中的指定资源。
 *
 * 如果 cache 未包含该 GUID，返回 `errc::not_found`。保存目标、格式和覆盖策略由 saver 实现决定。
 */
template <typename T>
[[nodiscard]] error save_from(const cache<T>& source, saver<T>& target, resource_guid guid) {
  if (!guid) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const T* existing = source.find(guid);
  if (existing == nullptr) {
    return {error_domain::resources, errc::not_found};
  }

  return target.save(*existing);
}

} // namespace caors::resources

#endif
