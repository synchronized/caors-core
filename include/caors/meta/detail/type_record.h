// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_RECORD_H_
#define CAORS_META_DETAIL_TYPE_RECORD_H_

#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/meta/detail/builtin/ops.h"
#include "caors/meta/detail/capability_data.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/metadata.h"
#include "caors/meta/type.h"
#include "caors/meta/type_id.h"
#include "caors/meta/value_ops.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace caors::meta::detail {

class registry_state;
class enum_value_record;
class method_record;
class method_set_record;
class property_record;
struct type_details;

/** @brief 类型语义中引用的另一个 C++ 类型。 */
struct type_relation {
  type_id id{};
  const type_record* fallback = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept { return id && fallback != nullptr; }
};

/** @brief C++ 类型的运行期语义摘要。 */
struct type_semantics {
  type_kind kind = type_kind::value;
  wrapper_semantics wrapper = wrapper_semantics::none;
  type_relation key_type{};
  type_relation value_type{};
  bool indirect = false;
  bool nullable = false;
};

/** @brief 注册类型内部声明的嵌套类型关系。 */
struct nested_type_data {
  string_name name{};
  type_id nested_type{};

  [[nodiscard]] constexpr bool valid() const noexcept { return name && nested_type; }
};

/**
 * @brief 注册表内部保存的完整 meta 类型数据。
 *
 * 类型注册 builder 会在调用模块中构造临时记录，因此共享库构建必须导出其非模板操作。记录最终
 * 仍由 CAORS runtime 接管，调用方不应直接依赖该 detail 类型。
 */
class CAORS_API type_record {
public:
  type_record() = default;
  type_record(type_id id, string_name name = {}, type_guid guid = type_guid::nil(),
              type_id parent_id = type_id::invalid());
  type_record(type_id id, string_name name, type_guid guid, value_storage_ops value,
              type_semantics semantics, builtin_ops builtin,
              const capability_entry* static_capabilities, std::size_t static_capability_count,
              bool registered) noexcept;
  type_record(const type_record& other) = delete;
  type_record& operator=(const type_record& other) = delete;
  type_record(type_record&&) noexcept;
  type_record& operator=(type_record&&) noexcept;
  ~type_record();

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
  [[nodiscard]] bool static_model_matches(const type_record& other) const noexcept;

  constexpr void set_name(string_name name) noexcept { name_ = name; }
  constexpr void set_guid(type_guid guid) noexcept { guid_ = guid; }
  void set_parent_id(type_id parent_id);
  void set_static_data(const type_record& source) noexcept;
  void set_parent_cast(parent_cast_ops cast);

  [[nodiscard]] bool members_valid() const noexcept;
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept;
  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept;
  void set_metadata(meta::metadata_key key, meta::metadata_value value);
  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] const property_record* property_at(std::size_t index) const noexcept;
  [[nodiscard]] const property_record* find_property_ptr(string_name name) const noexcept;
  [[nodiscard]] property_record* mutable_find_property_ptr(string_name name) noexcept;
  void add_property(property_record property);
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] const method_set_record* method_at(std::size_t index) const noexcept;
  [[nodiscard]] const method_set_record* find_method_ptr(string_name name) const noexcept;
  [[nodiscard]] method_set_record* mutable_find_method_ptr(string_name name) noexcept;
  [[nodiscard]] std::optional<std::size_t> add_method(method_record method);
  [[nodiscard]] std::size_t nested_type_count() const noexcept;
  [[nodiscard]] const nested_type_data* nested_type_at(std::size_t index) const noexcept;
  [[nodiscard]] const nested_type_data* find_nested_type_ptr(string_name name) const noexcept;
  [[nodiscard]] bool contains_nested_type(type_id id) const noexcept;
  [[nodiscard]] error add_nested_type(nested_type_data nested_type);
  [[nodiscard]] std::size_t enum_value_count() const noexcept;
  [[nodiscard]] const enum_value_record* enum_value_at(std::size_t index) const noexcept;
  [[nodiscard]] const enum_value_record* find_enum_value_ptr(string_name name) const noexcept;
  [[nodiscard]] enum_value_record* mutable_find_enum_value_ptr(string_name name) noexcept;
  [[nodiscard]] const enum_value_record* find_enum_value_ptr(std::int64_t value) const noexcept;
  [[nodiscard]] const enum_value_record* find_enum_value_ptr(std::uint64_t value) const noexcept;
  void add_enum_value(enum_value_record value);
  [[nodiscard]] const void* find_capability(capability_key key) const noexcept;
  [[nodiscard]] const void* find_static_capability(capability_key key) const noexcept;
  [[nodiscard]] bool contains_capability(capability_key key) const noexcept;
  [[nodiscard]] std::size_t capability_count() const noexcept;
  [[nodiscard]] error add_capability(capability_key key, const void* capability);
  [[nodiscard]] error add_owned_capability(capability_key key,
                                           memory::unique_ptr<owned_capability_base> capability);

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
[[nodiscard]] type_record make_type_record(string_name name,
                                           type_id parent_id = type_id::invalid()) noexcept {
  return type_record{detail::type_id_of_cpp_type<T>(), name, type_guid::nil(), parent_id};
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_record make_type_record(string_name name, type_guid guid,
                                           type_id parent_id = type_id::invalid()) noexcept {
  return type_record{detail::type_id_of_cpp_type<T>(), name, guid, parent_id};
}

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_record make_type_record(std::string_view name,
                                           type_id parent_id = type_id::invalid()) {
  return make_type_record<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] type_record make_type_record(std::string_view name, type_guid guid,
                                           type_id parent_id = type_id::invalid()) {
  return make_type_record<T>(string_name{name}, guid, parent_id);
}

} // namespace caors::meta::detail

#endif
