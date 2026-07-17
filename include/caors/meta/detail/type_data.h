// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DATA_H_
#define CAORS_META_DETAIL_TYPE_DATA_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/meta/detail/field_data.h"
#include "caors/meta/detail/method_data.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

class registry_state;

/** @brief 注册表内部保存的完整 meta 类型数据。 */
class type_data {
public:
  type_data() = default;
  constexpr type_data(type_id id, string_name name = {}, type_guid guid = type_guid::nil(),
                      type_id parent_id = type_id::invalid()) noexcept
      : id_(id), name_(name), guid_(guid), parent_id_(parent_id) {}

  [[nodiscard]] constexpr type_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr string_name name() const noexcept { return name_; }
  [[nodiscard]] constexpr type_guid guid() const noexcept { return guid_; }
  [[nodiscard]] constexpr type_id parent_id() const noexcept { return parent_id_; }

  [[nodiscard]] constexpr bool valid() const noexcept { return static_cast<bool>(id_); }
  [[nodiscard]] constexpr bool registered() const noexcept { return valid(); }

  constexpr void set_id(type_id id) noexcept { id_ = id; }
  constexpr void set_name(string_name name) noexcept { name_ = name; }
  constexpr void set_guid(type_guid guid) noexcept { guid_ = guid; }
  constexpr void set_parent_id(type_id parent_id) noexcept { parent_id_ = parent_id; }

  [[nodiscard]] bool members_valid() const noexcept;
  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] const field_data* field_at(std::size_t index) const noexcept;
  [[nodiscard]] field_data* mutable_field_at(std::size_t index) noexcept;
  [[nodiscard]] const field_data* find_field_ptr(string_name name) const noexcept;
  [[nodiscard]] field_data* mutable_find_field_ptr(string_name name) noexcept;
  [[nodiscard]] std::optional<field_data> find_field(string_name name) const;
  [[nodiscard]] std::optional<field_data> find_field(std::string_view name) const;
  void add_field(field_data field);
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] const method_data* method_at(std::size_t index) const noexcept;
  [[nodiscard]] const method_data* find_method_ptr(string_name name) const noexcept;
  [[nodiscard]] method_data* mutable_find_method_ptr(string_name name) noexcept;
  [[nodiscard]] std::optional<method_data> find_method(string_name name) const;
  [[nodiscard]] std::optional<method_data> find_method(std::string_view name) const;
  void add_method(method_data method);

private:
  type_id id_{};
  string_name name_{};
  type_guid guid_{};
  type_id parent_id_{};
  vector<field_data> fields_{};
  vector<method_data> methods_{};
};

[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return type_data{detail::type_id_of_cpp_type<T>(), name, type_guid::nil(), parent_id};
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_data make_type_data(string_name name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) noexcept {
  return type_data{detail::type_id_of_cpp_type<T>(), name, guid, parent_id};
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
