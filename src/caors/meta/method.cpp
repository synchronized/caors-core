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

[[nodiscard]] const detail::method_set_data* as_method_set_data(const void* method_set) noexcept {
  return static_cast<const detail::method_set_data*>(method_set);
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

any method::invoke(std::span<const any_ref> arguments) const {
  const detail::method_data* method = as_method_data(info());
  if (method == nullptr) {
    return {};
  }

  return method->invoke(nullptr, false, arguments);
}

const void* method::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_;
}

method_set::method_set(const void* method_set) noexcept : method_set_(method_set) {}

bool method_set::valid() const noexcept {
  const detail::method_set_data* method_set = as_method_set_data(method_set_);
  return method_set != nullptr && method_set->valid();
}

core::string_name method_set::name() const noexcept {
  const detail::method_set_data* method_set = as_method_set_data(info());
  return method_set != nullptr ? method_set->name() : core::string_name{};
}

std::size_t method_set::overload_count() const noexcept {
  const detail::method_set_data* method_set = as_method_set_data(info());
  return method_set != nullptr ? method_set->overload_count() : 0;
}

method method_set::overload_at(std::size_t index) const {
  const detail::method_set_data* method_set = as_method_set_data(info());
  return method_set != nullptr ? method{method_set->overload_at(index)} : method{};
}

method method_set::single() const {
  const detail::method_set_data* method_set = as_method_set_data(info());
  return method_set != nullptr ? method{method_set->single()} : method{};
}

method method_set::resolve(std::span<const any_ref> arguments) const {
  const detail::method_set_data* method_set = as_method_set_data(info());
  return method_set != nullptr ? method{method_set->resolve(arguments)} : method{};
}

any method_set::invoke(void* object, std::span<const any_ref> arguments) const {
  const method target = resolve(arguments);
  return target ? target.invoke(object, arguments) : any{};
}

any method_set::invoke(const void* object, std::span<const any_ref> arguments) const {
  const method target = resolve(arguments);
  return target ? target.invoke(object, arguments) : any{};
}

any method_set::invoke(std::span<const any_ref> arguments) const {
  const method target = resolve(arguments);
  return target ? target.invoke(arguments) : any{};
}

const void* method_set::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_set_;
}

} // namespace caors::meta
