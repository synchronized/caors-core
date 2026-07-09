// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_FIELD_DATA_H_
#define CAORS_REFLECTION_DETAIL_FIELD_DATA_H_

#include "caors/meta.h"
#include "caors/meta/detail/type_descriptor_model.h"
#include "caors/traits/container.h"
#include "caors/value/any_ref.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection::detail {

template <typename T> [[nodiscard]] type_id find_field_type_id(std::string_view name) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const meta::type registered = caors::meta::find<type>();
  if (!registered || registered.name().empty() || registered.name() != name) {
    return type_id::invalid();
  }

  return registered.id();
}

/**
 * @brief 底层字段反射元信息。
 *
 * 字段访问基于注册时记录的 offset 和运行期类型 id。调用方需要传入该字段所属对象的地址；
 * 类型不匹配、对象为空或字段不可写时会返回失败，不抛出异常。常规查询优先使用 `reflection::field`，
 * 该结构主要作为注册表内部存储和底层注册入口。
 */
struct field_data {
  string_name name{};
  type_id type{};
  const meta::type_descriptor* descriptor = nullptr;
  std::size_t offset = 0;
  bool writable = true;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return !name.empty() && static_cast<bool>(type);
  }

  /** @brief 判断字段是否匹配指定 C++ 类型。 */
  template <typename Field> [[nodiscard]] bool matches_type() const noexcept {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    const std::size_t size = descriptor != nullptr ? descriptor->layout.size : 0;
    const std::size_t alignment = descriptor != nullptr ? descriptor->layout.alignment : 0;
    if (descriptor != nullptr) {
      return descriptor->type == caors::meta::detail::type_id_of_cpp_type<field_type>() &&
             size == sizeof(field_type) && alignment == alignof(field_type);
    }

    const meta::type meta = caors::meta::find(type);
    return meta && meta.id() == caors::meta::detail::type_id_of_cpp_type<field_type>() &&
           meta.size() == sizeof(field_type) && meta.alignment() == alignof(field_type);
  }

  /** @brief 获取字段在对象内的只读地址。 */
  [[nodiscard]] const void* data(const void* object) const noexcept {
    if (object == nullptr || !valid()) {
      return nullptr;
    }

    const auto* bytes = static_cast<const unsigned char*>(object);
    return bytes + offset;
  }

  /** @brief 获取字段在对象内的可写地址。 */
  [[nodiscard]] void* mutable_data(void* object) const noexcept {
    if (object == nullptr || !valid() || !writable) {
      return nullptr;
    }

    auto* bytes = static_cast<unsigned char*>(object);
    return bytes + offset;
  }

  /** @brief 获取字段值的运行期拷贝。 */
  [[nodiscard]] any get_value(const void* object) const {
    const void* field_value = data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any::copy_from_raw(type, descriptor, field_value);
  }

  /** @brief 获取字段值的只读运行期引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const {
    const void* field_value = data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, descriptor, field_value, false);
  }

  /** @brief 获取字段值的可写运行期引用。 */
  [[nodiscard]] any_ref get_ref(void* object) const {
    void* field_value = mutable_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, descriptor, field_value, true);
  }

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const {
    const meta::type meta = caors::meta::find(type);
    const auto assign = descriptor != nullptr ? descriptor->ops.assign
                                              : caors::meta::detail::get_assign_value_fn(meta);
    const std::size_t expected_size = descriptor != nullptr ? descriptor->layout.size : meta.size();
    const std::size_t expected_alignment =
        descriptor != nullptr ? descriptor->layout.alignment : meta.alignment();
    if (!source.valid() || source.get_type().id() != type || source.size() != expected_size ||
        source.alignment() != expected_alignment || assign == nullptr) {
      return false;
    }

    void* storage = mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    return assign(storage, source.data());
  }

  /** @brief 按指定类型读取字段值。 */
  template <typename Field>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Field>>>
  try_get(const void* object) const {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    static_assert(std::is_copy_constructible_v<field_type>,
                  "Reflected field values must be copy constructible");

    if (!matches_type<field_type>()) {
      return std::nullopt;
    }

    const void* field_value = data(object);
    if (field_value == nullptr) {
      return std::nullopt;
    }

    return *static_cast<const field_type*>(field_value);
  }

  /** @brief 按指定类型写入字段值。 */
  template <typename Field> bool try_set(void* object, const Field& value) const {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    if (!matches_type<field_type>()) {
      return false;
    }

    void* storage = mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    auto* target = static_cast<field_type*>(storage);
    if constexpr (std::is_assignable_v<field_type&, const Field&>) {
      *target = value;
      return true;
    } else {
      return false;
    }
  }
};

/** @brief 创建字段反射元信息。 */
template <typename Field>
[[nodiscard]] field_data make_field_data(std::string_view name, std::string_view type_name,
                                         std::size_t offset) {
  using field_type = std::remove_cv_t<Field>;
  const meta::type_descriptor* descriptor = meta::detail::descriptor_ptr_of<field_type>();
  type_id field_type_id = descriptor->type;
  if (!type_name.empty()) {
    field_type_id = detail::find_field_type_id<field_type>(type_name);
  }

  field_data field{
      string_name{name}, field_type_id, descriptor, offset, !std::is_const_v<Field>,
  };

  return field;
}

template <typename Class, typename Field>
[[nodiscard]] std::size_t member_offset(Field Class::* member) noexcept {
  const auto* object = reinterpret_cast<const volatile Class*>(0x1000);
  const auto* field = &(object->*member);
  return reinterpret_cast<std::uintptr_t>(field) - reinterpret_cast<std::uintptr_t>(object);
}

} // namespace caors::reflection::detail

#endif
