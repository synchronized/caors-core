// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_H_
#define CAORS_REFLECTION_H_

#include "caors/export.h"
#include "caors/meta.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

namespace caors {

/**
 * @brief 字段反射元信息。
 *
 * 第一版字段反射只描述字段身份、字段类型名、布局位置和大小，不负责动态 get/set。
 */
struct field_info {
  string_name name{};
  type_id type{};
  string_name type_name{};
  std::size_t offset = 0;
  std::size_t size = 0;
  std::size_t alignment = 0;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return !name.empty() && static_cast<bool>(type) && !type_name.empty() && size > 0 &&
           alignment > 0;
  }
};

/** @brief 创建字段反射元信息。 */
template <typename Field>
[[nodiscard]] field_info make_field_info(std::string_view name, std::string_view type_name,
                                         std::size_t offset) {
  using field_type = std::remove_cv_t<Field>;
  return field_info{
      string_name{name},  make_type_id(type_name), string_name{type_name}, offset,
      sizeof(field_type), alignof(field_type),
  };
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
  hash_collision,
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

    if (pending_result_ != reflection_register_result::success) {
      return *this;
    }

    std::optional<type_info> resolved_type = registry_->template find_registered_type<field_type>();
    if (!resolved_type.has_value()) {
      if constexpr (detail::has_type_name_v<field_type>) {
        resolved_type = make_type_info<field_type>(type_name<field_type>::value);
      } else {
        pending_result_ = reflection_register_result::invalid_field;
        return *this;
      }
    }

    type_.fields.push_back(field_info{
        string_name{name},
        resolved_type->id,
        resolved_type->name,
        detail::member_offset(member),
        sizeof(field_type),
        alignof(field_type),
    });
    return *this;
  }

  template <typename Field>
  class_builder& field(std::string_view name, Field Class::* member, std::string_view type_name) {
    using field_type = std::remove_cv_t<Field>;

    if (pending_result_ != reflection_register_result::success) {
      return *this;
    }

    type_.fields.push_back(field_info{
        string_name{name},
        make_type_id(type_name),
        string_name{type_name},
        detail::member_offset(member),
        sizeof(field_type),
        alignof(field_type),
    });
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
