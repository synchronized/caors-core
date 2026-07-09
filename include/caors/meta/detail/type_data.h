// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DATA_H_
#define CAORS_META_DETAIL_TYPE_DATA_H_

#include "caors/error.h"
#include "caors/meta/type_registration.h"

namespace caors::meta::detail {

class registry_state;
using type_data = meta::type_registration_data;

[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return meta::make_type_registration_data<T>(name, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return meta::make_type_registration_data<T>(name, guid, parent_id);
}

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(std::string_view name,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_data<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(std::string_view name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_data<T>(string_name{name}, guid, parent_id);
}

} // namespace caors::meta::detail

#endif
