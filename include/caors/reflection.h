// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_H_
#define CAORS_REFLECTION_H_

#include "caors/export.h"
#include "caors/meta.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
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
  smart_pointer,
  optional,
  wrapper,
};

/** @brief 字段包装或指针的所有权语义。 */
enum class field_ownership {
  none,
  owning,
  shared,
  weak,
};

namespace detail {

template <typename T> struct is_unique_ptr : std::false_type {};

template <typename T, typename Deleter>
struct is_unique_ptr<std::unique_ptr<T, Deleter>> : std::true_type {};

template <typename T> struct is_shared_ptr : std::false_type {};

template <typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template <typename T> struct is_weak_ptr : std::false_type {};

template <typename T> struct is_weak_ptr<std::weak_ptr<T>> : std::true_type {};

template <typename T> struct is_optional : std::false_type {};

template <typename T> struct is_optional<std::optional<T>> : std::true_type {};

template <typename T> struct is_reference_wrapper : std::false_type {};

template <typename T> struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_supported_wrapper_v =
    is_unique_ptr<std::remove_cv_t<T>>::value || is_shared_ptr<std::remove_cv_t<T>>::value ||
    is_weak_ptr<std::remove_cv_t<T>>::value || is_optional<std::remove_cv_t<T>>::value ||
    is_reference_wrapper<std::remove_cv_t<T>>::value;

template <typename T>
inline constexpr bool is_nested_field_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

template <typename T> struct dependent_false : std::false_type {};

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
  field_ownership ownership = field_ownership::none;
  type_id value_type{};
  type_id pointed_type{};
  type_id wrapped_type{};
  cpp_type_key value_cpp_type = nullptr;
  cpp_type_key pointed_cpp_type = nullptr;
  cpp_type_key wrapped_cpp_type = nullptr;
  std::uint8_t pointer_depth = 0;
  bool nullable = false;
};

template <typename Field> struct field_traits {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  using value_type = field_type;

  [[nodiscard]] static field_traits_info get() {
    return field_traits_info{
        std::is_enum_v<value_type> ? field_kind::enumeration : field_kind::value,
        field_ownership::none,
        known_type_id<value_type>(),
        type_id::invalid(),
        type_id::invalid(),
        cpp_type_key_of<value_type>(),
        nullptr,
        nullptr,
        0,
        false,
    };
  }
};

