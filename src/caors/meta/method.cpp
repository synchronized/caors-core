// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/method.h"

#include "caors/meta/detail/method_data.h"
#include "caors/meta/instance.h"

namespace caors::meta {
namespace {

[[nodiscard]] const metadata_map& empty_metadata_map() noexcept {
  static const metadata_map empty{
      typename metadata_map::allocator_type{&memory::library_resource()}};
  return empty;
}

} // namespace

method::method(const detail::method_data* method) noexcept : method_(method) {}

bool method::valid() const noexcept {
  return method_ != nullptr && method_->valid();
}

core::string_name method::name() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->name() : core::string_name{};
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
  return method != nullptr && method->const_method();
}

const meta::metadata_map& method::metadata() const noexcept {
  const detail::method_data* method = info();
  return method != nullptr ? method->metadata() : empty_metadata_map();
}

const meta::metadata_value* method::find_metadata(meta::metadata_key key) const noexcept {
  return meta::find_metadata(metadata(), key);
}

any method::invoke(const instance& object, std::span<const argument> arguments) const {
  const detail::method_data* method = info();
  if (method == nullptr || !object) {
    return {};
  }

  return method->invoke(object.writable() ? object.mutable_data() : object.data(),
                        object.writable(), arguments);
}

any method::invoke(std::span<const argument> arguments) const {
  const detail::method_data* method = info();
  if (method == nullptr) {
    return {};
  }

  return method->invoke(nullptr, false, arguments);
}

const detail::method_data* method::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_;
}

method_set::method_set(const detail::method_set_data* method_set) noexcept
    : method_set_(method_set) {}

bool method_set::valid() const noexcept {
  return method_set_ != nullptr && method_set_->valid();
}

core::string_name method_set::name() const noexcept {
  const detail::method_set_data* method_set = info();
  return method_set != nullptr ? method_set->name() : core::string_name{};
}

std::size_t method_set::overload_count() const noexcept {
  const detail::method_set_data* method_set = info();
  return method_set != nullptr ? method_set->overload_count() : 0;
}

method method_set::overload_at(std::size_t index) const {
  const detail::method_set_data* method_set = info();
  return method_set != nullptr ? method{method_set->overload_at(index)} : method{};
}

method method_set::single() const {
  const detail::method_set_data* method_set = info();
  return method_set != nullptr ? method{method_set->single()} : method{};
}

method method_set::resolve(std::span<const argument> arguments) const {
  const detail::method_set_data* method_set = info();
  return method_set != nullptr ? method{method_set->resolve(arguments)} : method{};
}

any method_set::invoke(const instance& object, std::span<const argument> arguments) const {
  const method target = resolve(arguments);
  return target ? target.invoke(object, arguments) : any{};
}

any method_set::invoke(std::span<const argument> arguments) const {
  const method target = resolve(arguments);
  return target ? target.invoke(arguments) : any{};
}

const detail::method_set_data* method_set::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return method_set_;
}

} // namespace caors::meta
