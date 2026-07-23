// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_QUERY_H_
#define CAORS_META_QUERY_H_

#include "caors/meta/context.h"
#include "caors/meta/detail/static_type_data_model.h"
#include "caors/meta/detail/type_view.h"
#include "caors/meta/registry.h"

#include <string_view>
#include <type_traits>

namespace caors::meta {

/**
 * @brief 解析 C++ 类型视图；类型不要求已经显式注册。
 *
 * `resolve<T>()` 优先返回已注册 `meta::type`；类型尚未注册时退回未注册 C++ 类型视图。
 * 需要强制判断是否存在稳定名称、GUID、父类型或模块注册 extension 时，应使用 `find<T>()`
 * 查询已注册类型。
 */
template <typename T> [[nodiscard]] type resolve(context& ctx) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return detail::make_type_view(ctx.get_registry(), detail::static_type_id<value_type>(),
                                detail::static_type_data_ptr_of<value_type>());
}

/** @brief 使用默认上下文解析 C++ 类型视图。 */
template <typename T> [[nodiscard]] type resolve() { return resolve<T>(default_context()); }

/** @brief 按 C++ 类型查找已注册基础类型；类型未显式注册时返回空视图。 */
template <typename T> [[nodiscard]] type find(context& ctx) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const type stored = ctx.get_registry().find_type(detail::static_type_id<value_type>());
  return stored.registered() ? stored : type{};
}

/** @brief 使用默认上下文按 C++ 类型查找已注册基础类型。 */
template <typename T> [[nodiscard]] type find() { return find<T>(default_context()); }

/** @brief 按类型 id 查找基础类型。 */
[[nodiscard]] CAORS_API type find(type_id id);
[[nodiscard]] CAORS_API type find(context& ctx, type_id id);

/** @brief 按稳定 GUID 查找基础类型。 */
[[nodiscard]] CAORS_API type find(type_guid guid);
[[nodiscard]] CAORS_API type find(context& ctx, type_guid guid);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API type find(std::string_view name);
[[nodiscard]] CAORS_API type find(context& ctx, std::string_view name);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API type find(string_name name);
[[nodiscard]] CAORS_API type find(context& ctx, string_name name);

/** @brief 获取 C++ 类型的运行期类型 id；类型不要求已经显式注册。 */
template <typename T> [[nodiscard]] type_id id_of() {
  const type stored = resolve<T>();
  return stored ? stored.id() : type_id::invalid();
}

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(string_name name) noexcept;
[[nodiscard]] CAORS_API type_id id(context& ctx, string_name name) noexcept;

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(std::string_view name);
[[nodiscard]] CAORS_API type_id id(context& ctx, std::string_view name);

/** @brief 在全局元数据注册表中判断类型继承关系。 */
[[nodiscard]] CAORS_API bool is_a(type_id id, type_id base_id);
[[nodiscard]] CAORS_API bool is_a(context& ctx, type_id id, type_id base_id);

} // namespace caors::meta

#endif
