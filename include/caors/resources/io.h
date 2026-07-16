// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_IO_H_
#define CAORS_RESOURCES_IO_H_

#include "caors/resources/cache.h"
#include "caors/result.h"

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
