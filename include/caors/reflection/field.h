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
#include <vector>

namespace caors {

/** @brief 字段类型形态。 */
enum class field_kind {
  value,
  enumeration,
  raw_pointer,
  wrapper,
  sequence,
};

/**
 * @brief 顺序容器反射 traits。
 *
 * 自定义顺序容器可以特化该模板接入反射系统。traits 负责提供元素类型、类型名前缀和基础访问函数。
 */
template <typename T> struct sequence_traits {
  static constexpr bool is_sequence = false;
};

template <typename T, typename Allocator> struct sequence_traits<std::vector<T, Allocator>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    if constexpr (std::is_same_v<Allocator, allocator<T>>) {
      return "caors.vector";
    } else {
      return "std.vector";
    }
  }

  [[nodiscard]] static std::size_t size(const std::vector<T, Allocator>& values) noexcept {
    return values.size();
  }

  [[nodiscard]] static const element_type* element(const std::vector<T, Allocator>& values,
                                                   std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(std::vector<T, Allocator>& values,
                                                     std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  static void clear(std::vector<T, Allocator>& values) { values.clear(); }

  [[nodiscard]] static bool resize(std::vector<T, Allocator>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool append(std::vector<T, Allocator>& values, const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

namespace detail {

using object_ref_get_fn = object* (*)(const void*) noexcept;
using object_ref_set_fn = bool (*)(void*, object*) noexcept;
using sequence_size_fn = std::size_t (*)(const void*) noexcept;
using sequence_const_element_fn = const void* (*)(const void*, std::size_t) noexcept;
using sequence_mutable_element_fn = void* (*)(void*, std::size_t) noexcept;
using sequence_clear_fn = void (*)(void*) noexcept;
using sequence_resize_fn = bool (*)(void*, std::size_t);
using sequence_append_fn = bool (*)(void*, const void*);

template <typename T>
inline constexpr bool is_supported_wrapper_v = wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool is_nested_field_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

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

struct field_traits_info {
  field_kind kind = field_kind::value;
  field_wrapper_semantics wrapper_semantics = field_wrapper_semantics::none;
  type_id value_type{};
  type_id element_type{};
  std::uint8_t pointer_depth = 0;
  bool nullable = false;
};

template <typename Field> struct field_traits {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;

  [[nodiscard]] static field_traits_info get() {
    field_traits_info info{};
    if constexpr (sequence_traits<field_type>::is_sequence) {
      using element_type = typename sequence_traits<field_type>::element_type;

      info.kind = field_kind::sequence;
      info.value_type = known_type_id<field_type>();
      info.element_type = known_type_id<element_type>();
      return info;
    } else if constexpr (wrapper_traits<field_type>::is_wrapper) {
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

template <typename Field> [[nodiscard]] sequence_size_fn sequence_size_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (sequence_traits<field_type>::is_sequence) {
    return [](const void* storage) noexcept -> std::size_t {
      if (storage == nullptr) {
        return 0;
      }

      return sequence_traits<field_type>::size(*static_cast<const field_type*>(storage));
    };
  } else {
    return nullptr;
  }
}

template <typename Field>
[[nodiscard]] sequence_const_element_fn sequence_const_element_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (sequence_traits<field_type>::is_sequence) {
    return [](const void* storage, std::size_t index) noexcept -> const void* {
      if (storage == nullptr) {
        return nullptr;
      }

      return sequence_traits<field_type>::element(*static_cast<const field_type*>(storage), index);
    };
  } else {
    return nullptr;
  }
}

template <typename Field>
[[nodiscard]] sequence_mutable_element_fn sequence_mutable_element_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (sequence_traits<field_type>::is_sequence) {
    return [](void* storage, std::size_t index) noexcept -> void* {
      if (storage == nullptr) {
        return nullptr;
      }

      return sequence_traits<field_type>::mutable_element(*static_cast<field_type*>(storage),
                                                          index);
    };
  } else {
    return nullptr;
  }
}

template <typename Field> [[nodiscard]] sequence_clear_fn sequence_clear_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (sequence_traits<field_type>::is_sequence) {
    return [](void* storage) noexcept {
      if (storage != nullptr) {
        sequence_traits<field_type>::clear(*static_cast<field_type*>(storage));
      }
    };
  } else {
    return nullptr;
  }
}

template <typename Field> [[nodiscard]] sequence_resize_fn sequence_resize_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (sequence_traits<field_type>::is_sequence) {
    return [](void* storage, std::size_t size) -> bool {
      if (storage == nullptr) {
        return false;
      }

      return sequence_traits<field_type>::resize(*static_cast<field_type*>(storage), size);
    };
  } else {
    return nullptr;
  }
}

template <typename Field> [[nodiscard]] sequence_append_fn sequence_append_fn_of() noexcept {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  if constexpr (sequence_traits<field_type>::is_sequence) {
    using element_type = typename sequence_traits<field_type>::element_type;
    return [](void* storage, const void* element) -> bool {
      if (storage == nullptr || element == nullptr) {
        return false;
      }

      return sequence_traits<field_type>::append(*static_cast<field_type*>(storage),
                                                 *static_cast<const element_type*>(element));
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
  string_name type_name{};
  detail::cpp_type_key cpp_type = nullptr;
  type_id value_type{};
  type_id element_type{};
  string_name element_type_name{};
  detail::cpp_type_key element_cpp_type = nullptr;
  std::size_t element_size = 0;
  std::size_t element_alignment = 0;
  detail::value_clone_fn clone_element = nullptr;
  detail::value_destroy_fn destroy_element = nullptr;
  detail::value_assign_fn assign_element = nullptr;
  detail::value_clone_fn clone_value = nullptr;
  detail::value_destroy_fn destroy_value = nullptr;
  detail::value_assign_fn assign_value = nullptr;
  detail::object_ref_get_fn get_object_ref = nullptr;
  detail::object_ref_set_fn set_object_ref = nullptr;
  detail::sequence_size_fn sequence_size = nullptr;
  detail::sequence_const_element_fn sequence_const_element = nullptr;
  detail::sequence_mutable_element_fn sequence_mutable_element = nullptr;
  detail::sequence_clear_fn sequence_clear = nullptr;
  detail::sequence_resize_fn sequence_resize = nullptr;
  detail::sequence_append_fn sequence_append = nullptr;
  field_kind kind = field_kind::value;
  field_wrapper_semantics wrapper_semantics = field_wrapper_semantics::none;
  std::uint8_t pointer_depth = 0;
  bool nullable = false;
  std::size_t offset = 0;
  std::size_t size = 0;
  std::size_t alignment = 0;
  bool writable = true;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return !name.empty() && !type_name.empty() && size > 0 && alignment > 0;
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
  [[nodiscard]] any get_value(const void* object) const {
    const void* field_value = data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any::copy_from_raw(type, type_name, cpp_type, field_value, size, alignment, clone_value,
                              destroy_value, assign_value);
  }

  /** @brief 获取字段值的只读运行期引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const {
    const void* field_value = data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, type_name, cpp_type, field_value, size, alignment, clone_value,
                             destroy_value, assign_value, false);
  }

  /** @brief 获取字段值的可写运行期引用。 */
  [[nodiscard]] any_ref get_mutable_ref(void* object) const {
    void* field_value = mutable_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, type_name, cpp_type, field_value, size, alignment, clone_value,
                             destroy_value, assign_value, true);
  }

  /** @brief 获取顺序容器字段元素只读运行期引用。 */
  [[nodiscard]] any_ref get_sequence_element_ref(const void* object, std::size_t index) const {
    if (kind != field_kind::sequence || sequence_const_element == nullptr || !element_type ||
        element_type_name.empty() || element_cpp_type == nullptr) {
      return {};
    }

    const void* field_value = data(object);
    const void* element = sequence_const_element(field_value, index);
    if (element == nullptr) {
      return {};
    }

    return any_ref::from_raw(element_type, element_type_name, element_cpp_type, element,
                             element_size, element_alignment, clone_element, destroy_element,
                             assign_element, false);
  }

  /** @brief 获取顺序容器字段元素可写运行期引用。 */
  [[nodiscard]] any_ref get_sequence_element_mutable_ref(void* object, std::size_t index) const {
    if (kind != field_kind::sequence || sequence_mutable_element == nullptr || !element_type ||
        element_type_name.empty() || element_cpp_type == nullptr) {
      return {};
    }

    void* field_value = mutable_data(object);
    void* element = sequence_mutable_element(field_value, index);
    if (element == nullptr) {
      return {};
    }

    return any_ref::from_raw(element_type, element_type_name, element_cpp_type, element,
                             element_size, element_alignment, clone_element, destroy_element,
                             assign_element, true);
  }

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const {
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
[[nodiscard]] reflection_field_data
make_reflection_field_data(std::string_view name, std::string_view type_name, std::size_t offset) {
  using field_type = std::remove_cv_t<Field>;
  const detail::field_traits_info traits = detail::make_field_traits_info<field_type>();
  reflection_field_data field{
      string_name{name},
      detail::register_field_type_id<field_type>(type_name),
      string_name{type_name},
      detail::cpp_type_key_of<field_type>(),
      traits.value_type,
      traits.element_type,
      {},
      nullptr,
      0,
      0,
      nullptr,
      nullptr,
      nullptr,
      detail::value_clone_fn_of<Field>(),
      detail::value_destroy_fn_of<Field>(),
      detail::value_assign_fn_of<Field>(),
      detail::object_ref_get_fn_of<Field>(),
      detail::object_ref_set_fn_of<Field>(),
      detail::sequence_size_fn_of<Field>(),
      detail::sequence_const_element_fn_of<Field>(),
      detail::sequence_mutable_element_fn_of<Field>(),
      detail::sequence_clear_fn_of<Field>(),
      detail::sequence_resize_fn_of<Field>(),
      detail::sequence_append_fn_of<Field>(),
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

  if constexpr (sequence_traits<field_type>::is_sequence) {
    using element_type = typename sequence_traits<field_type>::element_type;
    field.element_cpp_type = detail::cpp_type_key_of<element_type>();
    field.element_size = sizeof(element_type);
    field.element_alignment = alignof(element_type);
    field.clone_element = detail::value_clone_fn_of<element_type>();
    field.destroy_element = detail::value_destroy_fn_of<element_type>();
    field.assign_element = detail::value_assign_fn_of<element_type>();
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
