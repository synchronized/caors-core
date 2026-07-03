// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/container.h"
#include "caors/meta/container_traits.h"
#include "caors/meta/type.h"
#include "caors/meta/type_key.h"
#include "caors/meta/view.h"
#include "caors/meta/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename T> [[nodiscard]] error ensure_type_semantics_for(type_id id);
template <typename T> [[nodiscard]] error register_container_data_for(type_id id);

} // namespace caors::meta::detail

namespace caors::meta {

/** @brief 指定类型上的元数据组件挂载状态。 */
enum class component_status {
  invalid_type,
  missing_type,
  missing_component,
  present,
};

/** @brief 延迟到 `meta::freeze_registry()` 前执行的元数据注册函数。 */
using registration_fn = error (*)();

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
  [[nodiscard]] virtual vector<type_id> ids() const = 0;
};

template <typename Component>
class meta_component_storage final : public meta_component_storage_base {
public:
  static_assert(std::is_copy_constructible_v<Component>,
                "Meta components must be copy constructible");

  template <typename... Args> [[nodiscard]] bool emplace(type_id id, Args&&... args) {
    std::lock_guard lock(mutex_);
    auto component = std::make_unique<Component>(std::forward<Args>(args)...);
    return components_.try_emplace(id.value(), std::move(component)).second;
  }

  [[nodiscard]] std::optional<Component> find(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return std::nullopt;
    }

    return *position->second;
  }

  [[nodiscard]] const Component* find_ptr(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return nullptr;
    }

    return position->second.get();
  }

  [[nodiscard]] bool contains(type_id id) const override {
    std::lock_guard lock(mutex_);
    return components_.contains(id.value());
  }

  [[nodiscard]] std::size_t size() const noexcept override {
    std::lock_guard lock(mutex_);
    return components_.size();
  }

  [[nodiscard]] vector<type_id> ids() const override {
    std::lock_guard lock(mutex_);
    vector<type_id> result;
    result.reserve(components_.size());
    for (const auto& [id, component] : components_) {
      (void)component;
      result.emplace_back(id);
    }

    return result;
  }

private:
  mutable std::mutex mutex_;
  hash_map<std::uint64_t, std::unique_ptr<Component>> components_;
};

} // namespace detail

class registry_state;

/**
 * @brief 底层全局类型元数据注册表。
 *
 * `meta::registry` 以 `type_id` 为 key，先注册基础 `meta::type_data`，再按类型挂载对象工厂、
 * 反射描述、序列化描述等元数据组件。普通代码应使用 `meta::type<T>()`、`meta::get<T>()`
 * 和 `meta::find()` 入口，不要创建或管理注册表实例。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] error register_type(const type_data& type);
  [[nodiscard]] error register_cpp_type(caors::detail::cpp_type_key key, const type_data& type);
  [[nodiscard]] error add_registration(registration_fn fn);
  [[nodiscard]] error run_registrations();
  [[nodiscard]] error freeze();
  [[nodiscard]] bool is_frozen() const noexcept;

  template <typename T> [[nodiscard]] error register_type_for(const type_data& type) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    const caors::detail::cpp_type_key key = caors::detail::cpp_type_key_of<value_type>();
    const error status = register_cpp_type(key, type);
    if (status && status.value() != errc::duplicate_id) {
      return status;
    }

    const std::optional<type_view> registered = find_cpp_type(key);
    if (registered.has_value()) {
      (void)detail::ensure_type_semantics_for<value_type>(registered->id());
      (void)detail::register_container_data_for<value_type>(registered->id());
    }

    return status;
  }

  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<type_view> find_cpp_type(caors::detail::cpp_type_key key) const;
  template <typename T> [[nodiscard]] std::optional<type_view> find_type() const {
    return find_cpp_type(caors::detail::cpp_type_key_of<T>());
  }

  [[nodiscard]] std::optional<type_view> find_type(type_id id) const;
  [[nodiscard]] std::optional<type_view> find_type(type_guid guid) const;
  [[nodiscard]] std::optional<type_view> find_type(std::string_view name) const;
  [[nodiscard]] std::optional<type_view> find_type(string_name name) const;
  [[nodiscard]] bool is_a(type_id id, type_id base_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  /** @brief 给已注册类型挂载一个元数据组件。 */
  template <typename Component, typename... Args>
  [[nodiscard]] error emplace(type_id id, Args&&... args) {
    using component_type = std::decay_t<Component>;

    if (is_frozen()) {
      return {error_domain::meta, errc::frozen};
    }

    if (!id) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!contains(id)) {
      return {error_domain::meta, errc::missing_type};
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
      return {error_domain::meta, errc::duplicate_component};
    }

    return {};
  }

  /**
   * @brief 确保指定类型拥有组件。
   *
   * 如果组件已经存在，直接返回现有组件指针；如果不存在，则用传入参数创建。返回指针不拥有组件，
   * 生命周期由全局注册表管理。
   */
  template <typename Component, typename... Args>
  [[nodiscard]] const Component* ensure_component(type_id id, Args&&... args) {
    if (const Component* existing = find_ptr<Component>(id); existing != nullptr) {
      return existing;
    }

    const error status = emplace<Component>(id, std::forward<Args>(args)...);
    if (status && status.value() != errc::duplicate_component) {
      return nullptr;
    }

    return find_ptr<Component>(id);
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

  /**
   * @brief 获取指定类型上的元数据组件只读指针。
   *
   * 组件由全局注册表以 append-only 方式保存。返回指针不拥有对象，只要组件已注册且进程未退出，
   * 指针地址保持稳定。
   */
  template <typename Component> [[nodiscard]] const Component* find_ptr(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return nullptr;
    }

    const auto& storage =
        static_cast<const detail::meta_component_storage<component_type>&>(*storage_base);
    return storage.find_ptr(id);
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

  /** @brief 查询指定类型上的组件状态，用于重复注册和缺失组件诊断。 */
  template <typename Component> [[nodiscard]] component_status component_status(type_id id) const {
    using component_type = std::decay_t<Component>;
    if (!id) {
      return component_status::invalid_type;
    }

    if (!contains(id)) {
      return component_status::missing_type;
    }

    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr || !storage_base->contains(id)) {
      return component_status::missing_component;
    }

    return component_status::present;
  }

  /** @brief 查询同时拥有指定组件集合的类型。 */
  template <typename... Components> [[nodiscard]] view<Components...> view() const;

