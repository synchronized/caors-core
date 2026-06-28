// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_FIELD_H_
#define CAORS_REFLECTION_FIELD_H_

#include "caors/meta.h"
#include "caors/reflection/value.h"
#include "caors/reflection/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors {

/** @brief 字段类型形态。 */
enum class field_kind {
  value,
  enumeration,
  raw_pointer,
  wrapper,
};

namespace detail {

template <typename T>
inline constexpr bool is_supported_wrapper_v = wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool is_nested_field_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

template <typename T> [[nodiscard]] type_id known_type_id() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (has_type_name_v<type>) {
    return make_type_id(type_name<type>::value);
  } else {
    return type_id::invalid();
  }
}

struct field_traits_info {
  field_kind kind = field_kind::value;
  field_wrapper_semantics wrapper_semantics = field_wrapper_semantics::none;
  type_id value_type{};
  std::uint8_t pointer_depth = 0;
  bool nullable = false;
};

template <typename Field> struct field_traits {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;

  [[nodiscard]] static field_traits_info get() {
    field_traits_info info{};
    if constexpr (wrapper_traits<field_type>::is_wrapper) {
      using wrapped_type = typename wrapper_traits<field_type>::wrapped_type;
      using value_type = std::remove_cv_t<wrapped_type>;

      static_assert(!is_nested_field_indirection_v<value_type>,
                    "CAORS reflection supports at most one pointer or wrapper level");

      info.kind = field_kind::wrapper;
      info.wrapper_semantics = wrapper_traits<field_type>::semantics;
      info.value_type = known_type_id<value_type>();
      info.nullable = wrapper_traits<field_type>::nullable;
      return info;
    } else {
      using value_type = field_type;
      info.kind = std::is_enum_v<value_type> ? field_kind::enumeration : field_kind::value;
      info.value_type = known_type_id<value_type>();
      return info;
    }
  }
};

template <typename T> struct field_traits<T*> {
  using value_type = std::remove_cv_t<T>;

  static_assert(!is_nested_field_indirection_v<value_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static field_traits_info get() {
    field_traits_info info{};
    info.kind = field_kind::raw_pointer;
    info.wrapper_semantics = field_wrapper_semantics::inline_value;
    info.value_type = known_type_id<value_type>();
    info.pointer_depth = 1;
    info.nullable = true;
    return info;
  }
};

template <typename Field> [[nodiscard]] field_traits_info make_field_traits_info() {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  return field_traits<field_type>::get();
}

} // namespace detail

/**
 * @brief 底层字段反射元信息。
 *
 * 字段访问基于注册时记录的 offset 和 C++ 类型 key。调用方需要传入该字段所属对象的地址；
 * 类型不匹配、对象为空或字段不可写时会返回失败，不抛出异常。常规查询优先使用
 * `field_view`，该结构主要作为注册表内部存储和底层注册入口。
 */
struct field_info {
  string_name name{};
  type_id type{};
  string_name type_name{};
  detail::cpp_type_key cpp_type = nullptr;
  type_id value_type{};
  detail::value_clone_fn clone_value = nullptr;
  detail::value_destroy_fn destroy_value = nullptr;
  detail::value_assign_fn assign_value = nullptr;
  field_kind kind = field_kind::value;
  field_wrapper_semantics wrapper_semantics = field_wrapper_semantics::none;
  std::uint8_t pointer_depth = 0;
  bool nullable = false;
  std::size_t offset = 0;
  std::size_t size = 0;
  std::size_t alignment = 0;
  bool writable = true;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return !name.empty() && static_cast<bool>(type) && !type_name.empty() && size > 0 &&
           alignment > 0;
  }

  /** @brief 判断字段是否匹配指定 C++ 类型。 */
  template <typename Field> [[nodiscard]] bool matches_type() const noexcept {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    return cpp_type == detail::cpp_type_key_of<field_type>() && size == sizeof(field_type) &&
           alignment == alignof(field_type);
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
  [[nodiscard]] value get_value(const void* object) const {
    const void* field_value = data(object);
    if (field_value == nullptr) {
      return {};
    }

    return value::copy_from_raw(type, type_name, cpp_type, field_value, size, alignment,
                                clone_value, destroy_value, assign_value);
  }

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const value& source) const {
    if (!source.valid() || source.cpp_type() != cpp_type || source.size() != size ||
        source.alignment() != alignment || assign_value == nullptr) {
      return false;
    }

    void* storage = mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    return assign_value(storage, source.data());
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
[[nodiscard]] field_info make_field_info(std::string_view name, std::string_view type_name,
                                         std::size_t offset) {
  using field_type = std::remove_cv_t<Field>;
  const detail::field_traits_info traits = detail::make_field_traits_info<field_type>();
  field_info field{
      string_name{name},
      make_type_id(type_name),
      string_name{type_name},
      detail::cpp_type_key_of<field_type>(),
      traits.value_type,
      detail::value_clone_fn_of<Field>(),
      detail::value_destroy_fn_of<Field>(),
      detail::value_assign_fn_of<Field>(),
      traits.kind,
      traits.wrapper_semantics,
      traits.pointer_depth,
      traits.nullable,
      offset,
      sizeof(field_type),
      alignof(field_type),
      !std::is_const_v<Field>,
  };

  if ((field.kind == field_kind::value || field.kind == field_kind::enumeration) &&
      !field.value_type) {
    field.value_type = field.type;
  }

  return field;
}

namespace detail {

template <typename Member> struct member_pointer_traits;

template <typename Class, typename Field> struct member_pointer_traits<Field Class::*> {
  using class_type = Class;
  using field_type = Field;
};

template <typename Class, typename Field>
[[nodiscard]] std::size_t member_offset(Field Class::* member) noexcept {
  const auto* object = reinterpret_cast<const volatile Class*>(0x1000);
  const auto* field = &(object->*member);
  return reinterpret_cast<std::uintptr_t>(field) - reinterpret_cast<std::uintptr_t>(object);
}

} // namespace detail

} // namespace caors

#endif
