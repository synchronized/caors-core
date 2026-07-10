// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/method.h"

#include "caors/reflection/detail/method_data.h"

namespace caors::reflection {
method::method(const detail::method_data* method) noexcept : method_(method) {}

bool method::valid() const noexcept { return method_ != nullptr && method_->valid(); }

core::string_name method::name() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->name : core::string_name{};
}

meta::type method::get_owner_type() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->get_owner_type() : meta::type{};
}

meta::type method::get_return_type() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->get_return_type() : meta::type{};
}

std::size_t method::parameter_count() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->parameter_count() : 0;
}

std::optional<meta::type> method::get_parameter_type(std::size_t index) const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->get_parameter_type(index) : std::nullopt;
}

bool method::const_method() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr && method->const_method;
}

any method::invoke(void* object, std::span<const any_ref> arguments) const {
  const detail::method_data* method = info();
  if (method == nullptr) {
    return {};
  }

  return method->invoke(object, true, arguments);
}

any method::invoke(const void* object, std::span<const any_ref> arguments) const {
  const detail::method_data* method = info();
  if (method == nullptr) {
    return {};
  }

  return method->invoke(object, false, arguments);
}

const detail::method_data* method::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_;
}

} // namespace caors::reflection
