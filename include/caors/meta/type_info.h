// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_INFO_H_
#define CAORS_META_TYPE_INFO_H_

#include "caors/meta/type_id.h"

#include <cstddef>
#include <string_view>

namespace caors {

/**
 * @brief 对象系统和反射系统共享的基础类型描述。
 *
 * 该结构只保存类型身份、名称、稳定 GUID、布局和父类型关系，不包含对象生命周期或字段/方法反射
 * 信息。
 */
struct type_info {
  type_id id{};
  string_name name{};
  type_guid guid{};
  std::size_t size = 0;
  std::size_t alignment = 0;
  type_id parent_id{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return static_cast<bool>(id) && !name.empty() && size > 0 && alignment > 0;
  }
};

/** @brief 创建基础类型描述。 */
template <typename T>
[[nodiscard]] constexpr type_info make_type_info(string_name name,
                                                 type_id parent_id = type_id::invalid()) noexcept {
  return type_info{
      make_type_id(name), name, type_guid::nil(), sizeof(T), alignof(T), parent_id,
  };
}

/** @brief 创建带稳定 GUID 的基础类型描述。 */
template <typename T>
[[nodiscard]] constexpr type_info make_type_info(string_name name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) noexcept {
  return type_info{
      make_type_id(name), name, guid, sizeof(T), alignof(T), parent_id,
  };
}

/** @brief 创建基础类型描述。 */
template <typename T>
[[nodiscard]] type_info make_type_info(std::string_view name,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_info<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础类型描述。 */
template <typename T>
[[nodiscard]] type_info make_type_info(std::string_view name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_info<T>(string_name{name}, guid, parent_id);
}

} // namespace caors

#endif