template <typename T> struct field_traits<T*> {
  using pointed_type = std::remove_cv_t<T>;
  using value_type = pointed_type;

  static_assert(!is_nested_field_indirection_v<pointed_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static field_traits_info get() {
    return field_traits_info{
        field_kind::raw_pointer,
        field_ownership::none,
        known_type_id<value_type>(),
        known_type_id<pointed_type>(),
        type_id::invalid(),
        cpp_type_key_of<value_type>(),
        cpp_type_key_of<pointed_type>(),
        nullptr,
        1,
        true,
    };
  }
};

template <typename T, typename Deleter> struct field_traits<std::unique_ptr<T, Deleter>> {
  using wrapped_type = std::remove_cv_t<T>;
  using value_type = wrapped_type;

  static_assert(!is_nested_field_indirection_v<wrapped_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static field_traits_info get() {
    return field_traits_info{
        field_kind::smart_pointer,
        field_ownership::owning,
        known_type_id<value_type>(),
        type_id::invalid(),
        known_type_id<wrapped_type>(),
        cpp_type_key_of<value_type>(),
        nullptr,
        cpp_type_key_of<wrapped_type>(),
        0,
        true,
    };
  }
};

template <typename T> struct field_traits<std::shared_ptr<T>> {
  using wrapped_type = std::remove_cv_t<T>;
  using value_type = wrapped_type;

  static_assert(!is_nested_field_indirection_v<wrapped_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static field_traits_info get() {
    return field_traits_info{
        field_kind::smart_pointer,
        field_ownership::shared,
        known_type_id<value_type>(),
        type_id::invalid(),
        known_type_id<wrapped_type>(),
        cpp_type_key_of<value_type>(),
        nullptr,
        cpp_type_key_of<wrapped_type>(),
        0,
        true,
    };
  }
};

template <typename T> struct field_traits<std::weak_ptr<T>> {
  using wrapped_type = std::remove_cv_t<T>;
  using value_type = wrapped_type;

  static_assert(!is_nested_field_indirection_v<wrapped_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static field_traits_info get() {
    return field_traits_info{
        field_kind::smart_pointer,
        field_ownership::weak,
        known_type_id<value_type>(),
        type_id::invalid(),
        known_type_id<wrapped_type>(),
        cpp_type_key_of<value_type>(),
        nullptr,
        cpp_type_key_of<wrapped_type>(),
        0,
        true,
    };
  }
};

template <typename T> struct field_traits<std::optional<T>> {
  using wrapped_type = std::remove_cv_t<T>;
  using value_type = wrapped_type;

  static_assert(!is_nested_field_indirection_v<wrapped_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static field_traits_info get() {
    return field_traits_info{
        field_kind::optional,
        field_ownership::none,
        known_type_id<value_type>(),
        type_id::invalid(),
        known_type_id<wrapped_type>(),
        cpp_type_key_of<value_type>(),
        nullptr,
        cpp_type_key_of<wrapped_type>(),
        0,
        true,
    };
  }
};

template <typename T> struct field_traits<std::reference_wrapper<T>> {
  using wrapped_type = std::remove_cv_t<T>;
  using value_type = wrapped_type;

  static_assert(!is_nested_field_indirection_v<wrapped_type>,
                "CAORS reflection supports at most one pointer or wrapper level");

  [[nodiscard]] static field_traits_info get() {
    return field_traits_info{
        field_kind::wrapper,
        field_ownership::none,
        known_type_id<value_type>(),
        type_id::invalid(),
        known_type_id<wrapped_type>(),
        cpp_type_key_of<value_type>(),
        nullptr,
        cpp_type_key_of<wrapped_type>(),
        0,
        false,
    };
  }
};

template <typename Field> [[nodiscard]] field_traits_info make_field_traits_info() {
  using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
  return field_traits<field_type>::get();
}

} // namespace detail

/**
 * @brief 字段反射元信息。
 *
 * 字段访问基于注册时记录的 offset 和 C++ 类型 key。调用方需要传入该字段所属对象的地址；
 * 类型不匹配、对象为空或字段不可写时会返回失败，不抛出异常。
 */
struct field_info {
  string_name name{};
  type_id type{};
  string_name type_name{};
  detail::cpp_type_key cpp_type = nullptr;
  type_id value_type{};
  type_id pointed_type{};
  type_id wrapped_type{};
  detail::cpp_type_key value_cpp_type = nullptr;
  detail::cpp_type_key pointed_cpp_type = nullptr;
  detail::cpp_type_key wrapped_cpp_type = nullptr;
  field_kind kind = field_kind::value;
  field_ownership ownership = field_ownership::none;
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

    const void* value = data(object);
    if (value == nullptr) {
      return std::nullopt;
    }

    return *static_cast<const field_type*>(value);
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
      traits.pointed_type,
      traits.wrapped_type,
      traits.value_cpp_type,
      traits.pointed_cpp_type,
      traits.wrapped_cpp_type,
      traits.kind,
      traits.ownership,
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

/**
 * @brief 类型反射元信息。
 *
 * `reflection_type` 复用基础 `type_info` 表示类型身份，并保存该类型的字段列表。
 */
struct reflection_type {
  type_info info{};
  std::vector<field_info> fields{};

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] std::optional<field_info> find_field(string_name name) const;
  [[nodiscard]] std::optional<field_info> find_field(std::string_view name) const;
};

/** @brief 创建类型反射描述。 */
template <typename T>
[[nodiscard]] reflection_type make_reflection_type(std::string_view name,
                                                   std::initializer_list<field_info> fields,
                                                   type_id parent_id = type_id::invalid()) {
  return reflection_type{
      make_type_info<T>(name, parent_id),
      std::vector<field_info>{fields},
  };
}

/** @brief 创建带稳定 GUID 的类型反射描述。 */
template <typename T>
[[nodiscard]] reflection_type make_reflection_type(std::string_view name, type_guid guid,
                                                   std::initializer_list<field_info> fields,
                                                   type_id parent_id = type_id::invalid()) {
  return reflection_type{
      make_type_info<T>(name, guid, parent_id),
      std::vector<field_info>{fields},
  };
}

/** @brief 反射类型注册结果。 */
enum class reflection_register_result {
  success,
  invalid_type,
  invalid_field,
  duplicate_id,
  duplicate_name,
  duplicate_guid,
  duplicate_field,
  duplicate_reflection,
  inconsistent_type,
  missing_parent,
};

/** @brief 判断反射类型注册是否成功。 */
[[nodiscard]] constexpr bool
reflection_register_succeeded(reflection_register_result result) noexcept {
  return result == reflection_register_result::success;
}

class reflection_registry_state;

/**
 * @brief 反射注册表。
 *
 * 注册表基于 `meta_registry` 保存基础类型信息，并把 `reflection_type` 作为元数据组件挂载到同一个
 * `type_id` 上。
 */
class CAORS_API reflection_registry final {
public:
  reflection_registry();
  ~reflection_registry();

  reflection_registry(const reflection_registry&) = delete;
  reflection_registry& operator=(const reflection_registry&) = delete;

  [[nodiscard]] reflection_register_result register_type(const reflection_type& type);
  template <typename T>
  [[nodiscard]] reflection_register_result register_type_for(const reflection_type& type) {
    return register_type(detail::cpp_type_key_of<T>(), type);
  }

  [[nodiscard]] bool contains(type_id id) const;
  template <typename T> [[nodiscard]] std::optional<type_info> find_registered_type() const {
    return find_registered_type(detail::cpp_type_key_of<T>());
  }

  [[nodiscard]] std::optional<reflection_type> find_type(type_id id) const;
  [[nodiscard]] std::optional<reflection_type> find_type(type_guid guid) const;
  [[nodiscard]] std::optional<reflection_type> find_type(std::string_view name) const;
  [[nodiscard]] std::optional<reflection_type> find_type(string_name name) const;
  [[nodiscard]] std::size_t size() const noexcept;
  void clear() noexcept;

private:
  [[nodiscard]] reflection_register_result register_type(detail::cpp_type_key key,
                                                         const reflection_type& type);
  [[nodiscard]] std::optional<type_info> find_registered_type(detail::cpp_type_key key) const;

  reflection_registry_state* state_ = nullptr;
};

namespace reflection {

/**
 * @brief RTTR 风格的反射注册构建器。
 *
 * 构建器通过成员指针推导字段大小、对齐和偏移；字段类型会优先使用同一注册表中已注册的 C++
 * 类型映射，未注册时再使用 `type_name<T>` 默认映射。
 */
template <typename Class> class class_builder {
public:
  class_builder(reflection_registry& registry, std::string_view name,
                type_id parent_id = type_id::invalid())
      : registry_(&registry), type_(make_reflection_type<Class>(name, {}, parent_id)) {}

  class_builder(reflection_registry& registry, std::string_view name, type_guid guid,
                type_id parent_id = type_id::invalid())
      : registry_(&registry), type_(make_reflection_type<Class>(name, guid, {}, parent_id)) {}

  template <typename Field> class_builder& field(std::string_view name, Field Class::* member) {
    using field_type = std::remove_cv_t<Field>;
    (void)detail::make_field_traits_info<field_type>();

    if (pending_result_ != reflection_register_result::success) {
      return *this;
    }

    const std::optional<type_info> resolved_type =
        registry_->template find_registered_type<field_type>();
    if (resolved_type.has_value()) {
      field_info field =
          make_field_info<Field>(name, resolved_type->name.view(), detail::member_offset(member));
      field.type = resolved_type->id;
      field.type_name = resolved_type->name;
      if (field.kind == field_kind::value || field.kind == field_kind::enumeration) {
        field.value_type = resolved_type->id;
      }
      type_.fields.push_back(field);
    } else {
      if constexpr (detail::has_type_name_v<field_type>) {
        type_.fields.push_back(make_field_info<Field>(name, type_name<field_type>::value,
                                                      detail::member_offset(member)));
      } else {
        pending_result_ = reflection_register_result::invalid_field;
      }
    }

    return *this;
  }

  template <typename Field>
  class_builder& field(std::string_view name, Field Class::* member, std::string_view type_name) {
    if (pending_result_ != reflection_register_result::success) {
      return *this;
    }

    field_info field = make_field_info<Field>(name, type_name, detail::member_offset(member));
    if (field.kind == field_kind::value || field.kind == field_kind::enumeration) {
      field.value_type = field.type;
    }
    type_.fields.push_back(field);
    return *this;
  }

  [[nodiscard]] reflection_register_result commit() {
    if (pending_result_ != reflection_register_result::success) {
      return pending_result_;
    }

    return registry_->template register_type_for<Class>(type_);
  }

  [[nodiscard]] const reflection_type& type() const noexcept { return type_; }

private:
  reflection_registry* registry_ = nullptr;
  reflection_type type_{};
  reflection_register_result pending_result_ = reflection_register_result::success;
};

/** @brief 创建反射注册构建器。 */
template <typename Class>
[[nodiscard]] class_builder<Class> class_(reflection_registry& registry, std::string_view name,
                                          type_id parent_id = type_id::invalid()) {
  return class_builder<Class>{registry, name, parent_id};
}

/** @brief 创建带稳定 GUID 的反射注册构建器。 */
template <typename Class>
[[nodiscard]] class_builder<Class> class_(reflection_registry& registry, std::string_view name,
                                          type_guid guid, type_id parent_id = type_id::invalid()) {
  return class_builder<Class>{registry, name, guid, parent_id};
}

} // namespace reflection

/** @brief 获取进程内默认反射注册表。 */
[[nodiscard]] CAORS_API reflection_registry& get_reflection_registry() noexcept;

/** @brief 向指定注册表注册反射类型。 */
template <typename T>
[[nodiscard]] reflection_register_result
register_reflection(reflection_registry& registry, std::string_view name,
                    std::initializer_list<field_info> fields,
                    type_id parent_id = type_id::invalid()) {
  return registry.register_type_for<T>(make_reflection_type<T>(name, fields, parent_id));
}

/** @brief 向指定注册表注册带稳定 GUID 的反射类型。 */
template <typename T>
[[nodiscard]] reflection_register_result
register_reflection(reflection_registry& registry, std::string_view name, type_guid guid,
                    std::initializer_list<field_info> fields,
                    type_id parent_id = type_id::invalid()) {
  return registry.register_type_for<T>(make_reflection_type<T>(name, guid, fields, parent_id));
}

/** @brief 向默认注册表注册反射类型。 */
template <typename T>
[[nodiscard]] reflection_register_result
register_reflection(std::string_view name, std::initializer_list<field_info> fields,
                    type_id parent_id = type_id::invalid()) {
  return register_reflection<T>(get_reflection_registry(), name, fields, parent_id);
}

/** @brief 向默认注册表注册带稳定 GUID 的反射类型。 */
template <typename T>
[[nodiscard]] reflection_register_result
register_reflection(std::string_view name, type_guid guid, std::initializer_list<field_info> fields,
                    type_id parent_id = type_id::invalid()) {
  return register_reflection<T>(get_reflection_registry(), name, guid, fields, parent_id);
}

/** @brief 在默认注册表中查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(type_id id);

/** @brief 在默认注册表中按类型名查找反射类型。 */
[[nodiscard]] CAORS_API std::optional<reflection_type> find_reflection(std::string_view name);

}

#endif
