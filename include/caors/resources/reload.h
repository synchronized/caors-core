// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_RELOAD_H_
#define CAORS_RESOURCES_RELOAD_H_

#include "caors/events/signal.h"
#include "caors/resources/io.h"

namespace caors::resources {

/**
 * @brief 资源重载通知事件。
 *
 * 事件由 `reload_and_notify()` 在 `reload_into()` 完成后同步发出。成功时 `resource`
 * 指向 cache 中的新资源；失败时 `resource` 为 `nullptr`，cache 会保留旧资源。
 */
template <typename T> struct reload_event {
  resource_guid guid{};
  error status{};
  T* resource = nullptr;

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
};

/** @brief 指定资源类型的同步重载通知信号。 */
template <typename T> using reload_signal = events::signal<void(const reload_event<T>&)>;

/**
 * @brief 重新加载资源并同步广播重载结果。
 *
 * 该函数不会让 `cache<T>` 隐式持有 signal，也不会自动刷新外部引用。调用方可以在监听回调中根据
 * `reload_event<T>` 决定是否重新解析 `resource_ref<T>`、刷新运行期对象或记录诊断信息。
 */
template <typename T>
[[nodiscard]] result<T*> reload_and_notify(cache<T>& target, loader<T>& source, resource_guid guid,
                                           reload_signal<T>& signal) {
  result<T*> reloaded = reload_into(target, source, guid);
  reload_event<T> event{guid, reloaded.status, reloaded.value};
  signal.emit(event);
  return reloaded;
}

} // namespace caors::resources

namespace caors {

template <typename T> using resource_reload_event = resources::reload_event<T>;
template <typename T> using resource_reload_signal = resources::reload_signal<T>;

using resources::reload_and_notify;

} // namespace caors

#endif
