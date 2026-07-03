// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_FIELD_H_
#define CAORS_REFLECTION_FIELD_H_

#include "caors/meta.h"
#include "caors/adapters/container_traits.h"
#include "caors/object/wrapper_traits.h"
#include "caors/value/any.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection::detail {

using object_ref_get_fn = object* (*)(const void*) noexcept;
using object_ref_set_fn = bool (*)(void*, object*) noexcept;

template <typename T> [[nodiscard]] type_id register_field_type_id(std::string_view name) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const error result = caors::meta::detail::register_type<type>(name);
  if (result.succeeded() || result.value() == errc::duplicate_id) {
    const std::optional<meta::type_view> registered = caors::meta::find<type>();
    return registered.has_value() ? registered->id() : type_id::invalid();
  }

  return type_id::invalid();
}

template <typename Field> [[nodiscard]] object_ref_get_fn object_ref_get_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (objects::detail::object_ref_traits<field_type>::is_object_ref) {
    return [](const void* storage) noexcept -> object* {
      if (storage == nullptr) {
        return nullptr;
      }

      return static_cast<const field_type*>(storage)->get();
    };
  } else {
    return nullptr;
  }
}

template <typename Field> [[nodiscard]] object_ref_set_fn object_ref_set_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (objects::detail::object_ref_traits<field_type>::is_object_ref) {
    using object_type = typename objects::detail::object_ref_traits<field_type>::object_type;
    return [](void* storage, object* instance) noexcept -> bool {
      if (storage == nullptr) {
        return false;
      }

      if (instance == nullptr) {
        static_cast<field_type*>(storage)->reset();
        return true;
      }

      object_type* typed = dynamic_cast<object_type*>(instance);
      if (typed == nullptr) {
        return false;
      }

      static_cast<field_type*>(storage)->reset(typed);
      return true;
    };
  } else {
    return nullptr;
  }
}

} // namespace caors::reflection::detail

namespace caors::reflection {

/**
 * @brief 底层字段反射元信息。
 *
 * 字段访问基于注册时记录的 offset 和 C++ 类型 key。调用方需要传入该字段所属对象的地址；
 * 类型不匹配、对象为空或字段不可写时会返回失败，不抛出异常。常规查询优先使用
 * `reflection::field`，该结构主要作为注册表内部存储和底层注册入口。
 */
struct field_data {
  string_name name{};
  type_id type{};
  detail::object_ref_get_fn get_object_ref = nullptr;
  detail::object_ref_set_fn set_object_ref = nullptr;
  std::size_t offset = 0;
  bool writable = true;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return !name.empty() && static_cast<bool>(type);
  }

  /** @brief 判断字段是否匹配指定 C++ 类型。 */
  template <typename Field> [[nodiscard]] bool matches_type() const noexcept {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    const std::optional<meta::type_view> meta = caors::meta::find(type);
    return meta.has_value() && meta->cpp_type() == caors::detail::cpp_type_key_of<field_type>() &&
           meta->size() == sizeof(field_type) && meta->alignment() == alignof(field_type);
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

    return any::copy_from_raw(type, field_value);
  }

  /** @brief 获取字段值的只读运行期引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const {
    const void* field_value = data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, field_value, false);
  }

  /** @brief 获取字段值的可写运行期引用。 */
  [[nodiscard]] any_ref get_ref(void* object) const {
    void* field_value = mutable_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, field_value, true);
  }

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const {
    const std::optional<meta::type_view> meta = caors::meta::find(type);
    if (!source.valid() || !meta.has_value() || source.cpp_type() != meta->cpp_type() ||
        source.size() != meta->size() || source.alignment() != meta->alignment() ||
        meta->assign_value() == nullptr) {
      return false;
    }

    void* storage = mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    return meta->assign_value()(storage, source.data());
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
  const type_id field_type_id = detail::register_field_type_id<field_type>(type_name);

  field_data field{
      string_name{name},
      field_type_id,
      detail::object_ref_get_fn_of<Field>(),
      detail::object_ref_set_fn_of<Field>(),
      offset,
      !std::is_const_v<Field>,
  };

  if constexpr (adapters::sequence_traits<field_type>::is_sequence) {
    (void)meta::detail::register_container_data_for<field_type>(field.type);
  } else if constexpr (adapters::set_traits<field_type>::is_set) {
    (void)meta::detail::register_container_data_for<field_type>(field.type);
  } else if constexpr (adapters::associative_traits<field_type>::is_associative) {
    (void)meta::detail::register_container_data_for<field_type>(field.type);
  } else if constexpr (adapters::pair_traits<field_type>::is_pair) {
    (void)meta::detail::register_container_data_for<field_type>(field.type);
  } else if constexpr (adapters::tuple_traits<field_type>::is_tuple) {
    (void)meta::detail::register_container_data_for<field_type>(field.type);
  } else if constexpr (adapters::variant_traits<field_type>::is_variant) {
    (void)meta::detail::register_container_data_for<field_type>(field.type);
  }

  return field;
}

} // namespace caors::reflection

namespace caors::reflection::detail {

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

} // namespace caors::reflection::detail

#endif
