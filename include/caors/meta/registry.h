// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/adapters/stl_wrapper.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/component_storage.h"
#include "caors/meta/detail/component_storage.h"
#include "caors/meta/detail/container.h"
#include "caors/meta/registration.h"
#include "caors/meta/type.h"
#include "caors/meta/view.h"
#include "caors/traits/container.h"

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

class registry_state;

/**
 * @brief 底层全局类型元数据注册表。
 *
 * `meta::registry` 以 `type_id` 为 key，先注册内部基础类型数据，再按类型挂载对象工厂、
 * 反射描述、序列化描述等元数据组件。普通代码应使用 `meta::type<T>()`、`meta::get<T>()`
 * 和 `meta::find()` 入口，不要创建或管理注册表实例。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] error register_type(const detail::type_data& type);
  [[nodiscard]] error add_registration(registration_fn fn);
  [[nodiscard]] error run_registrations();
  [[nodiscard]] error freeze();
  [[nodiscard]] bool is_frozen() const noexcept;

  template <typename T> [[nodiscard]] error register_type_for(const detail::type_data& type) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    const type_id expected_id = detail::type_id_of_cpp_type<value_type>();
    if (!type.valid() || (type.id() && type.id() != expected_id)) {
      return {error_domain::meta, errc::invalid_type};
    }

    detail::type_data stored_type = type;
    if (!stored_type.id()) {
      stored_type.set_id(expected_id);
    }

    const error status = register_type(stored_type);
    if (status && status.value() != errc::duplicate_id) {
      return status;
    }

    const type_view registered = find_type(expected_id);
    if (registered) {
      (void)detail::ensure_type_semantics_for<value_type>(registered.id());
      (void)detail::register_container_data_for<value_type>(registered.id());
    }

    return status;
  }

  [[nodiscard]] bool contains(type_id id) const;
  template <typename T> [[nodiscard]] type_view find_type() const {
    return find_type(detail::type_id_of_cpp_type<T>());
  }

  [[nodiscard]] type_view find_type(type_id id) const;
  [[nodiscard]] type_view find_type(type_guid guid) const;
  [[nodiscard]] type_view find_type(std::string_view name) const;
  [[nodiscard]] type_view find_type(string_name name) const;
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

} // namespace caors::meta

#include "caors/meta/query.h"

#endif
