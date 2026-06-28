// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_VIEW_H_
#define CAORS_REFLECTION_VIEW_H_

#include "caors/export.h"
#include "caors/reflection/type.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors {

class reflection_registry;
class reflection_registry_state;

/**
 * @brief 字段反射只读视图。
 *
 * `field_view` 绑定到产生它的 `reflection_registry`。当注册表销毁或清理后，视图会失效；
 * 外部代码应优先通过该类型读取字段描述和访问字段值，而不是直接持有 `field_info`。
 */
class CAORS_API field_view final {
public:
  field_view() noexcept = default;

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
  [[nodiscard]] value get_value(const void* object) const;

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const value& source) const;

  /** @brief 按指定类型读取字段值。 */
  template <typename Field>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Field>>>
  try_get(const void* object) const {
    const field_info* field = info();
    if (field == nullptr) {
      return std::nullopt;
    }

    return field->try_get<Field>(object);
  }

  /** @brief 按指定类型写入字段值。 */
  template <typename Field> bool try_set(void* object, const Field& value) const {
    const field_info* field = info();
    if (field == nullptr) {
      return false;
    }

    return field->try_set<Field>(object, value);
  }

private:
  friend class reflection_type_view;

  field_view(std::weak_ptr<const reflection_registry_state> state, std::uint64_t generation,
             std::shared_ptr<const reflection_type> type, std::size_t field_index) noexcept;
  [[nodiscard]] const field_info* info() const noexcept;

  std::weak_ptr<const reflection_registry_state> state_{};
  std::uint64_t generation_ = 0;
  std::shared_ptr<const reflection_type> type_{};
  std::size_t field_index_ = 0;
};

/**
 * @brief 类型反射只读视图。
 *
 * 视图持有类型描述快照，并通过注册表 generation 判断有效性。注册表销毁或 `clear()` 后，
 * 已有视图会失效，避免外部长期持有内部存储。
 */
class CAORS_API reflection_type_view final {
public:
  reflection_type_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_info info() const noexcept;
  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] std::optional<field_view> field_at(std::size_t index) const;
  [[nodiscard]] std::optional<field_view> find_field(string_name name) const;
  [[nodiscard]] std::optional<field_view> find_field(std::string_view name) const;

private:
  friend class reflection_registry;

  reflection_type_view(std::weak_ptr<const reflection_registry_state> state,
                       std::uint64_t generation,
                       std::shared_ptr<const reflection_type> type) noexcept;
  [[nodiscard]] const reflection_type* type() const noexcept;

  std::weak_ptr<const reflection_registry_state> state_{};
  std::uint64_t generation_ = 0;
  std::shared_ptr<const reflection_type> type_{};
};

} // namespace caors

#endif
