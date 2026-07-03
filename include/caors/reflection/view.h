// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_VIEW_H_
#define CAORS_REFLECTION_VIEW_H_

#include "caors/export.h"
#include "caors/reflection/type.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection {

class field;
class registry;
class type;

/**
 * @brief 字段反射只读视图。
 *
 * `reflection::field` 是全局反射注册表返回的字段只读句柄。反射数据注册后不可撤销，因此有效视图
 * 可以在进程生命周期内稳定使用。
 */
class CAORS_API field final {
public:
  field() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_id type() const noexcept;
  [[nodiscard]] string_name type_name() const noexcept;

  /** @brief 查询统一 value 类型。容器字段返回元素或 mapped/value 类型。 */
  [[nodiscard]] type_id value_type() const noexcept;

  /** @brief 查询 sequence/set 字段的元素类型，等价于对应语义下的 `value_type()`。 */
  [[nodiscard]] type_id element_type() const noexcept;

  /** @brief 查询 sequence/set 字段的元素类型名。 */
  [[nodiscard]] string_name element_type_name() const noexcept;

  /** @brief 查询 associative 字段的 key 类型。 */
  [[nodiscard]] type_id key_type() const noexcept;

  /** @brief 查询 associative 字段的 key 类型名。 */
  [[nodiscard]] string_name key_type_name() const noexcept;

  /** @brief 查询 pair 字段的 first 类型，当前等价于对应语义下的 `key_type()`。 */
  [[nodiscard]] type_id first_type() const noexcept;

  /** @brief 查询 pair 字段的 first 类型名。 */
  [[nodiscard]] string_name first_type_name() const noexcept;

  /** @brief 查询 associative 字段的 mapped/value 类型，等价于 `value_type()`。 */
  [[nodiscard]] type_id mapped_type() const noexcept;

  /** @brief 查询 associative 字段的 mapped/value 类型名。 */
  [[nodiscard]] string_name mapped_type_name() const noexcept;

  /** @brief 查询 pair 字段的 second 类型，当前等价于对应语义下的 `value_type()`。 */
  [[nodiscard]] type_id second_type() const noexcept;

  /** @brief 查询 pair 字段的 second 类型名。 */
  [[nodiscard]] string_name second_type_name() const noexcept;

  [[nodiscard]] meta::type_kind kind() const noexcept;
  [[nodiscard]] meta::wrapper_semantics wrapper_semantics() const noexcept;
  [[nodiscard]] std::uint8_t pointer_depth() const noexcept;
  [[nodiscard]] bool nullable() const noexcept;
  [[nodiscard]] bool writable() const noexcept;
  [[nodiscard]] std::size_t offset() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;

  /** @brief 获取字段值的运行期拷贝。 */
  [[nodiscard]] any get_value(const void* object) const;

  /** @brief 获取字段值的只读运行期引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const;

  /** @brief 获取字段值的可写运行期引用。 */
  [[nodiscard]] any_ref get_ref(void* object) const;

  /** @brief 获取指针或 wrapper 字段内部值的只读视图。 */
  [[nodiscard]] value::indirect_view create_indirect_view(const void* object) const;

  /** @brief 获取指针或 wrapper 字段内部值的可写视图。 */
  [[nodiscard]] value::indirect_view create_indirect_view(void* object) const;

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const;

  /** @brief 获取对象引用字段指向的对象。 */
  [[nodiscard]] object* get_object_ref(const void* object) const;

  /** @brief 写入对象引用字段。 */
  bool try_set_object_ref(void* object, caors::object* instance) const;

  /** @brief 判断字段是否是顺序容器。 */
  [[nodiscard]] bool is_sequence() const noexcept;

  /** @brief 判断字段是否是集合容器。 */
  [[nodiscard]] bool is_set() const noexcept;

  /** @brief 判断字段是否是关联容器。 */
  [[nodiscard]] bool is_associative() const noexcept;

  /** @brief 判断字段是否是二元组。 */
  [[nodiscard]] bool is_pair() const noexcept;

  /** @brief 判断字段是否是 tuple-like 字段。 */
  [[nodiscard]] bool is_tuple() const noexcept;

  /** @brief 判断字段是否是 variant-like 字段。 */
  [[nodiscard]] bool is_variant() const noexcept;

  /** @brief 按指定类型读取字段值。 */
  template <typename Field>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Field>>>
  try_get(const void* object) const {
    const field_data* reflected_field = info();
    if (reflected_field == nullptr) {
      return std::nullopt;
    }

    return reflected_field->try_get<Field>(object);
  }

  /** @brief 按指定类型写入字段值。 */
  template <typename Field> bool try_set(void* object, const Field& value) const {
    const field_data* reflected_field = info();
    if (reflected_field == nullptr) {
      return false;
    }

    return reflected_field->try_set<Field>(object, value);
  }

private:
  friend class type;

  field(meta::type_view owner, const field_data* reflected_field) noexcept;
  [[nodiscard]] const field_data* info() const noexcept;

  meta::type_view owner_{};
  const field_data* field_ = nullptr;
};

/**
 * @brief 类型反射只读视图。
 *
 * `reflection::type` 不拥有反射数据，只引用全局注册表中 append-only 保存的 `meta::type_view` 和
 * `reflection::type_data`。有效视图可以在进程生命周期内稳定使用。
 */
class CAORS_API type final {
public:
  type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type_view info() const noexcept;
  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] std::optional<field> field_at(std::size_t index) const;
  [[nodiscard]] std::optional<field> find_field(string_name name) const;
  [[nodiscard]] std::optional<field> find_field(std::string_view name) const;

private:
  friend class registry;

  type(meta::type_view info, const type_data* data) noexcept;
  [[nodiscard]] const type_data* data() const noexcept;

  meta::type_view type_{};
  const type_data* data_ = nullptr;
};

} // namespace caors::reflection

#endif
