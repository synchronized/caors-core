// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_PROPERTY_H_
#define CAORS_META_PROPERTY_H_

#include "caors/export.h"
#include "caors/meta/property_schema.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/value.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::meta {

class property;
class property_accessor;

[[nodiscard]] CAORS_API property_accessor
compile_property_accessor(const property& source) noexcept;

namespace detail {
class property_record;
class property_access;
} // namespace detail

class type;
class instance;

/**
 * @brief 属性反射只读视图。
 *
 * `meta::property` 是全局 `meta::registry` 返回的属性只读句柄。属性数据注册后不可撤销，
 * 因此有效视图可以在进程生命周期内稳定使用。
 */
class CAORS_API property final {
public:
  property() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  /** @brief 查询属性所属类型视图。 */
  [[nodiscard]] meta::type get_owner_type() const noexcept;
  /** @brief 查询属性声明类型视图。 */
  [[nodiscard]] meta::type get_type() const noexcept;

  /** @brief 查询统一 value 类型。容器属性返回元素或 mapped/value 类型。 */
  [[nodiscard]] meta::type get_value_type() const noexcept;

  /** @brief 查询 sequence/set 属性的元素类型，等价于对应语义下的 `get_value_type()`。 */
  [[nodiscard]] meta::type get_element_type() const noexcept;

  /** @brief 查询 associative 属性的 key 类型。 */
  [[nodiscard]] meta::type get_key_type() const noexcept;

  /** @brief 查询 pair 属性的 first 类型，当前等价于对应语义下的 `get_key_type()`。 */
  [[nodiscard]] meta::type get_first_type() const noexcept;

  /** @brief 查询 associative 属性的 mapped/value 类型，等价于 `get_value_type()`。 */
  [[nodiscard]] meta::type get_mapped_type() const noexcept;

  /** @brief 查询 pair 属性的 second 类型，当前等价于对应语义下的 `get_value_type()`。 */
  [[nodiscard]] meta::type get_second_type() const noexcept;

  [[nodiscard]] meta::type_kind kind() const noexcept;
  [[nodiscard]] meta::wrapper_semantics wrapper_semantics() const noexcept;
  [[nodiscard]] bool indirect() const noexcept;
  [[nodiscard]] bool nullable() const noexcept;
  [[nodiscard]] bool writable() const noexcept;
  [[nodiscard]] const meta::property_schema& schema() const noexcept;
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept;
  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept;
  [[nodiscard]] std::size_t offset() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;

  /** @brief 获取实例属性值的高层运行期值。 */
  [[nodiscard]] meta::value get_value(const instance& object) const;

  /** @brief 获取全局属性值的高层运行期值。成员属性会返回 `null`。 */
  [[nodiscard]] meta::value get_value() const;

  /** @brief 获取实例属性值的精确 C++ 拷贝。 */
  [[nodiscard]] any get_any(const instance& object) const;

  /** @brief 获取全局属性值的精确 C++ 拷贝。成员属性会返回无效 `any`。 */
  [[nodiscard]] any get_any() const;

  /** @brief 获取实例属性值引用；可写实例返回可写引用，只读实例返回只读引用。 */
  [[nodiscard]] any_ref get_ref(const instance& object) const;

  /** @brief 获取全局属性值的只读运行期引用。成员属性会返回无效引用。 */
  [[nodiscard]] any_ref get_ref() const;

  /** @brief 从高层运行期值写入实例属性。 */
  bool try_set_value(const instance& object, const meta::value& source) const;

  /** @brief 从高层运行期值写入全局属性。成员属性会返回 false。 */
  bool try_set_value(const meta::value& source) const;

  /** @brief 从精确 C++ 运行期值写入实例属性。 */
  bool try_set_any(const instance& object, const any& source) const;

  /** @brief 从精确 C++ 运行期值写入全局属性。成员属性会返回 false。 */
  bool try_set_any(const any& source) const;

  /** @brief 判断属性是否是顺序容器。 */
  [[nodiscard]] bool is_sequence() const noexcept;

  /** @brief 判断属性是否是集合容器。 */
  [[nodiscard]] bool is_set() const noexcept;

  /** @brief 判断属性是否是关联容器。 */
  [[nodiscard]] bool is_associative() const noexcept;

  /** @brief 判断属性是否是二元组。 */
  [[nodiscard]] bool is_pair() const noexcept;

  /** @brief 判断属性是否是 tuple-like 类型。 */
  [[nodiscard]] bool is_tuple() const noexcept;

  /** @brief 判断属性是否是 variant-like 类型。 */
  [[nodiscard]] bool is_variant() const noexcept;

  /** @brief 按指定类型读取实例属性值。 */
  template <typename Property>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Property>>>
  try_get(const instance& object) const {
    using property_type = std::remove_cv_t<std::remove_reference_t<Property>>;
    static_assert(std::is_copy_constructible_v<property_type>,
                  "Reflected property values must be copy constructible");

    const any_ref ref = get_ref(object);
    const property_type* value = ref.template try_get<property_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

  /** @brief 按指定类型读取全局属性值。 */
  template <typename Property>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Property>>> try_get() const {
    using property_type = std::remove_cv_t<std::remove_reference_t<Property>>;
    static_assert(std::is_copy_constructible_v<property_type>,
                  "Reflected property values must be copy constructible");

    const any_ref ref = get_ref();
    const property_type* value = ref.template try_get<property_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

  /** @brief 按指定类型写入实例属性值。 */
  template <typename Property> bool try_set(const instance& object, const Property& value) const {
    const any_ref ref = get_ref(object);
    return ref.try_set(value);
  }

  /** @brief 按指定类型写入全局属性值。 */
  template <typename Property> bool try_set(const Property& value) const {
    const any_ref ref = get_ref();
    return ref.try_set(value);
  }

private:
  friend class detail::property_access;
  friend CAORS_API property_accessor compile_property_accessor(const property& source) noexcept;

  property(const detail::property_record* reflected_property,
           std::shared_ptr<detail::registry_layer_state> layer) noexcept;
  [[nodiscard]] const detail::property_record* info() const noexcept;

  const detail::property_record* property_ = nullptr;
  std::shared_ptr<detail::registry_layer_state> layer_{};
};

} // namespace caors::meta

#endif
