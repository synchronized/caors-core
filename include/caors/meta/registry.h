// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/export.h"
#include "caors/meta/type_info.h"
#include "caors/meta/type_key.h"
#include "caors/meta/view.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace caors {

/** @brief 基础类型注册结果。 */
enum class type_register_result {
  success,
  invalid_type,
  duplicate_id,
  duplicate_name,
  duplicate_guid,
  missing_parent,
};

/** @brief 判断基础类型注册是否成功。 */
[[nodiscard]] constexpr bool type_register_succeeded(type_register_result result) noexcept {
  return result == type_register_result::success;
}

/** @brief 元数据组件注册结果。 */
enum class meta_component_result {
  success,
  invalid_type,
  missing_type,
  duplicate_component,
};

/** @brief 判断元数据组件注册是否成功。 */
[[nodiscard]] constexpr bool meta_component_succeeded(meta_component_result result) noexcept {
  return result == meta_component_result::success;
}

namespace detail {

using meta_component_key = const void*;

template <typename Component> [[nodiscard]] meta_component_key meta_component_key_of() noexcept {
  static const int key = 0;
  return &key;
}

class meta_component_storage_base {
public:
  virtual ~meta_component_storage_base() = default;

  [[nodiscard]] virtual bool contains(type_id id) const = 0;
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;
  [[nodiscard]] virtual std::vector<type_id> ids() const = 0;
};

template <typename Component>
class meta_component_storage final : public meta_component_storage_base {
public:
  static_assert(std::is_copy_constructible_v<Component>,
                "Meta components must be copy constructible");

  template <typename... Args> [[nodiscard]] bool emplace(type_id id, Args&&... args) {
    std::lock_guard lock(mutex_);
    return components_.try_emplace(id.value(), std::forward<Args>(args)...).second;
  }

  [[nodiscard]] std::optional<Component> find(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return std::nullopt;
    }

    return position->second;
  }

  [[nodiscard]] bool contains(type_id id) const override {
    std::lock_guard lock(mutex_);
    return components_.contains(id.value());
  }

  [[nodiscard]] std::size_t size() const noexcept override {
    std::lock_guard lock(mutex_);
    return components_.size();
  }

  [[nodiscard]] std::vector<type_id> ids() const override {
    std::lock_guard lock(mutex_);
    std::vector<type_id> result;
    result.reserve(components_.size());
    for (const auto& [id, component] : components_) {
      (void)component;
      result.emplace_back(id);
    }

    return result;
  }

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::uint64_t, Component> components_;
};

} // namespace detail

class meta_registry_state;

/**
 * @brief 底层全局类型元数据注册表。
 *
 * `meta_registry` 以 `type_id` 为 key，先注册基础 `type_info`，再按类型挂载对象工厂、
 * 反射描述、序列化描述等元数据组件。普通代码应使用全局 `register_type()` 和
 * `find_type()` 入口，不要创建或管理注册表实例。
 */
class CAORS_API meta_registry final {
public:
  [[nodiscard]] static meta_registry& instance() noexcept;

  meta_registry(const meta_registry&) = delete;
  meta_registry& operator=(const meta_registry&) = delete;

  [[nodiscard]] type_register_result register_type(const type_info& type);
  [[nodiscard]] type_register_result register_cpp_type(detail::cpp_type_key key,
                                                       const type_info& type);
  template <typename T>
  [[nodiscard]] type_register_result register_type_for(const type_info& type) {
    return register_cpp_type(detail::cpp_type_key_of<T>(), type);
  }

  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<type_info> find_cpp_type(detail::cpp_type_key key) const;
  template <typename T> [[nodiscard]] std::optional<type_info> find_type() const {
    return find_cpp_type(detail::cpp_type_key_of<T>());
  }

