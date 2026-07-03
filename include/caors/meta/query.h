// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_QUERY_H_
#define CAORS_META_QUERY_H_

#include "caors/meta/registry.h"

// clang-format off
#include "caors/meta/type_semantics.h"
#include "caors/meta/container_registration.h"
// clang-format on

#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::meta {

/** @brief RTTR 风格的元类型注册构建器。 */
template <typename T> class type_builder {
public:
  type_builder() = default;

  type_builder& type(std::string_view name) {
    status_ = get_registry().register_type_for<T>(detail::make_type_data<T>(name));
    refresh();
    return *this;
  }

  type_builder& guid(type_guid guid_value) {
    if (!type_) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    detail::type_data data{
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

    detail::type_data data{
        type_.id(), type_.name(), type_.guid(), type_.size(), type_.alignment(), parent_id,
    };
    status_ = get_registry().register_type_for<T>(data);
    refresh();
    return *this;
  }

  [[nodiscard]] ::caors::meta::type get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

private:
  void refresh() {
    ::caors::meta::type refreshed = get_registry().find_type<T>();
    if (refreshed) {
      type_ = refreshed;
    }
  }

  ::caors::meta::type type_{};
  error status_{};
};

/** @brief 获取指定 C++ 类型的元类型注册构建器；默认不注册类型。 */
template <typename T> [[nodiscard]] type_builder<T> type_() { return type_builder<T>{}; }

/** @brief 显式注册指定 C++ 类型并设置类型名。 */
template <typename T> [[nodiscard]] type_builder<T> type_(std::string_view name) {
  return type_builder<T>{}.type(name);
}

/** @brief 按 C++ 类型查找基础类型。 */
template <typename T> [[nodiscard]] type find() { return get_registry().find_type<T>(); }

/** @brief 按类型 id 查找基础类型。 */
[[nodiscard]] CAORS_API type find(type_id id);

/** @brief 按稳定 GUID 查找基础类型。 */
[[nodiscard]] CAORS_API type find(type_guid guid);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API type find(std::string_view name);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API type find(string_name name);

/** @brief 获取已注册 C++ 类型的运行期类型 id；未注册时返回无效 id。 */
template <typename T> [[nodiscard]] type_id id_of() {
  const type stored = find<T>();
  return stored ? stored.id() : type_id::invalid();
}

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(string_name name) noexcept;

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(std::string_view name);

/** @brief 在全局元数据注册表中判断类型继承关系。 */
[[nodiscard]] CAORS_API bool is_a(type_id id, type_id base_id);

namespace detail {

/** @brief 向全局元数据注册表注册基础类型。内部入口，公开代码优先使用 `meta::type_<T>()`。 */
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
