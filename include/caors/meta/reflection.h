// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REFLECTION_H_
#define CAORS_META_REFLECTION_H_

#include "caors/meta/builder.h"
#include "caors/meta/detail/type_data.h"
#include "caors/meta/query.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

namespace detail {

template <typename Member> struct member_object_pointer_traits;

template <typename Class, typename Field> struct member_object_pointer_traits<Field Class::*> {
  using class_type = Class;
  using field_type = Field;
};

template <typename Member> struct member_function_pointer_traits;

template <typename Class, typename Return, typename... Arguments>
struct member_function_pointer_traits<Return (Class::*)(Arguments...)> {
  using class_type = Class;
  using return_type = Return;
  static constexpr bool is_const = false;
};

template <typename Class, typename Return, typename... Arguments>
struct member_function_pointer_traits<Return (Class::*)(Arguments...) const> {
  using class_type = Class;
  using return_type = Return;
  static constexpr bool is_const = true;
};

} // namespace detail

template <typename T>
template <typename Member>
class_builder<T>& class_builder<T>::field(std::string_view name, Member member) {
  static_assert(std::is_member_object_pointer_v<Member>,
                "meta::class_builder::field requires a data member pointer");
  using traits_type = detail::member_object_pointer_traits<Member>;
  using owner_type = typename traits_type::class_type;
  using field_type = std::remove_cv_t<typename traits_type::field_type>;
  static_assert(std::is_same_v<owner_type, T>,
                "Field member pointer owner must match builder type");

  if (!type_.registered()) {
    status_ = {error_domain::reflection, errc::invalid_type};
    return *this;
  }

  if (!caors::meta::resolve<field_type>()) {
    status_ = {error_domain::reflection, errc::invalid_field};
    return *this;
  }

  detail::field_data field = detail::make_field_data<T, typename traits_type::field_type>(
      name, std::string_view{}, detail::member_offset(member));
  if (!field.valid()) {
    status_ = {error_domain::reflection, errc::invalid_field};
    return *this;
  }

  detail::type_data* reflection =
      get_registry().ensure_reflection(type_.id(), detail::make_reflection_type_ops<T>());
  if (reflection == nullptr) {
    status_ = {error_domain::reflection, errc::invalid_type};
    return *this;
  }

  if (reflection->find_field_ptr(field.name()) != nullptr) {
    status_ = {error_domain::reflection, errc::duplicate_field};
    return *this;
  }

  last_field_name_ = field.name();
  reflection->add_field(std::move(field));
  status_ = {};
  return *this;
}

template <typename T>
template <typename Member>
class_builder<T>& class_builder<T>::field(std::string_view name, Member member,
                                          std::string_view type_name) {
  static_assert(std::is_member_object_pointer_v<Member>,
                "meta::class_builder::field requires a data member pointer");
  using traits_type = detail::member_object_pointer_traits<Member>;
  using owner_type = typename traits_type::class_type;
  static_assert(std::is_same_v<owner_type, T>,
                "Field member pointer owner must match builder type");

  if (!type_.registered()) {
    status_ = {error_domain::reflection, errc::invalid_type};
    return *this;
  }

  detail::field_data field = detail::make_field_data<T, typename traits_type::field_type>(
      name, type_name, detail::member_offset(member));
  if (!field.valid()) {
    status_ = {error_domain::reflection, errc::invalid_field};
    return *this;
  }

  detail::type_data* reflection =
      get_registry().ensure_reflection(type_.id(), detail::make_reflection_type_ops<T>());
  if (reflection == nullptr) {
    status_ = {error_domain::reflection, errc::invalid_type};
    return *this;
  }

  if (reflection->find_field_ptr(field.name()) != nullptr) {
    status_ = {error_domain::reflection, errc::duplicate_field};
    return *this;
  }

  last_field_name_ = field.name();
  reflection->add_field(std::move(field));
  status_ = {};
  return *this;
}

template <typename T>
template <typename Member>
class_builder<T>& class_builder<T>::method(std::string_view name, Member member) {
  static_assert(std::is_member_function_pointer_v<Member>,
                "meta::class_builder::method requires a member function pointer");
  using traits_type = detail::member_function_pointer_traits<Member>;
  using owner_type = typename traits_type::class_type;
  static_assert(std::is_same_v<owner_type, T>,
                "Method member pointer owner must match builder type");

  if (!type_.registered()) {
    status_ = {error_domain::reflection, errc::invalid_type};
    return *this;
  }

  detail::method_data method = detail::make_method_data<T>(name, member);
  if (!method.valid()) {
    status_ = {error_domain::reflection, errc::invalid_field};
    return *this;
  }

  detail::type_data* reflection =
      get_registry().ensure_reflection(type_.id(), detail::make_reflection_type_ops<T>());
  if (reflection == nullptr) {
    status_ = {error_domain::reflection, errc::invalid_type};
    return *this;
  }

  if (reflection->find_method_ptr(method.name()) != nullptr) {
    status_ = {error_domain::reflection, errc::duplicate_method};
    return *this;
  }

  reflection->add_method(std::move(method));
  status_ = {};
  return *this;
}

template <typename T>
template <typename Value>
class_builder<T>& class_builder<T>::field_metadata(std::string_view key, Value&& value) {
  return field_metadata(string_name{key}, std::forward<Value>(value));
}

template <typename T>
template <typename Value>
class_builder<T>& class_builder<T>::field_metadata(string_name key, Value&& value) {
  if (!type_.registered() || !key || !last_field_name_) {
    status_ = {error_domain::reflection, errc::invalid_field};
    return *this;
  }

  detail::type_data* reflection = get_registry().find_mutable_reflection(type_.id());
  detail::field_data* field =
      reflection != nullptr ? reflection->mutable_find_field_ptr(last_field_name_) : nullptr;
  if (field == nullptr) {
    status_ = {error_domain::reflection, errc::invalid_field};
    return *this;
  }

  field->set_metadata(key, meta::make_metadata_value(std::forward<Value>(value)));
  status_ = {};
  return *this;
}

} // namespace caors::meta

#endif
