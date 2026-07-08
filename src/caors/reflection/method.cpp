// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/method.h"

#include "caors/reflection/detail/method_data.h"

namespace caors::reflection {
method::method(meta::type owner, const detail::method_data* method) noexcept
    : owner_(owner), method_(method) {}

bool method::valid() const noexcept {
  return owner_.valid() && method_ != nullptr && method_->valid();
}

core::string_name method::name() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->name : core::string_name{};
}

meta::type method::get_return_type() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr
             ? meta::detail::make_type_view(method->return_type, method->return_descriptor)
             : meta::type{};
}

std::size_t method::parameter_count() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->parameter_types.size() : 0;
}

std::optional<meta::type> method::get_parameter_type(std::size_t index) const noexcept {
  const detail::method_data* method = info();
  if (method == nullptr || index >= method->parameter_descriptors.size()) {
    return std::nullopt;
  }

  meta::type parameter = meta::detail::make_type_view(method->parameter_types[index],
                                                      method->parameter_descriptors[index]);
  if (!parameter) {
    return std::nullopt;
  }

  return parameter;
}

bool method::const_method() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr && method->const_method;
}

any method::invoke(void* object, std::span<const any_ref> arguments) const {
  const detail::method_data* method = info();
  if (method == nullptr || method->invoke == nullptr) {
    return {};
  }

  return method->invoke(*method, object, true, arguments);
}

any method::invoke(const void* object, std::span<const any_ref> arguments) const {
  const detail::method_data* method = info();
  if (method == nullptr || method->invoke == nullptr) {
    return {};
  }

  return method->invoke(*method, object, false, arguments);
}

const detail::method_data* method::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_;
}

} // namespace caors::reflection
