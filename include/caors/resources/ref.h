// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_REF_H_
#define CAORS_RESOURCES_REF_H_

#include "caors/resources/resource.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::resources {

/**
 * @brief 持有宿主资源键的非拥有引用；不规范化键，不读取对象身份。
 * @note Key 必须拥有所引用的数据，已解析指针不延长对象寿命，缓存替换后应重新解析。
 */
template <typename T = resource, typename Key = resource_guid> class ref {
  static_assert(!std::is_same_v<Key, std::string_view>, "Key must own its data");

public:
  using resource_type = T;
  using key_type = Key;
  ref() = default;
  explicit ref(Key key) : key_(std::move(key)) {
    if constexpr (std::is_same_v<Key, resource_guid>) {
      if (!*key_)
        key_.reset();
    }
  }
  /** @brief 获取拥有式键；空引用返回空 optional。 */
  [[nodiscard]] const std::optional<Key>& key() const noexcept { return key_; }
  [[nodiscard]] bool has_key() const noexcept { return key_.has_value(); }
  [[nodiscard]] T* get() const noexcept { return instance_; }
  [[nodiscard]] T& operator*() const noexcept { return *instance_; }
  [[nodiscard]] T* operator->() const noexcept { return instance_; }
  [[nodiscard]] bool resolved() const noexcept { return instance_ != nullptr; }
  [[nodiscard]] resource_generation generation() const noexcept { return generation_; }
  [[nodiscard]] bool has_generation() const noexcept { return generation_ != 0; }
  [[nodiscard]] explicit operator bool() const noexcept { return has_key(); }
  /** @brief 重置并清除身份。 */
  void reset() noexcept {
    key_.reset();
    unresolve();
  }
  /** @brief 先构造新键，再替换引用；构造失败保留旧引用。 */
  void reset(Key key) {
    ref next{std::move(key)};
    swap(next);
  }
  /** @brief 清除非拥有指针，保留资源键。 */
  void unresolve() noexcept {
    instance_ = nullptr;
    generation_ = 0;
    owner_.reset();
  }
  /** @brief 由宿主或缓存绑定指针与代次；不检查对象内嵌身份。 */
  bool resolve(T* instance, resource_generation generation = 0) noexcept {
    instance_ = instance;
    generation_ = instance ? generation : 0;
    owner_.reset();
    return true;
  }
  void swap(ref& other) noexcept(std::is_nothrow_swappable_v<Key>) {
    using std::swap;
    swap(key_, other.key_);
    swap(instance_, other.instance_);
    swap(generation_, other.generation_);
    swap(owner_, other.owner_);
  }

private:
  template <typename, typename, typename, typename> friend class cache;
  std::optional<Key> key_;
  T* instance_ = nullptr;
  resource_generation generation_ = 0;
  std::weak_ptr<const void> owner_;
};

/**
 * @brief 轻量资源引用。
 *
 * `ref<T>` 保存资源 GUID 和一个可选的已解析资源指针。它不拥有资源，也不延长资源生命周期。
 * 加载、缓存、卸载和 GUID 解析由后续 `resources::manager` 或上层系统负责。
 */
template <typename T>
  requires std::is_base_of_v<resource, T>
class ref<T, resource_guid> {
  static_assert(std::is_base_of_v<resource, T>, "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  ref() noexcept = default;
  ref(std::nullptr_t) noexcept {}
  explicit ref(resource_guid guid) noexcept : guid_(guid) {}
  explicit ref(resource_type* instance) noexcept { reset(instance); }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  ref(const ref<U>& other) noexcept
      : guid_(other.guid()), instance_(other.get()), generation_(other.generation()) {}

  /** @brief 返回引用保存的稳定资源 GUID。 */
  [[nodiscard]] resource_guid guid() const noexcept { return guid_; }

  /** @brief 迁移期提供统一键查询；nil GUID 表示没有键。 */
  [[nodiscard]] std::optional<resource_guid> key() const noexcept {
    return guid_ ? std::optional<resource_guid>{guid_} : std::nullopt;
  }

  /** @brief 返回引用解析时记录的资源绑定版本；0 表示没有有效版本。 */
  [[nodiscard]] resource_generation generation() const noexcept { return generation_; }
  [[nodiscard]] bool has_generation() const noexcept {
    return generation_ != invalid_resource_generation;
  }

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
    generation_ = invalid_resource_generation;
  }

  /** @brief 重置为已解析资源引用，GUID 会从资源对象读取。 */
  void reset(resource_type* instance) noexcept {
    instance_ = instance;
    guid_ = instance != nullptr ? instance->guid() : resource_guid::nil();
    generation_ = invalid_resource_generation;
  }

  /** @brief 清除已解析指针，但保留当前 GUID。 */
  void unresolve() noexcept {
    instance_ = nullptr;
    generation_ = invalid_resource_generation;
  }

  /**
   * @brief 尝试把引用解析到资源对象。
   *
   * 如果当前引用已有非 nil GUID，传入资源也有非 nil GUID 且两者不一致，则解析失败并保持原值。
   */
  [[nodiscard]] bool resolve(resource_type* instance) noexcept {
    return resolve(instance, invalid_resource_generation);
  }

  /**
   * @brief 尝试把引用解析到带版本的资源对象。
   *
   * `generation` 通常由 `resources::manager` 或 `resources::cache`
   * 写入，用于后续判断已解析引用是否仍指向当前索引中的资源对象。
   */
  [[nodiscard]] bool resolve(resource_type* instance, resource_generation generation) noexcept {
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
    generation_ = generation;
    return true;
  }

private:
  resource_guid guid_{};
  resource_type* instance_ = nullptr;
  resource_generation generation_ = invalid_resource_generation;
};

} // namespace caors::resources

namespace caors {

template <typename T = resources::resource, typename Key = resource_guid>
using resource_ref = resources::ref<T, Key>;

} // namespace caors

#endif
