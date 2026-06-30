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

namespace caors {

class reflection_registry;

/**
 * @brief 字段反射只读视图。
 *
 * `reflection_field` 是全局反射注册表返回的字段只读句柄。反射数据注册后不可撤销，因此有效视图
 * 可以在进程生命周期内稳定使用。
 */
class CAORS_API reflection_field final {
public:
  reflection_field() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_id type() const noexcept;
  [[nodiscard]] string_name type_name() const noexcept;
  [[nodiscard]] type_id value_type() const noexcept;
  [[nodiscard]] field_kind kind() const noexcept;
  [[nodiscard]] field_wrapper_semantics wrapper_semantics() const noexcept;
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
  [[nodiscard]] any_ref get_mutable_ref(void* object) const;

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const;

  /** @brief 按指定类型读取字段值。 */
  template <typename Field>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Field>>>
  try_get(const void* object) const {
    const reflection_field_data* field = info();
    if (field == nullptr) {
      return std::nullopt;
    }

    return field->try_get<Field>(object);
  }

  /** @brief 按指定类型写入字段值。 */
  template <typename Field> bool try_set(void* object, const Field& value) const {
    const reflection_field_data* field = info();
    if (field == nullptr) {
      return false;
    }

    return field->try_set<Field>(object, value);
  }

private:
  friend class reflection_type;

  reflection_field(meta_type owner, const reflection_field_data* field) noexcept;
  [[nodiscard]] const reflection_field_data* info() const noexcept;

  meta_type owner_{};
  const reflection_field_data* field_ = nullptr;
};

/**
 * @brief 类型反射只读视图。
 *
 * `reflection_type` 不拥有反射数据，只引用全局注册表中 append-only 保存的 `meta_type` 和
 * `reflection_type_data`。有效视图可以在进程生命周期内稳定使用。
 */
class CAORS_API reflection_type final {
public:
  reflection_type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta_type info() const noexcept;
  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] std::optional<reflection_field> field_at(std::size_t index) const;
  [[nodiscard]] std::optional<reflection_field> find_field(string_name name) const;
  [[nodiscard]] std::optional<reflection_field> find_field(std::string_view name) const;

private:
  friend class reflection_registry;

  reflection_type(meta_type type, const reflection_type_data* data) noexcept;
  [[nodiscard]] const reflection_type_data* data() const noexcept;

  meta_type type_{};
  const reflection_type_data* data_ = nullptr;
};

} // namespace caors

#endif
