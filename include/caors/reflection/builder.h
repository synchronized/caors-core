// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_BUILDER_H_
#define CAORS_REFLECTION_BUILDER_H_

#include "caors/reflection/registry.h"

#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection {

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

} // namespace caors::reflection

#endif
