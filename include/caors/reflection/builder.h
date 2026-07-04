// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_BUILDER_H_
#define CAORS_REFLECTION_BUILDER_H_

#include "caors/reflection/registry.h"

#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::reflection {

/**
 * @brief RTTR 风格的反射注册构建器。
 *
 * 构建器通过成员指针推导字段大小、对齐和偏移；普通业务字段类型必须已经通过
 * `meta::type_<T>()`
 * 显式注册到全局 meta 注册表。容器、指针、wrapper、pair、tuple 和
 * variant 等结构类型可以通过
 * meta descriptor 描述，不要求自身注册为 meta 类型。
 */
template <typename Class> class class_builder {
public:
  explicit class_builder(std::string_view name, type_id parent_id = type_id::invalid())
      : type_(meta::detail::make_type_data<Class>(name, parent_id)) {}

  class_builder(std::string_view name, type_guid guid, type_id parent_id = type_id::invalid())
      : type_(meta::detail::make_type_data<Class>(name, guid, parent_id)) {}

  explicit class_builder(meta::type type) : existing_type_(type) {
    if (!type.valid()) {
      pending_status_ = {error_domain::reflection, errc::invalid_type};
    }
  }

  template <typename Field> class_builder& field(std::string_view name, Field Class::* member) {
    using field_type = std::remove_cv_t<Field>;

    if (pending_status_) {
      return *this;
    }

    const meta::type resolved_type = caors::meta::find<field_type>();
    const meta::detail::type_descriptor* descriptor = meta::detail::descriptor_ptr_of<field_type>();
    if ((!resolved_type || resolved_type.name().empty()) &&
        (descriptor == nullptr || !descriptor->has_view_capability())) {
      pending_status_ = {error_domain::reflection, errc::invalid_field};
      return *this;
    }

    const std::string_view resolved_name = resolved_type && !resolved_type.name().empty()
                                               ? resolved_type.name().view()
                                               : std::string_view{};
    detail::field_data field =
        detail::make_field_data<Field>(name, resolved_name, detail::member_offset(member));
    if (!field.valid()) {
      pending_status_ = {error_domain::reflection, errc::invalid_field};
      return *this;
    }

    reflection_.fields.push_back(field);
    return *this;
  }

  template <typename Field>
  class_builder& field(std::string_view name, Field Class::* member, std::string_view type_name) {
    if (pending_status_) {
      return *this;
    }

    detail::field_data field =
        detail::make_field_data<Field>(name, type_name, detail::member_offset(member));
    if (!field.valid()) {
      pending_status_ = {error_domain::reflection, errc::invalid_field};
      return *this;
    }

    reflection_.fields.push_back(field);
    return *this;
  }

  [[nodiscard]] error commit() {
    if (pending_status_) {
      return pending_status_;
    }

    if (existing_type_.valid()) {
      return get_registry().register_type(existing_type_, reflection_);
    }

    return get_registry().template register_type_for<Class>(type_, reflection_);
  }

private:
  meta::detail::type_data type_{};
  meta::type existing_type_{};
  detail::type_data reflection_{};
  error pending_status_{};
};

/** @brief 创建反射注册构建器。 */
template <typename Class>
[[nodiscard]] class_builder<Class> class_(std::string_view name,
                                          type_id parent_id = type_id::invalid()) {
  return class_builder<Class>{name, parent_id};
}

/** @brief 创建带稳定 GUID 的反射注册构建器。 */
template <typename Class>
[[nodiscard]] class_builder<Class> class_(std::string_view name, type_guid guid,
                                          type_id parent_id = type_id::invalid()) {
  return class_builder<Class>{name, guid, parent_id};
}

/** @brief 为已注册元类型创建反射注册构建器，不会创建或修改基础类型记录。 */
template <typename Class> [[nodiscard]] class_builder<Class> class_(meta::type type) {
  return class_builder<Class>{type};
}

} // namespace caors::reflection

#endif
