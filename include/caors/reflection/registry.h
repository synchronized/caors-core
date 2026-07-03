// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_REGISTRY_H_
#define CAORS_REFLECTION_REGISTRY_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/reflection/view.h"

#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection {

/**
 * @brief 底层全局反射注册表。
 *
 * 注册表基于 `meta::registry` 保存基础类型信息，并把 `reflection::type_data`
 * 作为元数据组件挂载到同一个 `type_id` 上。普通代码应使用 `reflection::class_<T>()` 和
 * `reflection::find()`，不要创建或管理注册表实例。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] error register_type(const meta::type_data& type, const type_data& reflection);
  [[nodiscard]] error register_type(type_id id, const type_data& reflection);
  [[nodiscard]] error register_type(meta::type_view type, const type_data& reflection);
  template <typename T>
  [[nodiscard]] error register_type_for(const meta::type_data& type, const type_data& reflection) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    const type_id expected_id = meta::detail::type_id_of_cpp_type<value_type>();
    if (type.id() && type.id() != expected_id) {
      return {error_domain::reflection, errc::invalid_type};
    }

    meta::type_data stored_type = type;
    if (!stored_type.id()) {
      stored_type.set_id(expected_id);
    }

    return register_type(stored_type, reflection);
  }

  [[nodiscard]] bool contains(type_id id) const;
  template <typename T> [[nodiscard]] std::optional<meta::type_view> find_registered_type() const {
    return meta::get_registry().find_type<T>();
  }

  [[nodiscard]] std::optional<type> find_type(type_id id) const;
  [[nodiscard]] std::optional<type> find_type(type_guid guid) const;
  [[nodiscard]] std::optional<type> find_type(std::string_view name) const;
  [[nodiscard]] std::optional<type> find_type(string_name name) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  registry();
  ~registry();

  [[nodiscard]] const type_data* find_reflection_data(type_id id) const;
};

/** @brief 获取进程内全局反射注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

/** @brief 在全局反射注册表中查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<type> find(type_id id);

/** @brief 在全局反射注册表中按稳定 GUID 查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<type> find(type_guid guid);

/** @brief 在全局反射注册表中按类型名查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<type> find(std::string_view name);

/** @brief 判断指定类型是否挂载了反射组件。 */
[[nodiscard]] CAORS_API bool contains(type_id id);

/** @brief 获取当前反射组件数量。 */
[[nodiscard]] CAORS_API std::size_t size() noexcept;

/** @brief 向全局反射注册表注册指定 C++ 类型。 */
template <typename T>
[[nodiscard]] error register_type(std::string_view name,
                                  std::initializer_list<reflection::field_data> fields,
                                  type_id parent_id = type_id::invalid()) {
  return reflection::get_registry().register_type_for<T>(meta::make_type_data<T>(name, parent_id),
                                                         reflection::make_type_data(fields));
}

/** @brief 向全局反射注册表注册带稳定 GUID 的指定 C++ 类型。 */
template <typename T>
[[nodiscard]] error register_type(std::string_view name, type_guid guid,
                                  std::initializer_list<reflection::field_data> fields,
                                  type_id parent_id = type_id::invalid()) {
  return reflection::get_registry().register_type_for<T>(
      meta::make_type_data<T>(name, guid, parent_id), reflection::make_type_data(fields));
}

/** @brief 向已注册元类型挂载反射组件，不会创建或修改基础类型记录。 */
[[nodiscard]] CAORS_API error register_type(type_id id,
                                            reflection::type_data data = reflection::type_data{});

/** @brief 向已注册元类型挂载反射组件，不会创建或修改基础类型记录。 */
[[nodiscard]] CAORS_API error register_type(meta::type_view type,
                                            reflection::type_data data = reflection::type_data{});

namespace detail {

/** @brief 向全局反射注册表注册反射类型。内部入口，公开代码优先使用 `reflection::class_<T>()`。 */
template <typename T>
[[nodiscard]] error register_reflection(std::string_view name,
                                        std::initializer_list<reflection::field_data> fields,
                                        type_id parent_id = type_id::invalid()) {
  return reflection::get_registry().register_type_for<T>(meta::make_type_data<T>(name, parent_id),
                                                         reflection::make_type_data(fields));
}

/** @brief 向全局反射注册表注册带稳定 GUID 的反射类型。内部入口。 */
template <typename T>
[[nodiscard]] error register_reflection(std::string_view name, type_guid guid,
                                        std::initializer_list<reflection::field_data> fields,
                                        type_id parent_id = type_id::invalid()) {
  return reflection::get_registry().register_type_for<T>(
      meta::make_type_data<T>(name, guid, parent_id), reflection::make_type_data(fields));
}

/** @brief 向已注册元类型挂载反射组件。内部入口，不会创建或修改基础类型记录。 */
[[nodiscard]] CAORS_API error
register_reflection(type_id id, reflection::type_data data = reflection::type_data{});

/** @brief 向已注册元类型挂载反射组件。内部入口，不会创建或修改基础类型记录。 */
[[nodiscard]] CAORS_API error
register_reflection(meta::type_view type, reflection::type_data data = reflection::type_data{});

} // namespace detail

} // namespace caors::reflection

#endif
