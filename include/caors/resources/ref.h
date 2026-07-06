// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_REF_H_
#define CAORS_RESOURCES_REF_H_

#include "caors/resources/resource.h"

#include <cstddef>
#include <type_traits>

namespace caors::resources {

/**
 * @brief 轻量资源引用。
 *
 * `ref<T>` 保存资源 GUID 和一个可选的已解析资源指针。它不拥有资源，也不延长资源生命周期。
 * 加载、缓存、卸载和 GUID 解析由后续 `resources::manager` 或上层系统负责。
 */
template <typename T = resource> class ref {
  static_assert(std::is_base_of_v<resource, T>, "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  ref() noexcept = default;
  ref(std::nullptr_t) noexcept {}
  explicit ref(resource_guid guid) noexcept : guid_(guid) {}
  explicit ref(resource_type* instance) noexcept { reset(instance); }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  ref(const ref<U>& other) noexcept : guid_(other.guid()), instance_(other.get()) {}

  /** @brief 返回引用保存的稳定资源 GUID。 */
  [[nodiscard]] resource_guid guid() const noexcept { return guid_; }

  /** @brief 判断引用是否保存了非 nil GUID。 */
  [[nodiscard]] bool has_guid() const noexcept { return static_cast<bool>(guid_); }

  /** @brief 返回已解析资源指针；未解析或已清除时返回 `nullptr`。 */
  [[nodiscard]] resource_type* get() const noexcept { return instance_; }
  [[nodiscard]] resource_type& operator*() const noexcept { return *instance_; }
  [[nodiscard]] resource_type* operator->() const noexcept { return instance_; }

  /** @brief 判断当前是否已经解析到资源对象。 */
  [[nodiscard]] bool resolved() const noexcept { return instance_ != nullptr; }

  /** @brief 判断当前是否保存了 GUID 或已解析指针。 */
  [[nodiscard]] explicit operator bool() const noexcept { return has_guid() || resolved(); }

  /** @brief 重置为未解析引用，可保留一个稳定 GUID。 */
  void reset(resource_guid guid = resource_guid::nil()) noexcept {
    guid_ = guid;
    instance_ = nullptr;
  }

  /** @brief 重置为已解析资源引用，GUID 会从资源对象读取。 */
  void reset(resource_type* instance) noexcept {
    instance_ = instance;
    guid_ = instance != nullptr ? instance->guid() : resource_guid::nil();
  }

  /** @brief 清除已解析指针，但保留当前 GUID。 */
  void unresolve() noexcept { instance_ = nullptr; }

  /**
   * @brief 尝试把引用解析到资源对象。
   *
   * 如果当前引用已有非 nil GUID，传入资源也有非 nil GUID 且两者不一致，则解析失败并保持原值。
   */
  [[nodiscard]] bool resolve(resource_type* instance) noexcept {
    if (instance == nullptr) {
      unresolve();
      return true;
    }

    const resource_guid instance_guid = instance->guid();
    if (guid_ && instance_guid && guid_ != instance_guid) {
      return false;
    }

    if (!guid_) {
      guid_ = instance_guid;
    }
    instance_ = instance;
    return true;
  }

private:
  resource_guid guid_{};
  resource_type* instance_ = nullptr;
};

} // namespace caors::resources

namespace caors {

template <typename T = resources::resource> using resource_ref = resources::ref<T>;

} // namespace caors

#endif
