// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DATA_H_
#define CAORS_META_DETAIL_TYPE_DATA_H_

#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/meta/detail/capability_data.h"
#include "caors/meta/detail/extension_data.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace caors::meta::detail {

class registry_state;
class enum_value_data;
class method_data;
class method_set_data;
class property_data;
struct type_details;

/** @brief 注册类型内部声明的嵌套类型关系。 */
struct nested_type_data {
  string_name name{};
  type_id nested_type{};

  [[nodiscard]] constexpr bool valid() const noexcept { return name && nested_type; }
};

/** @brief 注册表内部保存的完整 meta 类型数据。 */
class type_data {
public:
  type_data() = default;
  type_data(type_id id, string_name name = {}, type_guid guid = type_guid::nil(),
            type_id parent_id = type_id::invalid());
  type_data(const type_descriptor& descriptor, bool registered) noexcept;
  type_data(const type_data& other) = delete;
  type_data& operator=(const type_data& other) = delete;
  type_data(type_data&&) noexcept;
  type_data& operator=(type_data&&) noexcept;
  ~type_data();

  [[nodiscard]] constexpr type_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr string_name name() const noexcept { return name_; }
  [[nodiscard]] constexpr type_guid guid() const noexcept { return guid_; }
  [[nodiscard]] type_id parent_id() const noexcept;
  [[nodiscard]] constexpr const value_storage_ops& value_ops() const noexcept { return value_; }
  [[nodiscard]] constexpr const type_semantics& semantics() const noexcept { return semantics_; }
  [[nodiscard]] constexpr const builtin_ops& builtin() const noexcept { return builtin_; }
  [[nodiscard]] const parent_cast_ops* parent_cast() const noexcept;

  [[nodiscard]] constexpr bool valid() const noexcept { return static_cast<bool>(id_); }
  [[nodiscard]] constexpr bool registered() const noexcept { return valid() && registered_; }

  constexpr void set_name(string_name name) noexcept { name_ = name; }
  constexpr void set_guid(type_guid guid) noexcept { guid_ = guid; }
  void set_parent_id(type_id parent_id);
  void set_static_data(const type_descriptor& descriptor) noexcept;
  void set_parent_cast(parent_cast_ops cast);

  [[nodiscard]] bool members_valid() const noexcept;
  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] const property_data* property_at(std::size_t index) const noexcept;
  [[nodiscard]] const property_data* find_property_ptr(string_name name) const noexcept;
  [[nodiscard]] property_data* mutable_find_property_ptr(string_name name) noexcept;
  void add_property(property_data property);
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] const method_set_data* method_at(std::size_t index) const noexcept;
  [[nodiscard]] const method_set_data* find_method_ptr(string_name name) const noexcept;
  [[nodiscard]] method_set_data* mutable_find_method_ptr(string_name name) noexcept;
  [[nodiscard]] std::optional<std::size_t> add_method(method_data method);
  [[nodiscard]] std::size_t nested_type_count() const noexcept;
  [[nodiscard]] const nested_type_data* nested_type_at(std::size_t index) const noexcept;
  [[nodiscard]] const nested_type_data* find_nested_type_ptr(string_name name) const noexcept;
  [[nodiscard]] bool contains_nested_type(type_id id) const noexcept;
  [[nodiscard]] error add_nested_type(nested_type_data nested_type);
  [[nodiscard]] std::size_t enum_value_count() const noexcept;
  [[nodiscard]] const enum_value_data* enum_value_at(std::size_t index) const noexcept;
  [[nodiscard]] const enum_value_data* find_enum_value_ptr(string_name name) const noexcept;
  [[nodiscard]] enum_value_data* mutable_find_enum_value_ptr(string_name name) noexcept;
  [[nodiscard]] const enum_value_data* find_enum_value_ptr(std::int64_t value) const noexcept;
  [[nodiscard]] const enum_value_data* find_enum_value_ptr(std::uint64_t value) const noexcept;
  void add_enum_value(enum_value_data value);
  [[nodiscard]] const void* find_capability(capability_key key) const noexcept;
  [[nodiscard]] const void* find_static_capability(capability_key key) const noexcept;
  [[nodiscard]] bool contains_capability(capability_key key) const noexcept;
  [[nodiscard]] std::size_t capability_count() const noexcept;
  [[nodiscard]] error add_capability(capability_key key, const void* capability);
  [[nodiscard]] error add_owned_capability(capability_key key,
                                           memory::unique_ptr<owned_capability_base> capability);
  [[nodiscard]] bool contains_extension(extension_key key) const noexcept;
  [[nodiscard]] const extension_data* find_extension(extension_key key) const noexcept;
  [[nodiscard]] extension_data* mutable_find_extension(extension_key key) noexcept;
  [[nodiscard]] error add_extension(extension_data extension);

private:
  [[nodiscard]] type_details& ensure_details();
  [[nodiscard]] const type_details* details() const noexcept { return details_; }

  type_id id_{};
  string_name name_{};
  type_guid guid_{};
  value_storage_ops value_{};
  type_semantics semantics_{};
  builtin_ops builtin_{};
  const capability_entry* static_capabilities_ = nullptr;
  std::size_t static_capability_count_ = 0;
  bool registered_ = false;
  type_details* details_ = nullptr;
};

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
