// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_REGISTRY_H_
#define CAORS_REFLECTION_REGISTRY_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta.h"
#include "caors/reflection/type.h"

#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection {

template <typename Class> class class_builder;

/**
 * @brief 底层全局反射注册表。
 *
 * 注册表基于 `meta::registry` 保存基础类型信息，并把内部反射数据
 * 作为元数据组件挂载到同一个 `type_id` 上。普通代码应使用 `reflection::class_<T>()` 和
 * `reflection::find()`，不要创建或管理注册表实例。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] bool contains(type_id id) const;
  template <typename T> [[nodiscard]] meta::type find_registered_type() const {
    return meta::get_registry().find_type<T>();
  }

  [[nodiscard]] type find_type(type_id id) const;
  [[nodiscard]] type find_type(type_guid guid) const;
  [[nodiscard]] type find_type(std::string_view name) const;
  [[nodiscard]] type find_type(string_name name) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  template <typename Class> friend class class_builder;

  registry();
  ~registry();

  [[nodiscard]] error attach_type(meta::type type, const void* reflection);

  [[nodiscard]] const void* find_reflection_data(type_id id) const;
};

/** @brief 获取进程内全局反射注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

/** @brief 在全局反射注册表中查找反射类型。 */
[[nodiscard]] CAORS_API type find(type_id id);

/** @brief 在全局反射注册表中按稳定 GUID 查找反射类型。 */
[[nodiscard]] CAORS_API type find(type_guid guid);

/** @brief 在全局反射注册表中按类型名查找反射类型。 */
[[nodiscard]] CAORS_API type find(std::string_view name);

/** @brief 判断指定类型是否挂载了反射组件。 */
[[nodiscard]] CAORS_API bool contains(type_id id);

/** @brief 获取当前反射组件数量。 */
[[nodiscard]] CAORS_API std::size_t size() noexcept;

} // namespace caors::reflection

#endif
