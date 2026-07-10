// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_FIELD_H_
#define CAORS_REFLECTION_FIELD_H_

#include "caors/export.h"
#include "caors/meta/type.h"
#include "caors/value/any_ref.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::objects {

class object;

} // namespace caors::objects

namespace caors::reflection {

class type;

namespace detail {

class field_wrapper;

} // namespace detail

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
  /** @brief 查询字段声明类型视图。 */
  [[nodiscard]] meta::type get_type() const noexcept;

  /** @brief 查询统一 value 类型。容器字段返回元素或 mapped/value 类型。 */
  [[nodiscard]] meta::type get_value_type() const noexcept;

  /** @brief 查询 sequence/set 字段的元素类型，等价于对应语义下的 `get_value_type()`。 */
  [[nodiscard]] meta::type get_element_type() const noexcept;

  /** @brief 查询 associative 字段的 key 类型。 */
  [[nodiscard]] meta::type get_key_type() const noexcept;

  /** @brief 查询 pair 字段的 first 类型，当前等价于对应语义下的 `get_key_type()`。 */
  [[nodiscard]] meta::type get_first_type() const noexcept;

  /** @brief 查询 associative 字段的 mapped/value 类型，等价于 `get_value_type()`。 */
  [[nodiscard]] meta::type get_mapped_type() const noexcept;

  /** @brief 查询 pair 字段的 second 类型，当前等价于对应语义下的 `get_value_type()`。 */
  [[nodiscard]] meta::type get_second_type() const noexcept;

  [[nodiscard]] meta::type_kind kind() const noexcept;
  [[nodiscard]] meta::wrapper_semantics wrapper_semantics() const noexcept;
  [[nodiscard]] bool indirect() const noexcept;
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

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const;

  /** @brief 获取对象指针字段指向的对象。 */
  [[nodiscard]] objects::object* get_object_ptr(const void* object) const;

  /** @brief 写入对象指针字段。 */
  bool try_set_object_ptr(void* object, objects::object* instance) const;

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
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    static_assert(std::is_copy_constructible_v<field_type>,
                  "Reflected field values must be copy constructible");

    const any_ref ref = get_ref(object);
    const field_type* value = ref.template try_get<field_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

  /** @brief 按指定类型写入字段值。 */
  template <typename Field> bool try_set(void* object, const Field& value) const {
    const any_ref ref = get_ref(object);
    return ref.try_set(value);
  }

private:
  friend class type;

  field(meta::type owner, const detail::field_wrapper* reflected_field) noexcept;
  [[nodiscard]] const detail::field_wrapper* info() const noexcept;

  meta::type owner_{};
  const detail::field_wrapper* field_ = nullptr;
};

} // namespace caors::reflection

#endif
