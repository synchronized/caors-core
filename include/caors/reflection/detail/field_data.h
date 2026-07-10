// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_FIELD_DATA_H_
#define CAORS_REFLECTION_DETAIL_FIELD_DATA_H_

#include "caors/meta.h"
#include "caors/meta/type_descriptor_model.h"
#include "caors/traits/container.h"
#include "caors/value/any_ref.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection::detail {

struct field_data;

using field_type_fn = meta::type (*)(const field_data& field) noexcept;
using field_data_fn = const void* (*)(const field_data& field, const void* object) noexcept;
using field_mutable_data_fn = void* (*)(const field_data& field, void* object) noexcept;
using field_get_value_fn = any (*)(const field_data& field, const void* object);
using field_get_const_ref_fn = any_ref (*)(const field_data& field, const void* object);
using field_get_mutable_ref_fn = any_ref (*)(const field_data& field, void* object);
using field_set_value_fn = bool (*)(const field_data& field, void* object, const any& source);

/** @brief 字段访问函数表，负责把字段底层存储转换成运行期值和引用。 */
struct field_ops {
  field_type_fn get_type = nullptr;
  field_data_fn data = nullptr;
  field_mutable_data_fn mutable_data = nullptr;
  field_get_value_fn get_value = nullptr;
  field_get_const_ref_fn get_const_ref = nullptr;
  field_get_mutable_ref_fn get_mutable_ref = nullptr;
  field_set_value_fn set_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_type != nullptr && data != nullptr && mutable_data != nullptr &&
           get_value != nullptr && get_const_ref != nullptr && get_mutable_ref != nullptr &&
           set_value != nullptr;
  }
};

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
  field_ops ops{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return !name.empty() && static_cast<bool>(type) && ops.valid();
  }

  /** @brief 查询字段值类型。 */
  [[nodiscard]] meta::type get_type() const noexcept { return ops.get_type(*this); }

  /** @brief 判断字段是否匹配指定 C++ 类型。 */
  template <typename Field> [[nodiscard]] bool matches_type() const noexcept {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    const meta::type meta = get_type();
    return meta && meta.id() == caors::meta::descriptor_id_of<field_type>() &&
           meta.size() == sizeof(field_type) && meta.alignment() == alignof(field_type);
  }

  /** @brief 获取字段在对象内的只读地址。 */
  [[nodiscard]] const void* member_data(const void* object) const noexcept {
    if (object == nullptr || !type) {
      return nullptr;
    }

    const auto* bytes = static_cast<const unsigned char*>(object);
    return bytes + offset;
  }

  /** @brief 获取成员字段在对象内的可写地址。 */
  [[nodiscard]] void* member_mutable_data(void* object) const noexcept {
    if (object == nullptr || !type || !writable) {
      return nullptr;
    }

    auto* bytes = static_cast<unsigned char*>(object);
    return bytes + offset;
  }

  /** @brief 获取成员字段值的运行期拷贝。 */
  [[nodiscard]] any member_get_value(const void* object) const {
    const void* field_value = member_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any::copy_from_raw(type, descriptor, field_value);
  }

  /** @brief 获取成员字段值的只读运行期引用。 */
  [[nodiscard]] any_ref member_get_ref(const void* object) const {
    const void* field_value = member_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, descriptor, field_value, false);
  }

  /** @brief 获取成员字段值的可写运行期引用。 */
  [[nodiscard]] any_ref member_get_ref(void* object) const {
    void* field_value = member_mutable_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, descriptor, field_value, true);
  }

  /** @brief 从运行期值写入成员字段。 */
  bool member_try_set_value(void* object, const any& source) const {
    const meta::type expected = get_type();
    const auto assign = caors::meta::get_assign_value_fn(expected);
    if (!source.valid() || source.get_type().id() != expected.id() ||
        source.size() != expected.size() || source.alignment() != expected.alignment() ||
        assign == nullptr) {
      return false;
    }

    void* storage = member_mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    return assign(storage, source.data());
  }

  /** @brief 获取字段在对象内的只读地址。 */
  [[nodiscard]] const void* data(const void* object) const noexcept {
    return valid() ? ops.data(*this, object) : nullptr;
  }

  /** @brief 获取字段在对象内的可写地址。 */
  [[nodiscard]] void* mutable_data(void* object) const noexcept {
    return valid() ? ops.mutable_data(*this, object) : nullptr;
  }

  /** @brief 获取字段值的运行期拷贝。 */
  [[nodiscard]] any get_value(const void* object) const {
    return valid() ? ops.get_value(*this, object) : any{};
  }

  /** @brief 获取字段值的只读运行期引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const {
    return valid() ? ops.get_const_ref(*this, object) : any_ref{};
  }

  /** @brief 获取字段值的可写运行期引用。 */
  [[nodiscard]] any_ref get_ref(void* object) const {
    return valid() ? ops.get_mutable_ref(*this, object) : any_ref{};
  }

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const {
    return valid() && ops.set_value(*this, object, source);
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

[[nodiscard]] inline meta::type member_field_type(const field_data& field) noexcept {
  return meta::make_type_view(field.type, field.descriptor);
}

[[nodiscard]] inline const void* member_field_data(const field_data& field,
                                                   const void* object) noexcept {
  return field.member_data(object);
}

[[nodiscard]] inline void* member_field_mutable_data(const field_data& field,
                                                     void* object) noexcept {
  return field.member_mutable_data(object);
}

[[nodiscard]] inline any member_field_get_value(const field_data& field, const void* object) {
  return field.member_get_value(object);
}

[[nodiscard]] inline any_ref member_field_get_const_ref(const field_data& field,
                                                        const void* object) {
  return field.member_get_ref(object);
}

[[nodiscard]] inline any_ref member_field_get_mutable_ref(const field_data& field, void* object) {
  return field.member_get_ref(object);
}

inline bool member_field_set_value(const field_data& field, void* object, const any& source) {
  return field.member_try_set_value(object, source);
}

[[nodiscard]] inline field_ops member_field_ops() noexcept {
  return field_ops{
      &member_field_type,      &member_field_data,          &member_field_mutable_data,
      &member_field_get_value, &member_field_get_const_ref, &member_field_get_mutable_ref,
      &member_field_set_value,
  };
}

/** @brief 创建字段反射元信息。 */
template <typename Field>
[[nodiscard]] field_data make_field_data(std::string_view name, std::string_view type_name,
                                         std::size_t offset) {
  using field_type = std::remove_cv_t<Field>;
  const meta::type_descriptor* descriptor = meta::descriptor_of<field_type>();
  type_id field_type_id = descriptor->type;
  if (!type_name.empty()) {
    field_type_id = detail::find_field_type_id<field_type>(type_name);
  }

  field_data field{
      string_name{name},       field_type_id,      descriptor, offset,
      !std::is_const_v<Field>, member_field_ops(),
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
