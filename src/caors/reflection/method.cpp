// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/method.h"

#include "caors/reflection/detail/method_data.h"

namespace caors::reflection {
namespace {

[[nodiscard]] core::string_name type_name_of(type_id id) noexcept {
  const meta::type type = caors::meta::find(id);
  return type ? type.name() : core::string_name{};
}

} // namespace

method::method(meta::type owner, const detail::method_data* method) noexcept
    : owner_(owner), method_(method) {}

bool method::valid() const noexcept {
  return owner_.valid() && method_ != nullptr && method_->valid();
}

core::string_name method::name() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->name : core::string_name{};
}

type_id method::return_type() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->return_type : type_id{};
}

core::string_name method::return_type_name() const noexcept { return type_name_of(return_type()); }

std::size_t method::parameter_count() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->parameter_types.size() : 0;
}

type_id method::parameter_type(std::size_t index) const noexcept {
  const detail::method_data* method = info();
  if (method == nullptr || index >= method->parameter_types.size()) {
    return type_id{};
  }

  return method->parameter_types[index];
}

core::string_name method::parameter_type_name(std::size_t index) const noexcept {
  return type_name_of(parameter_type(index));
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
