// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_FIELD_H_
#define CAORS_REFLECTION_FIELD_H_

#include "caors/meta.h"
#include "caors/reflection/container_traits.h"
#include "caors/reflection/value.h"
#include "caors/reflection/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors {

namespace detail {

using object_ref_get_fn = object* (*)(const void*) noexcept;
using object_ref_set_fn = bool (*)(void*, object*) noexcept;

template <typename T>
inline constexpr bool is_supported_wrapper_v = wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool is_nested_field_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

template <typename T> [[nodiscard]] const void* raw_pointer_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return *static_cast<T* const*>(storage);
}

template <typename T> [[nodiscard]] void* raw_pointer_mutable_value(void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_const_v<T>) {
    return nullptr;
  } else {
    return *static_cast<T**>(storage);
  }
}

template <typename T> [[nodiscard]] bool raw_pointer_has_value(const void* storage) noexcept {
  return raw_pointer_value<T>(storage) != nullptr;
}

template <typename T> bool raw_pointer_reset(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  *static_cast<T**>(storage) = nullptr;
  return true;
}

template <typename Wrapper>
[[nodiscard]] const void* wrapper_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return wrapper_access<Wrapper>::value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] void* wrapper_mutable_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return wrapper_access<Wrapper>::mutable_value(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] bool wrapper_has_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return wrapper_access<Wrapper>::has_value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper> bool wrapper_reset_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return wrapper_access<Wrapper>::reset(*static_cast<Wrapper*>(storage));
}

template <typename T> [[nodiscard]] type_id known_type_id() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (has_type_name_v<type>) {
    const error result = caors::register_type<type>(type_name<type>::value);
    if (result.succeeded() || result.value() == errc::duplicate_id) {
      const std::optional<meta_type> registered = caors::find_type<type>();
      return registered.has_value() ? registered->id() : type_id::invalid();
    }
  } else {
    return type_id::invalid();
  }

  return type_id::invalid();
}

template <typename T> [[nodiscard]] type_id register_field_type_id(std::string_view name) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const error result = caors::register_type<type>(name);
  if (result.succeeded() || result.value() == errc::duplicate_id) {
    const std::optional<meta_type> registered = caors::find_type<type>();
    return registered.has_value() ? registered->id() : type_id::invalid();
  }

  return type_id::invalid();
}

template <typename Field> struct type_semantics_traits {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;

  [[nodiscard]] static meta_type_semantics_data get() {
    meta_type_semantics_data info{};
    if constexpr (sequence_traits<field_type>::is_sequence) {
      using element_type = typename sequence_traits<field_type>::element_type;

      info.kind = type_kind::sequence;
      info.value_type = known_type_id<field_type>();
      info.element_type = known_type_id<element_type>();
      return info;
    } else if constexpr (set_traits<field_type>::is_set) {
      using element_type = typename set_traits<field_type>::element_type;

      info.kind = type_kind::set;
      info.value_type = known_type_id<field_type>();
      info.element_type = known_type_id<element_type>();
      return info;
    } else if constexpr (associative_traits<field_type>::is_associative) {
      using key_type = typename associative_traits<field_type>::key_type;
      using mapped_type = typename associative_traits<field_type>::mapped_type;

      info.kind = type_kind::associative;
      info.value_type = known_type_id<field_type>();
      info.key_type = known_type_id<key_type>();
      info.mapped_type = known_type_id<mapped_type>();
      return info;
    } else if constexpr (wrapper_traits<field_type>::is_wrapper) {
      using wrapped_type = typename wrapper_traits<field_type>::wrapped_type;
      using value_type = std::remove_cv_t<wrapped_type>;

      static_assert(!is_nested_field_indirection_v<value_type>,
                    "CAORS reflection supports at most one pointer or wrapper level");

      info.kind = type_kind::wrapper;
      info.wrapper_semantics = wrapper_traits<field_type>::semantics;
      info.value_type = known_type_id<value_type>();
      info.indirect_value = &wrapper_indirect_value<field_type>;
      info.mutable_indirect_value = &wrapper_mutable_indirect_value<field_type>;
      info.has_indirect_value = &wrapper_has_indirect_value<field_type>;
      info.reset_indirect_value = &wrapper_reset_indirect_value<field_type>;
      info.nullable = wrapper_traits<field_type>::nullable;
      return info;
    } else {
      using value_type = field_type;
      info.kind = std::is_enum_v<value_type> ? type_kind::enumeration : type_kind::value;
      info.value_type = known_type_id<value_type>();
      return info;
    }
  }
};

