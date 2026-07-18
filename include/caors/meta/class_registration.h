// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CLASS_REGISTRATION_H_
#define CAORS_META_CLASS_REGISTRATION_H_

#include "caors/meta/builder.h"
#include "caors/meta/detail/type_data.h"
#include "caors/meta/query.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

namespace detail {

template <typename Member> struct member_object_pointer_traits;

template <typename Class, typename Property>
struct member_object_pointer_traits<Property Class::*> {
  using class_type = Class;
  using property_type = Property;
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
property_bind<T> class_builder<T>::property(std::string_view name, Member member) {
  static_assert(std::is_member_object_pointer_v<Member>,
                "meta::class_builder::property requires a data member pointer");
  using traits_type = detail::member_object_pointer_traits<Member>;
  using owner_type = typename traits_type::class_type;
  using property_type = std::remove_cv_t<typename traits_type::property_type>;
  static_assert(std::is_same_v<owner_type, T>,
                "Property member pointer owner must match builder type");

  if (!type_.registered()) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  if (!caors::meta::resolve<property_type>()) {
    status_ = {error_domain::meta, errc::invalid_field};
    return property_bind<T>{*this, {}};
  }

  detail::property_data property =
      detail::make_property_data<T, typename traits_type::property_type>(
          name, std::string_view{}, detail::member_offset(member));
  if (!property.valid()) {
    status_ = {error_domain::meta, errc::invalid_field};
    return property_bind<T>{*this, {}};
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  if (type_data == nullptr) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  if (type_data->find_property_ptr(property.name()) != nullptr) {
    status_ = {error_domain::meta, errc::duplicate_field};
    return property_bind<T>{*this, {}};
  }

  const string_name property_name = property.name();
  type_data->add_property(std::move(property));
  status_ = {};
  return property_bind<T>{*this, property_name};
}

template <typename T>
template <typename Member>
property_bind<T> class_builder<T>::property(std::string_view name, Member member,
                                            std::string_view type_name) {
  static_assert(std::is_member_object_pointer_v<Member>,
                "meta::class_builder::property requires a data member pointer");
  using traits_type = detail::member_object_pointer_traits<Member>;
  using owner_type = typename traits_type::class_type;
  static_assert(std::is_same_v<owner_type, T>,
                "Property member pointer owner must match builder type");

  if (!type_.registered()) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  detail::property_data property =
      detail::make_property_data<T, typename traits_type::property_type>(
          name, type_name, detail::member_offset(member));
  if (!property.valid()) {
    status_ = {error_domain::meta, errc::invalid_field};
    return property_bind<T>{*this, {}};
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  if (type_data == nullptr) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  if (type_data->find_property_ptr(property.name()) != nullptr) {
    status_ = {error_domain::meta, errc::duplicate_field};
    return property_bind<T>{*this, {}};
  }

  const string_name property_name = property.name();
  type_data->add_property(std::move(property));
  status_ = {};
  return property_bind<T>{*this, property_name};
}

template <typename T>
template <typename Getter, typename Setter>
property_bind<T> class_builder<T>::property(std::string_view name, Getter getter, Setter setter)
  requires(!std::is_convertible_v<Setter, std::string_view>)
{
  static_assert(std::is_member_function_pointer_v<Getter>,
                "meta::class_builder::property getter must be a const member function pointer");
  static_assert(std::is_member_function_pointer_v<Setter>,
                "meta::class_builder::property setter must be a member function pointer");

  if (!type_.registered()) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  detail::property_data property = detail::make_accessor_property_data<T>(name, getter, setter);
  if (!property.valid()) {
    status_ = {error_domain::meta, errc::invalid_field};
    return property_bind<T>{*this, {}};
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  if (type_data == nullptr) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  if (type_data->find_property_ptr(property.name()) != nullptr) {
    status_ = {error_domain::meta, errc::duplicate_field};
    return property_bind<T>{*this, {}};
  }

  const string_name property_name = property.name();
  type_data->add_property(std::move(property));
  status_ = {};
  return property_bind<T>{*this, property_name};
}

template <typename T>
template <typename Getter>
property_bind<T> class_builder<T>::property_readonly(std::string_view name, Getter getter) {
  static_assert(std::is_member_function_pointer_v<Getter>,
                "meta::class_builder::property_readonly requires a const member function pointer");

  if (!type_.registered()) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  detail::property_data property = detail::make_readonly_property_data<T>(name, getter);
  if (!property.valid()) {
    status_ = {error_domain::meta, errc::invalid_field};
    return property_bind<T>{*this, {}};
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  if (type_data == nullptr) {
    status_ = {error_domain::meta, errc::invalid_type};
    return property_bind<T>{*this, {}};
  }

  if (type_data->find_property_ptr(property.name()) != nullptr) {
    status_ = {error_domain::meta, errc::duplicate_field};
    return property_bind<T>{*this, {}};
  }

  const string_name property_name = property.name();
  type_data->add_property(std::move(property));
  status_ = {};
  return property_bind<T>{*this, property_name};
}

template <typename T>
template <typename Member>
method_bind<T> class_builder<T>::method(std::string_view name, Member member) {
  static_assert(std::is_member_function_pointer_v<Member>,
                "meta::class_builder::method requires a member function pointer");
  using traits_type = detail::member_function_pointer_traits<Member>;
  using owner_type = typename traits_type::class_type;
  static_assert(std::is_same_v<owner_type, T>,
                "Method member pointer owner must match builder type");

  if (!type_.registered()) {
    status_ = {error_domain::meta, errc::invalid_type};
    return method_bind<T>{*this, {}};
  }

  detail::method_data method = detail::make_method_data<T>(name, member);
  if (!method.valid()) {
    status_ = {error_domain::meta, errc::invalid_field};
    return method_bind<T>{*this, {}};
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  if (type_data == nullptr) {
    status_ = {error_domain::meta, errc::invalid_type};
    return method_bind<T>{*this, {}};
  }

  detail::method_set_data* method_set = type_data->mutable_find_method_ptr(method.name());
  if (method_set != nullptr && method_set->contains_signature(method)) {
    status_ = {error_domain::meta, errc::duplicate_method};
    return method_bind<T>{*this, {}};
  }

  const string_name method_name = method.name();
  const std::optional<std::size_t> overload_index = type_data->add_method(std::move(method));
  if (!overload_index.has_value()) {
    status_ = {error_domain::meta, errc::duplicate_method};
    return method_bind<T>{*this, {}};
  }

  status_ = {};
  return method_bind<T>{*this, method_name, *overload_index};
}

template <typename T>
class_builder<T>& class_builder<T>::set_property_entry(string_name property_name, metadata_key key,
                                                       metadata_value value) {
  if (!type_.registered() || !key || !property_name) {
    status_ = {error_domain::meta, errc::invalid_field};
    return *this;
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  detail::property_data* property =
      type_data != nullptr ? type_data->mutable_find_property_ptr(property_name) : nullptr;
  if (property == nullptr) {
    status_ = {error_domain::meta, errc::invalid_field};
    return *this;
  }

  property->set_metadata(key, std::move(value));
  status_ = {};
  return *this;
}

template <typename T>
class_builder<T>&
class_builder<T>::set_property_schema_entry(string_name property_name,
                                            registration::property_schema_entry entry) {
  if (!type_.registered() || !property_name) {
    status_ = {error_domain::meta, errc::invalid_field};
    return *this;
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  detail::property_data* property =
      type_data != nullptr ? type_data->mutable_find_property_ptr(property_name) : nullptr;
  if (property == nullptr) {
    status_ = {error_domain::meta, errc::invalid_field};
    return *this;
  }

  property->apply_schema_entry(std::move(entry));
  status_ = {};
  return *this;
}

template <typename T>
class_builder<T>& class_builder<T>::set_method_metadata(string_name method_name,
                                                        std::size_t overload_index,
                                                        metadata_key key, metadata_value value) {
  if (!type_.registered() || !key || !method_name ||
      overload_index == static_cast<std::size_t>(-1)) {
    status_ = {error_domain::meta, errc::invalid_field};
    return *this;
  }

  detail::type_data* type_data = registry_ref().find_mutable_type_data(type_.id());
  detail::method_set_data* method_set =
      type_data != nullptr ? type_data->mutable_find_method_ptr(method_name) : nullptr;
  detail::method_data* method =
      method_set != nullptr ? method_set->mutable_overload_at(overload_index) : nullptr;
  if (method == nullptr) {
    status_ = {error_domain::meta, errc::invalid_field};
    return *this;
  }

  method->set_metadata(key, std::move(value));
  status_ = {};
  return *this;
}

} // namespace caors::meta

#endif