private:
  registry();
  ~registry();

  [[nodiscard]] detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key);
  [[nodiscard]] const detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key) const;
  detail::meta_component_storage_base&
  ensure_component_storage(detail::meta_component_key key,
                           std::unique_ptr<detail::meta_component_storage_base> storage);
  [[nodiscard]] vector<type_id> view_component_ids(const detail::meta_component_key* keys,
                                                   std::size_t count) const;

  registry_state* state_ = nullptr;
};

/** @brief 获取进程内全局类型元数据注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

namespace detail {

/** @brief 注册一个延迟执行的元数据注册函数。内部入口，优先使用 `CAORS_META_REGISTRATION`。 */
[[nodiscard]] CAORS_API error register_registration(registration_fn fn);

} // namespace detail

/** @brief 执行所有已收集的元数据注册函数；最多执行一次。 */
[[nodiscard]] CAORS_API error run_registrations();

/** @brief 冻结全局元数据注册表；冻结后只允许只读查询。 */
[[nodiscard]] CAORS_API error freeze_registry();

/** @brief 查询全局元数据注册表是否已经冻结。 */
[[nodiscard]] CAORS_API bool is_registry_frozen() noexcept;

} // namespace caors::meta

/**
 * @brief 定义一个延迟到 `meta::freeze_registry()` 前执行的元数据注册函数。
 *
 * `name` 必须是当前翻译单元内唯一的普通标识符。函数体需要返回 `caors::error`。
 */
#define CAORS_META_REGISTRATION(name)                                                              \
  static ::caors::error name();                                                                    \
  namespace {                                                                                      \
  struct name##_caors_meta_registration_auto_register {                                            \
    name##_caors_meta_registration_auto_register() {                                               \
      (void)::caors::meta::detail::register_registration(&name);                                   \
    }                                                                                              \
  };                                                                                               \
  static const name##_caors_meta_registration_auto_register                                        \
      name##_caors_meta_registration_auto_register_instance{};                                     \
  }                                                                                                \
  static ::caors::error name()

// clang-format off
#include "caors/meta/type_semantics.h"
#include "caors/meta/container_registration.h"
// clang-format on

