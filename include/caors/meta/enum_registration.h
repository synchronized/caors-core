// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_ENUM_REGISTRATION_H_
#define CAORS_META_ENUM_REGISTRATION_H_

#include "caors/meta/builder.h"
#include "caors/meta/detail/type_data.h"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace caors::meta {

template <typename T> enum_builder<T>& enum_builder<T>::add_enum_value(string_name name, T value) {
  if (!type_.registered() || !name) {
    status_ = {error_domain::meta, errc::invalid_type};
    return *this;
  }

  detail::type_data* type_data = get_registry().find_mutable_type_data(type_.id());
  if (type_data == nullptr) {
    status_ = {error_domain::meta, errc::invalid_type};
    return *this;
  }

  if (type_data->find_enum_value_ptr(name) != nullptr) {
    status_ = {error_domain::meta, errc::duplicate_name};
    return *this;
  }

  using underlying_type = std::underlying_type_t<T>;
  const underlying_type raw = static_cast<underlying_type>(value);
  type_data->add_enum_value(detail::enum_value_data{name, static_cast<std::int64_t>(raw),
                                                    static_cast<std::uint64_t>(raw)});
  status_ = {};
  return *this;
}

template <typename T>
enum_builder<T>& enum_builder<T>::set_enum_value_metadata(string_name value_name, metadata_key key,
                                                          metadata_value value) {
  if (!type_.registered() || !key || !value_name) {
    status_ = {error_domain::meta, errc::invalid_type};
    return *this;
  }

  detail::type_data* type_data = get_registry().find_mutable_type_data(type_.id());
  detail::enum_value_data* enum_value =
      type_data != nullptr ? type_data->mutable_find_enum_value_ptr(value_name) : nullptr;
  if (enum_value == nullptr) {
    status_ = {error_domain::meta, errc::invalid_type};
    return *this;
  }

  enum_value->set_metadata(key, std::move(value));
  status_ = {};
  return *this;
}

} // namespace caors::meta

#endif
