// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RUNTIME_QUERY_H_
#define CAORS_RUNTIME_QUERY_H_

#include "caors/meta/query.h"
#include "caors/runtime/type.h"

#include <string_view>
#include <type_traits>

namespace caors::runtime {

/** @brief 解析 C++ 类型运行时视图；类型不要求已经显式注册。 */
template <typename T> [[nodiscard]] type resolve() {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return meta::resolve<value_type>();
}

/** @brief 按 C++ 类型查找已注册运行时类型。 */
template <typename T> [[nodiscard]] type find() {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return meta::find<value_type>();
}

/** @brief 按运行期类型 id 查找运行时类型。 */
[[nodiscard]] CAORS_API type find(type_id id);

/** @brief 按稳定 GUID 查找运行时类型。 */
[[nodiscard]] CAORS_API type find(type_guid guid);

/** @brief 按类型名查找运行时类型。 */
[[nodiscard]] CAORS_API type find(std::string_view name);

/** @brief 按类型名查找运行时类型。 */
[[nodiscard]] CAORS_API type find(string_name name);

} // namespace caors::runtime

#endif
