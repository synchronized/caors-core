// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CORE_LOCATOR_H_
#define CAORS_CORE_LOCATOR_H_

#include <functional>
#include <type_traits>
#include <utility>

namespace caors::core {

/**
 * @brief 进程内服务定位器。
 *
 * `locator<T>` 只保存一个非拥有的 `T*`，用于定位默认资源管理器、文件系统、配置服务等进程级服务。
 * 它不负责创建或销毁服务对象；服务对象的生命周期必须由调用方保证。
 *
 * 线程安全：`get()` 读取当前指针本身不加锁，`set()` / `reset()`
 * 也不做同步。推荐只在初始化阶段或测试
 * 隔离代码中修改定位器；运行期多线程切换服务时应由上层自行同步。
 */
template <typename Service> class locator final {
public:
  using service_type = Service;

  locator() = delete;

  /** @brief 返回当前服务指针；未设置时返回 `nullptr`。 */
  [[nodiscard]] static service_type* get() noexcept { return slot(); }

  /** @brief 判断当前是否已经设置服务。 */
  [[nodiscard]] static bool has_value() noexcept { return get() != nullptr; }

  /**
   * @brief 设置当前服务指针。
   *
   * @return 返回之前保存的服务指针，便于调用方自行恢复。
   */
  static service_type* set(service_type* service) noexcept {
    service_type* previous = slot();
    slot() = service;
    return previous;
  }

  /**
   * @brief 清空当前服务指针。
   *
   * @return 返回清空前保存的服务指针。
   */
  static service_type* reset() noexcept { return set(nullptr); }

  /**
   * @brief 在回调执行期间临时设置服务，回调结束后恢复原值。
   *
   * 回调不接收参数；如需访问服务，可在回调内部调用 `locator<T>::get()`。这种形式允许临时设置为
   * `nullptr`，也避免定位器约束服务对象的传参形式。
   */
  template <typename Callback>
  static decltype(auto) with(service_type* service, Callback&& callback) {
    struct restore_guard {
      explicit restore_guard(service_type* previous_service) noexcept
          : previous(previous_service) {}
      restore_guard(const restore_guard&) = delete;
      restore_guard& operator=(const restore_guard&) = delete;
      ~restore_guard() noexcept { locator::set(previous); }

      service_type* previous;
    };

    restore_guard guard{set(service)};
    if constexpr (std::is_void_v<std::invoke_result_t<Callback>>) {
      std::invoke(std::forward<Callback>(callback));
    } else {
      return std::invoke(std::forward<Callback>(callback));
    }
  }

private:
  [[nodiscard]] static service_type*& slot() noexcept {
    static service_type* service = nullptr;
    return service;
  }
};

} // namespace caors::core

namespace caors {

template <typename Service> using locator = core::locator<Service>;

} // namespace caors

#endif
