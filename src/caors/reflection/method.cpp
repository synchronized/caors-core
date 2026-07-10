// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/method.h"

#include "caors/reflection/detail/method_wrapper.h"

namespace caors::reflection {
namespace {

[[nodiscard]] const detail::method_wrapper* as_method_wrapper(const void* method) noexcept {
  return static_cast<const detail::method_wrapper*>(method);
}

} // namespace

method::method(const void* method) noexcept : method_(method) {}

bool method::valid() const noexcept {
  const detail::method_wrapper* method = as_method_wrapper(method_);
  return method != nullptr && method->valid();
}

core::string_name method::name() const noexcept {
  const detail::method_wrapper* method = as_method_wrapper(info());
  return method != nullptr ? method->name() : core::string_name{};
}

meta::type method::get_owner_type() const noexcept {
  const detail::method_wrapper* method = as_method_wrapper(info());
  return method != nullptr ? method->get_owner_type() : meta::type{};
}

meta::type method::get_return_type() const noexcept {
  const detail::method_wrapper* method = as_method_wrapper(info());
  return method != nullptr ? method->get_return_type() : meta::type{};
}

std::size_t method::parameter_count() const noexcept {
  const detail::method_wrapper* method = as_method_wrapper(info());
  return method != nullptr ? method->parameter_count() : 0;
}

std::optional<meta::type> method::get_parameter_type(std::size_t index) const noexcept {
  const detail::method_wrapper* method = as_method_wrapper(info());
  return method != nullptr ? method->get_parameter_type(index) : std::nullopt;
}

bool method::const_method() const noexcept {
  const detail::method_wrapper* method = as_method_wrapper(info());
  return method != nullptr && method->const_method();
}

any method::invoke(void* object, std::span<const any_ref> arguments) const {
  const detail::method_wrapper* method = as_method_wrapper(info());
  if (method == nullptr) {
    return {};
  }

  return method->invoke(object, true, arguments);
}

any method::invoke(const void* object, std::span<const any_ref> arguments) const {
  const detail::method_wrapper* method = as_method_wrapper(info());
  if (method == nullptr) {
    return {};
  }

  return method->invoke(object, false, arguments);
}

const void* method::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_;
}

} // namespace caors::reflection
