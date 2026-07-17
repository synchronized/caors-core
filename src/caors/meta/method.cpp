// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/method.h"

#include "caors/meta/detail/method_data.h"

namespace caors::meta {
namespace {

[[nodiscard]] const metadata_map& empty_metadata_map() noexcept {
  static const metadata_map empty{
      typename metadata_map::allocator_type{&memory::library_resource()}};
  return empty;
}

[[nodiscard]] const detail::method_data* as_method_data(const void* method) noexcept {
  return static_cast<const detail::method_data*>(method);
}

} // namespace

method::method(const void* method) noexcept : method_(method) {}

bool method::valid() const noexcept {
  const detail::method_data* method = as_method_data(method_);
  return method != nullptr && method->valid();
}

core::string_name method::name() const noexcept {
  const detail::method_data* method = as_method_data(info());
  return method != nullptr ? method->name() : core::string_name{};
}

meta::type method::get_owner_type() const noexcept {
  const detail::method_data* method = as_method_data(info());
  return method != nullptr ? method->get_owner_type() : meta::type{};
}

meta::type method::get_return_type() const noexcept {
  const detail::method_data* method = as_method_data(info());
  return method != nullptr ? method->get_return_type() : meta::type{};
}

std::size_t method::parameter_count() const noexcept {
  const detail::method_data* method = as_method_data(info());
  return method != nullptr ? method->parameter_count() : 0;
}

std::optional<meta::type> method::get_parameter_type(std::size_t index) const noexcept {
  const detail::method_data* method = as_method_data(info());
  return method != nullptr ? method->get_parameter_type(index) : std::nullopt;
}

bool method::const_method() const noexcept {
  const detail::method_data* method = as_method_data(info());
  return method != nullptr && method->const_method();
}

const meta::metadata_map& method::metadata() const noexcept {
  const detail::method_data* method = as_method_data(info());
  return method != nullptr ? method->metadata() : empty_metadata_map();
}

const meta::metadata_value* method::find_metadata(meta::metadata_key key) const noexcept {
  return meta::find_metadata(metadata(), key);
}

any method::invoke(void* object, std::span<const any_ref> arguments) const {
  const detail::method_data* method = as_method_data(info());
  if (method == nullptr) {
    return {};
  }

  return method->invoke(object, true, arguments);
}

any method::invoke(const void* object, std::span<const any_ref> arguments) const {
  const detail::method_data* method = as_method_data(info());
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

} // namespace caors::meta