  [[nodiscard]] std::optional<type_info> find_type(type_id id) const;
  [[nodiscard]] std::optional<type_info> find_type(type_guid guid) const;
  [[nodiscard]] std::optional<type_info> find_type(std::string_view name) const;
  [[nodiscard]] std::optional<type_info> find_type(string_name name) const;
  [[nodiscard]] bool is_a(type_id id, type_id base_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  /** @brief 给已注册类型挂载一个元数据组件。 */
  template <typename Component, typename... Args>
  [[nodiscard]] meta_component_result emplace(type_id id, Args&&... args) {
    using component_type = std::decay_t<Component>;

    if (!id) {
      return meta_component_result::invalid_type;
    }

    if (!contains(id)) {
      return meta_component_result::missing_type;
    }

    detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      storage_base = &ensure_component_storage(
          detail::meta_component_key_of<component_type>(),
          std::make_unique<detail::meta_component_storage<component_type>>());
    }

    auto& storage = static_cast<detail::meta_component_storage<component_type>&>(*storage_base);
    if (!storage.emplace(id, std::forward<Args>(args)...)) {
      return meta_component_result::duplicate_component;
    }

    return meta_component_result::success;
  }

  /** @brief 判断指定类型是否挂载了某个元数据组件。 */
  template <typename Component> [[nodiscard]] bool has(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return false;
    }

    return storage_base->contains(id);
  }

  /** @brief 获取指定类型上的元数据组件副本。 */
  template <typename Component> [[nodiscard]] std::optional<Component> find(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return std::nullopt;
    }

    const auto& storage =
        static_cast<const detail::meta_component_storage<component_type>&>(*storage_base);
    return storage.find(id);
  }

  /** @brief 统计挂载了指定组件的类型数量。 */
  template <typename Component> [[nodiscard]] std::size_t count() const noexcept {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return 0;
    }

    return storage_base->size();
  }

  /** @brief 查询同时拥有指定组件集合的类型。 */
  template <typename... Components> [[nodiscard]] meta_view<Components...> view() const;

private:
  meta_registry();
  ~meta_registry();

  [[nodiscard]] detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key);
  [[nodiscard]] const detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key) const;
  detail::meta_component_storage_base&
  ensure_component_storage(detail::meta_component_key key,
                           std::unique_ptr<detail::meta_component_storage_base> storage);
  [[nodiscard]] std::vector<type_id> view_component_ids(const detail::meta_component_key* keys,
                                                        std::size_t count) const;

  meta_registry_state* state_ = nullptr;
};

/** @brief 获取进程内全局类型元数据注册表。 */
[[nodiscard]] CAORS_API meta_registry& get_meta_registry() noexcept;

/** @brief 向全局元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(string_name name,
                                                 type_id parent_id = type_id::invalid()) {
  return get_meta_registry().register_type_for<T>(make_type_info<T>(name, parent_id));
}

/** @brief 向全局元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(string_name name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return get_meta_registry().register_type_for<T>(make_type_info<T>(name, guid, parent_id));
}

/** @brief 向全局元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(std::string_view name,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(string_name{name}, parent_id);
}

/** @brief 向全局元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(std::string_view name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(string_name{name}, guid, parent_id);
}

/** @brief 在全局元数据注册表中按 C++ 类型查找基础类型。 */
template <typename T> [[nodiscard]] std::optional<type_info> find_type() {
  return get_meta_registry().find_type<T>();
}

/** @brief 在全局元数据注册表中按类型 id 查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_info> find_type(type_id id);

/** @brief 在全局元数据注册表中按稳定 GUID 查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_info> find_type(type_guid guid);

/** @brief 在全局元数据注册表中按类型名查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_info> find_type(std::string_view name);

/** @brief 在全局元数据注册表中按类型名查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_info> find_type(string_name name);

/** @brief 在全局元数据注册表中判断类型继承关系。 */
[[nodiscard]] CAORS_API bool is_a(type_id id, type_id base_id);

template <typename... Components>
template <typename Component>
[[nodiscard]] std::optional<Component> meta_view<Components...>::find(type_id id) const {
  if (registry_ == nullptr) {
    return std::nullopt;
  }

  return registry_->template find<Component>(id);
}

template <typename... Components>
[[nodiscard]] meta_view<Components...> meta_registry::view() const {
  static_assert(sizeof...(Components) > 0, "Meta view requires at least one component type");

  detail::meta_component_key keys[] = {
      detail::meta_component_key_of<std::decay_t<Components>>()...};
  return meta_view<Components...>{this, view_component_ids(keys, sizeof...(Components))};
}

} // namespace caors

#endif