template <typename T> struct type_semantics_traits<T*> {
  using value_type = std::remove_cv_t<T>;

  static_assert(!is_nested_field_indirection_v<value_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static meta_type_semantics_data get() {
    meta_type_semantics_data info{};
    info.kind = type_kind::raw_pointer;
    info.wrapper_semantics = wrapper_semantics::inline_value;
    info.value_type = known_type_id<value_type>();
    info.indirect_value = &raw_pointer_value<T>;
    info.mutable_indirect_value = &raw_pointer_mutable_value<T>;
    info.has_indirect_value = &raw_pointer_has_value<T>;
    info.reset_indirect_value = &raw_pointer_reset<T>;
    info.pointer_depth = 1;
    info.nullable = true;
    return info;
  }
};

template <typename Field> [[nodiscard]] meta_type_semantics_data make_meta_type_semantics_data() {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  return type_semantics_traits<field_type>::get();
}

template <typename Field> [[nodiscard]] object_ref_get_fn object_ref_get_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (object_ref_traits<field_type>::is_object_ref) {
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
  if constexpr (object_ref_traits<field_type>::is_object_ref) {
    using object_type = typename object_ref_traits<field_type>::object_type;
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

} // namespace detail

/**
 * @brief 底层字段反射元信息。
 *
 * 字段访问基于注册时记录的 offset 和 C++ 类型 key。调用方需要传入该字段所属对象的地址；
 * 类型不匹配、对象为空或字段不可写时会返回失败，不抛出异常。常规查询优先使用
 * `reflection_field`，该结构主要作为注册表内部存储和底层注册入口。
 */
struct reflection_field_data {
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
    const std::optional<meta_type> meta = caors::find_type(type);
    return meta.has_value() && meta->cpp_type() == detail::cpp_type_key_of<field_type>() &&
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
    const std::optional<meta_type> meta = caors::find_type(type);
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
[[nodiscard]] reflection_field_data
make_reflection_field_data(std::string_view name, std::string_view type_name, std::size_t offset) {
  using field_type = std::remove_cv_t<Field>;
  const meta_type_semantics_data traits = detail::make_meta_type_semantics_data<field_type>();
  const type_id field_type_id = detail::register_field_type_id<field_type>(type_name);
  meta_type_semantics_data stored_traits = traits;
  if ((stored_traits.kind == type_kind::value || stored_traits.kind == type_kind::enumeration) &&
      !stored_traits.value_type) {
    stored_traits.value_type = field_type_id;
  }
  if (!stored_traits.value_type) {
    stored_traits.value_type = field_type_id;
  }

  reflection_field_data field{
      string_name{name},
      field_type_id,
      detail::object_ref_get_fn_of<Field>(),
      detail::object_ref_set_fn_of<Field>(),
      offset,
      !std::is_const_v<Field>,
  };

  if constexpr (sequence_traits<field_type>::is_sequence) {
    (void)detail::register_container_types_for<field_type>(field.type);
  } else if constexpr (set_traits<field_type>::is_set) {
    (void)detail::register_container_types_for<field_type>(field.type);
  } else if constexpr (associative_traits<field_type>::is_associative) {
    (void)detail::register_container_types_for<field_type>(field.type);
  }

  const error traits_status =
      get_meta_registry().emplace<meta_type_semantics_data>(field.type, stored_traits);
  (void)traits_status;

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
