// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_MODULE_TEMPLATES_H_
#define CAORS_META_DETAIL_MODULE_TEMPLATES_H_

#include "caors/meta/detail/module_data.h"
#include "caors/meta/detail/method_model.h"
#include "caors/meta/detail/property_model.h"

namespace caors::meta {

template <typename Property>
module_builder& module_builder::property(std::string_view name, Property* value) {
  if (status_) {
    return *this;
  }

  detail::property_data property = detail::make_global_property_data(name, value);
  if (!property.valid()) {
    status_ = {error_domain::meta, errc::invalid_argument};
    return *this;
  }

  status_ = commit_property(std::move(property));
  refresh();
  return *this;
}

template <typename Return, typename... Arguments>
module_builder& module_builder::function(std::string_view name, Return (*function)(Arguments...)) {
  if (status_) {
    return *this;
  }

  detail::method_data method = detail::make_global_method_data(name, function);
  if (!method.valid()) {
    status_ = {error_domain::meta, errc::invalid_argument};
    return *this;
  }

  status_ = commit_method(std::move(method));
  refresh();
  return *this;
}

template <typename Callable>
  requires(!std::is_pointer_v<std::decay_t<Callable>> &&
           requires { typename detail::callable_signature_t<std::decay_t<Callable>>; })
module_builder& module_builder::function(std::string_view name, Callable&& callable) {
  if (status_) {
    return *this;
  }

  detail::method_data method =
      detail::make_global_callable_method_data(name, std::forward<Callable>(callable));
  if (!method.valid()) {
    status_ = {error_domain::meta, errc::invalid_argument};
    return *this;
  }

  status_ = commit_method(std::move(method));
  refresh();
  return *this;
}

} // namespace caors::meta

#endif
