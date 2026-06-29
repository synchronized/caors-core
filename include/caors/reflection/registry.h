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

/** @brief 反射类型注册结果。 */
enum class reflection_register_result {
  success,
  invalid_type,
  invalid_field,
  duplicate_id,
  duplicate_name,
  duplicate_guid,
  duplicate_field,
  duplicate_reflection,
  inconsistent_type,
  missing_parent,
};

/** @brief 判断反射类型注册是否成功。 */
[[nodiscard]] constexpr bool
reflection_register_succeeded(reflection_register_result result) noexcept {
  return result == reflection_register_result::success;
}

/** @brief 将反射类型注册结果转换为统一错误状态。 */
[[nodiscard]] constexpr error to_error(reflection_register_result result) noexcept {
  switch (result) {
  case reflection_register_result::success:
    return {};
  case reflection_register_result::invalid_type:
    return {error_domain::reflection, errc::invalid_type};
  case reflection_register_result::invalid_field:
    return {error_domain::reflection, errc::invalid_field};
  case reflection_register_result::duplicate_id:
    return {error_domain::reflection, errc::duplicate_id};
  case reflection_register_result::duplicate_name:
    return {error_domain::reflection, errc::duplicate_name};
  case reflection_register_result::duplicate_guid:
    return {error_domain::reflection, errc::duplicate_guid};
  case reflection_register_result::duplicate_field:
    return {error_domain::reflection, errc::duplicate_field};
  case reflection_register_result::duplicate_reflection:
    return {error_domain::reflection, errc::duplicate_reflection};
  case reflection_register_result::inconsistent_type:
    return {error_domain::reflection, errc::inconsistent_type};
  case reflection_register_result::missing_parent:
    return {error_domain::reflection, errc::missing_parent};
  }

  return {error_domain::reflection, errc::invalid_argument};
}

/**
 * @brief 底层全局反射注册表。
 *
 * 注册表基于 `meta_registry` 保存基础类型信息，并把 `reflection_type_data`
 * 作为元数据组件挂载到同一个 `type_id` 上。普通代码应使用
 * `register_reflection()`、`reflection::class_<T>()` 和
 * `find_reflection()`，不要创建或管理注册表实例。
 */
class CAORS_API reflection_registry final {
public:
  [[nodiscard]] static reflection_registry& instance() noexcept;

  reflection_registry(const reflection_registry&) = delete;
  reflection_registry& operator=(const reflection_registry&) = delete;

  [[nodiscard]] reflection_register_result register_type(const meta_type_data& type,
                                                         const reflection_type_data& reflection);
  template <typename T>
  [[nodiscard]] reflection_register_result
  register_type_for(const meta_type_data& type, const reflection_type_data& reflection) {
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

  [[nodiscard]] reflection_register_result register_type(detail::cpp_type_key key,
                                                         const meta_type_data& type,
                                                         const reflection_type_data& reflection);
  [[nodiscard]] std::optional<meta_type> find_registered_type(detail::cpp_type_key key) const;
  [[nodiscard]] const reflection_type_data* find_reflection_data(type_id id) const;
};

/** @brief 获取进程内全局反射注册表。 */
[[nodiscard]] CAORS_API reflection_registry& get_reflection_registry() noexcept;

/** @brief 向全局反射注册表注册反射类型。 */
template <typename T>
[[nodiscard]] reflection_register_result
register_reflection(std::string_view name, std::initializer_list<field_info> fields,
                    type_id parent_id = type_id::invalid()) {
  return get_reflection_registry().register_type_for<T>(make_meta_type_data<T>(name, parent_id),
                                                        make_reflection_type_data(fields));
}

/** @brief 向全局反射注册表注册带稳定 GUID 的反射类型。 */
template <typename T>
[[nodiscard]] reflection_register_result
register_reflection(std::string_view name, type_guid guid, std::initializer_list<field_info> fields,
                    type_id parent_id = type_id::invalid()) {
  return get_reflection_registry().register_type_for<T>(
      make_meta_type_data<T>(name, guid, parent_id), make_reflection_type_data(fields));
}

/** @brief 在全局反射注册表中查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(type_id id);

/** @brief 在全局反射注册表中按稳定 GUID 查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(type_guid guid);

/** @brief 在全局反射注册表中按类型名查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(std::string_view name);

} // namespace caors

#endif