namespace caors::meta {

/** @brief RTTR 风格的元类型注册构建器。 */
template <typename T> class type_builder {
public:
  type_builder() = default;

  type_builder& type(std::string_view name) {
    status_ = get_registry().register_type_for<T>(make_type_data<T>(name));
    refresh();
    return *this;
  }

  type_builder& guid(type_guid guid_value) {
    if (!type_) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    type_data data{
        type_.id(), type_.name(), guid_value, type_.size(), type_.alignment(), type_.parent_id(),
    };
    status_ = get_registry().register_type_for<T>(data);
    refresh();
    return *this;
  }

  type_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  type_builder& parent(type_id parent_id) {
    if (!type_) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    type_data data{
        type_.id(), type_.name(), type_.guid(), type_.size(), type_.alignment(), parent_id,
    };
    status_ = get_registry().register_type_for<T>(data);
    refresh();
    return *this;
  }

  [[nodiscard]] type_view get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

  [[nodiscard]] operator type_view() const noexcept { return type_; }

private:
  void refresh() {
    std::optional<type_view> refreshed = get_registry().find_type<T>();
    if (refreshed.has_value()) {
      type_ = *refreshed;
    }
  }

  type_view type_{};
  error status_{};
};

/** @brief 获取指定 C++ 类型的元类型注册构建器；默认不注册类型。 */
template <typename T> [[nodiscard]] type_builder<T> type() { return type_builder<T>{}; }

/** @brief 显式注册指定 C++ 类型并设置类型名。 */
template <typename T> [[nodiscard]] type_builder<T> type(std::string_view name) {
  return type_builder<T>{}.type(name);
}

/** @brief 获取已注册的 C++ 类型；不存在时返回无效 `type_view`。 */
template <typename T> [[nodiscard]] type_view get() {
  const std::optional<type_view> stored = get_registry().find_type<T>();
  return stored.value_or(type_view{});
}

/** @brief 按 C++ 类型查找基础类型。 */
template <typename T> [[nodiscard]] std::optional<type_view> find() {
  return get_registry().find_type<T>();
}

/** @brief 按类型 id 查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_view> find(type_id id);

/** @brief 按稳定 GUID 查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_view> find(type_guid guid);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_view> find(std::string_view name);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<type_view> find(string_name name);

/** @brief 获取已注册 C++ 类型的运行期类型 id；未注册时返回无效 id。 */
template <typename T> [[nodiscard]] type_id id_of() {
  const std::optional<type_view> stored = find<T>();
  return stored.has_value() ? stored->id() : type_id::invalid();
}

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(string_name name) noexcept;

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(std::string_view name);

/** @brief 在全局元数据注册表中判断类型继承关系。 */
[[nodiscard]] CAORS_API bool is_a(type_id id, type_id base_id);

namespace detail {

/** @brief 向全局元数据注册表注册基础类型。内部入口，公开代码优先使用 `meta::type<T>()`。 */
template <typename T>
[[nodiscard]] error register_type(string_name name, type_id parent_id = type_id::invalid()) {
  return get_registry().register_type_for<T>(make_type_data<T>(name, parent_id));
}

/** @brief 向全局元数据注册表注册带稳定 GUID 的基础类型。内部入口。 */
template <typename T>
[[nodiscard]] error register_type(string_name name, type_guid guid,
                                  type_id parent_id = type_id::invalid()) {
  return get_registry().register_type_for<T>(make_type_data<T>(name, guid, parent_id));
}

/** @brief 向全局元数据注册表注册基础类型。内部入口。 */
template <typename T>
[[nodiscard]] error register_type(std::string_view name, type_id parent_id = type_id::invalid()) {
  return register_type<T>(string_name{name}, parent_id);
}

/** @brief 向全局元数据注册表注册带稳定 GUID 的基础类型。内部入口。 */
template <typename T>
[[nodiscard]] error register_type(std::string_view name, type_guid guid,
                                  type_id parent_id = type_id::invalid()) {
  return register_type<T>(string_name{name}, guid, parent_id);
}

} // namespace detail

template <typename... Components>
template <typename Component>
[[nodiscard]] std::optional<Component> view<Components...>::find(type_id id) const {
  if (registry_ == nullptr) {
    return std::nullopt;
  }

  return registry_->template find<Component>(id);
}

template <typename... Components> [[nodiscard]] view<Components...> registry::view() const {
  static_assert(sizeof...(Components) > 0, "Meta view requires at least one component type");

  detail::meta_component_key keys[] = {
      detail::meta_component_key_of<std::decay_t<Components>>()...};
  return ::caors::meta::view<Components...>{this, view_component_ids(keys, sizeof...(Components))};
}

} // namespace caors::meta

#endif
