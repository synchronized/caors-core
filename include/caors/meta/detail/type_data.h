// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DATA_H_
#define CAORS_META_DETAIL_TYPE_DATA_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/meta/detail/capability_data.h"
#include "caors/meta/detail/enum_value_data.h"
#include "caors/meta/detail/method_data.h"
#include "caors/meta/detail/property_data.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <cstdint>
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
  type_data(const type_data& other);
  type_data& operator=(const type_data& other);
  type_data(type_data&&) noexcept = default;
  type_data& operator=(type_data&&) noexcept = default;

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
  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] const property_data* property_at(std::size_t index) const noexcept;
  [[nodiscard]] property_data* mutable_property_at(std::size_t index) noexcept;
  [[nodiscard]] const property_data* find_property_ptr(string_name name) const noexcept;
  [[nodiscard]] property_data* mutable_find_property_ptr(string_name name) noexcept;
  [[nodiscard]] std::optional<property_data> find_property(string_name name) const;
  [[nodiscard]] std::optional<property_data> find_property(std::string_view name) const;
  void add_property(property_data property);
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] const method_data* method_at(std::size_t index) const noexcept;
  [[nodiscard]] const method_data* find_method_ptr(string_name name) const noexcept;
  [[nodiscard]] method_data* mutable_find_method_ptr(string_name name) noexcept;
  [[nodiscard]] std::optional<method_data> find_method(string_name name) const;
  [[nodiscard]] std::optional<method_data> find_method(std::string_view name) const;
  void add_method(method_data method);
  [[nodiscard]] std::size_t enum_value_count() const noexcept;
  [[nodiscard]] const enum_value_data* enum_value_at(std::size_t index) const noexcept;
  [[nodiscard]] const enum_value_data* find_enum_value_ptr(string_name name) const noexcept;
  [[nodiscard]] enum_value_data* mutable_find_enum_value_ptr(string_name name) noexcept;
  [[nodiscard]] const enum_value_data* find_enum_value_ptr(std::int64_t value) const noexcept;
  [[nodiscard]] const enum_value_data* find_enum_value_ptr(std::uint64_t value) const noexcept;
  void add_enum_value(enum_value_data value);
  [[nodiscard]] const void* find_capability(capability_key key) const noexcept;
  [[nodiscard]] bool contains_capability(capability_key key) const noexcept;
  [[nodiscard]] std::size_t capability_count() const noexcept;
  [[nodiscard]] error add_capability(capability_key key, const void* capability);
  [[nodiscard]] error add_owned_capability(capability_key key,
                                           memory::unique_ptr<owned_capability_base> capability);

private:
  type_id id_{};
  string_name name_{};
  type_guid guid_{};
  type_id parent_id_{};
  vector<property_data> properties_{};
  vector<method_data> methods_{};
  vector<enum_value_data> enum_values_{};
  vector<capability_data> capabilities_{};
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
