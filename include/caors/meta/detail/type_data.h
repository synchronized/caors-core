// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DATA_H_
#define CAORS_META_DETAIL_TYPE_DATA_H_

#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/meta/type_id.h"

#include <string_view>

namespace caors::meta::detail {

class registry_state;
class type_data;

[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);

/**
 * @brief 元类型底层数据。
 *
 * 该类型用于注册表内部存储和低层注册描述。对外查询应使用 `meta::type` 视图，避免直接依赖
 * 注册表内部数据布局。
 */
class type_data {
public:
  constexpr type_data() noexcept = default;

  constexpr type_data(type_id id, string_name name, type_guid guid, type_id parent_id) noexcept
      : id_(id), name_(name), guid_(guid), parent_id_(parent_id) {}

  [[nodiscard]] constexpr type_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr string_name name() const noexcept { return name_; }
  [[nodiscard]] constexpr type_guid guid() const noexcept { return guid_; }
  [[nodiscard]] constexpr type_id parent_id() const noexcept { return parent_id_; }

  [[nodiscard]] constexpr bool valid() const noexcept { return static_cast<bool>(id_); }

  [[nodiscard]] constexpr bool registered() const noexcept {
    return static_cast<bool>(id_) && valid();
  }

  constexpr void set_id(type_id id) noexcept { id_ = id; }
  constexpr void set_name(string_name name) noexcept { name_ = name; }
  constexpr void set_guid(type_guid guid) noexcept { guid_ = guid; }
  constexpr void set_parent_id(type_id parent_id) noexcept { parent_id_ = parent_id; }

private:
  friend error merge_type_data(registry_state& state, type_data& stored, const type_data& incoming);

  type_id id_{};
  string_name name_{};
  type_guid guid_{};
  type_id parent_id_{};
};

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return type_data{
      type_id_of_cpp_type<T>(),
      name,
      type_guid::nil(),
      parent_id,
  };
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return type_data{
      type_id_of_cpp_type<T>(),
      name,
      guid,
      parent_id,
  };
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
