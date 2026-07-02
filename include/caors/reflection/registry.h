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

namespace caors {

/**
 * @brief 底层全局反射注册表。
 *
 * 注册表基于 `meta_registry` 保存基础类型信息，并把 `reflection_type_data`
 * 作为元数据组件挂载到同一个 `type_id` 上。普通代码应使用 `reflection::class_<T>()` 和
 * `find_reflection()`，不要创建或管理注册表实例。
 */
class CAORS_API reflection_registry final {
public:
  [[nodiscard]] static reflection_registry& instance() noexcept;

  reflection_registry(const reflection_registry&) = delete;
  reflection_registry& operator=(const reflection_registry&) = delete;

  [[nodiscard]] error register_type(const meta_type_data& type,
                                    const reflection_type_data& reflection);
  [[nodiscard]] error register_type(type_id id, const reflection_type_data& reflection);
  [[nodiscard]] error register_type(meta_type type, const reflection_type_data& reflection);
  template <typename T>
  [[nodiscard]] error register_type_for(const meta_type_data& type,
                                        const reflection_type_data& reflection) {
    return register_type(detail::cpp_type_key_of<T>(), type, reflection);
  }

  [[nodiscard]] bool contains(type_id id) const;
  template <typename T> [[nodiscard]] std::optional<meta_type> find_registered_type() const {
    return find_registered_type(detail::cpp_type_key_of<T>());
  }

  [[nodiscard]] std::optional<reflection_type> find_type(type_id id) const;
  [[nodiscard]] std::optional<reflection_type> find_type(type_guid guid) const;
  [[nodiscard]] std::optional<reflection_type> find_type(std::string_view name) const;
  [[nodiscard]] std::optional<reflection_type> find_type(string_name name) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  reflection_registry();
  ~reflection_registry();

  [[nodiscard]] error register_type(detail::cpp_type_key key, const meta_type_data& type,
                                    const reflection_type_data& reflection);
  [[nodiscard]] std::optional<meta_type> find_registered_type(detail::cpp_type_key key) const;
  [[nodiscard]] const reflection_type_data* find_reflection_data(type_id id) const;
};

/** @brief 获取进程内全局反射注册表。 */
[[nodiscard]] CAORS_API reflection_registry& get_reflection_registry() noexcept;

namespace detail {

/** @brief 向全局反射注册表注册反射类型。内部入口，公开代码优先使用 `reflection::class_<T>()`。 */
template <typename T>
[[nodiscard]] error register_reflection(std::string_view name,
                                        std::initializer_list<reflection_field_data> fields,
                                        type_id parent_id = type_id::invalid()) {
  return get_reflection_registry().register_type_for<T>(make_meta_type_data<T>(name, parent_id),
                                                        make_reflection_type_data(fields));
}

/** @brief 向全局反射注册表注册带稳定 GUID 的反射类型。内部入口。 */
template <typename T>
[[nodiscard]] error register_reflection(std::string_view name, type_guid guid,
                                        std::initializer_list<reflection_field_data> fields,
                                        type_id parent_id = type_id::invalid()) {
  return get_reflection_registry().register_type_for<T>(
      make_meta_type_data<T>(name, guid, parent_id), make_reflection_type_data(fields));
}

/** @brief 向已注册元类型挂载反射组件。内部入口，不会创建或修改基础类型记录。 */
[[nodiscard]] CAORS_API error
register_reflection(type_id id, reflection_type_data data = reflection_type_data{});

/** @brief 向已注册元类型挂载反射组件。内部入口，不会创建或修改基础类型记录。 */
[[nodiscard]] CAORS_API error
register_reflection(meta_type type, reflection_type_data data = reflection_type_data{});

} // namespace detail

/** @brief 在全局反射注册表中查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(type_id id);

/** @brief 在全局反射注册表中按稳定 GUID 查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(type_guid guid);

/** @brief 在全局反射注册表中按类型名查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(std::string_view name);

} // namespace caors

#endif
