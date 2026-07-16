// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/runtime/type.h"

#include "caors/meta/registry.h"
#include "caors/reflection/registry.h"

namespace caors::runtime {

type::type(meta::type meta_type) noexcept : meta_(meta_type) {}

bool type::valid() const noexcept { return meta_.valid(); }

meta::type type::meta() const noexcept { return meta_; }

reflection::type type::reflection() const noexcept {
  return valid() ? reflection::find(id()) : reflection::type{};
}

bool type::reflectable() const noexcept { return static_cast<bool>(reflection()); }

type_id type::id() const noexcept { return meta_.id(); }

string_name type::name() const noexcept { return meta_.name(); }

type_guid type::guid() const noexcept { return meta_.guid(); }

type type::parent() const noexcept { return type{meta_.parent()}; }

meta::type_kind type::kind() const noexcept { return meta_.kind(); }

meta::wrapper_semantics type::wrapper() const noexcept { return meta_.wrapper(); }

bool type::indirect() const noexcept { return meta_.indirect(); }

bool type::nullable() const noexcept { return meta_.nullable(); }

std::size_t type::size() const noexcept {
  const meta::value_storage_ops* storage = meta_.get_raw_ops<meta::value_storage_ops>();
  return storage != nullptr ? storage->layout.size : 0;
}

std::size_t type::alignment() const noexcept {
  const meta::value_storage_ops* storage = meta_.get_raw_ops<meta::value_storage_ops>();
  return storage != nullptr ? storage->layout.alignment : 0;
}

bool type::value_constructible() const noexcept {
  const meta::value_storage_ops* storage = meta_.get_raw_ops<meta::value_storage_ops>();
  return storage != nullptr && storage->ops.default_construct != nullptr &&
         storage->ops.destroy != nullptr;
}

bool type::constructible() const noexcept { return constructible(0); }

bool type::constructible(std::size_t argument_count) const noexcept {
  if (!valid()) {
    return false;
  }

  if (argument_count == 0) {
    const meta::factory_capability factory = meta_.get_capability<meta::factory_capability>();
    if (factory) {
      return true;
    }
  }

  const meta::constructor_capability constructor =
      meta_.get_capability<meta::constructor_capability>();
  if (constructor.matches(argument_count)) {
    return true;
  }

  return argument_count == 0 && value_constructible();
}

any type::create(memory::memory_resource* resource) const {
  return create(std::span<const meta::any_ref>{}, resource);
}

any type::create(std::span<const meta::any_ref> arguments,
                 memory::memory_resource* resource) const {
  if (!valid()) {
    return {};
  }

  if (arguments.empty()) {
    const meta::factory_capability factory = meta_.get_capability<meta::factory_capability>();
    if (factory) {
      any value = factory.construct_any(resource);
      if (value.valid()) {
        return value;
      }
    }
  }

  const meta::constructor_capability constructor =
      meta_.get_capability<meta::constructor_capability>();
  if (constructor.matches(arguments.size())) {
    any value = constructor.construct_any(arguments, resource);
    if (value.valid()) {
      return value;
    }
  }

  return arguments.empty() ? create_value(resource) : any{};
}

any type::create_value(memory::memory_resource* resource) const {
  return valid() ? any::default_construct(meta_, resource) : any{};
}

std::size_t type::field_count() const noexcept {
  const reflection::type reflected = reflection();
  return reflected ? reflected.field_count() : 0;
}

field type::field_at(std::size_t index) const {
  const reflection::type reflected = reflection();
  return reflected ? reflected.field_at(index) : field{};
}

field type::find_field(string_name field_name) const {
  const reflection::type reflected = reflection();
  return reflected ? reflected.find_field(field_name) : field{};
}

field type::find_field(std::string_view field_name) const {
  const reflection::type reflected = reflection();
  return reflected ? reflected.find_field(field_name) : field{};
}

std::size_t type::method_count() const noexcept {
  const reflection::type reflected = reflection();
  return reflected ? reflected.method_count() : 0;
}

method type::method_at(std::size_t index) const {
  const reflection::type reflected = reflection();
  return reflected ? reflected.method_at(index) : method{};
}

method type::find_method(string_name method_name) const {
  const reflection::type reflected = reflection();
  return reflected ? reflected.find_method(method_name) : method{};
}

method type::find_method(std::string_view method_name) const {
  const reflection::type reflected = reflection();
  return reflected ? reflected.find_method(method_name) : method{};
}

} // namespace caors::runtime
