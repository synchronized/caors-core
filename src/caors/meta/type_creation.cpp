// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/type_creation.h"

#include "caors/meta/capability_facade.h"
#include "caors/meta/constructor.h"
#include "caors/meta/value_ops.h"

namespace caors::meta {

std::size_t type::size() const noexcept {
  const meta::value_storage_ops* storage = get_raw_ops<meta::value_storage_ops>();
  return storage != nullptr ? storage->layout.size : 0;
}

std::size_t type::alignment() const noexcept {
  const meta::value_storage_ops* storage = get_raw_ops<meta::value_storage_ops>();
  return storage != nullptr ? storage->layout.alignment : 0;
}

bool type::value_constructible() const noexcept {
  const meta::value_storage_ops* storage = get_raw_ops<meta::value_storage_ops>();
  return storage != nullptr && storage->ops.default_construct != nullptr &&
         storage->ops.destroy != nullptr;
}

bool type::constructible() const noexcept { return constructible(0); }

bool type::constructible(std::size_t argument_count) const noexcept {
  if (!valid()) {
    return false;
  }

  if (argument_count == 0) {
    const meta::factory_capability factory = get_capability<meta::factory_capability>();
    if (factory) {
      return true;
    }
  }

  const meta::constructor_capability constructor =
      get_capability<meta::constructor_capability>();
  if (constructor.matches(argument_count)) {
    return true;
  }

  return argument_count == 0 && value_constructible();
}

any type::create(memory::memory_resource* resource) const {
  return create(std::span<const meta::argument>{}, resource);
}

any type::create(std::span<const meta::argument> arguments,
                 memory::memory_resource* resource) const {
  if (!valid()) {
    return {};
  }

  if (arguments.empty()) {
    const meta::factory_capability factory = get_capability<meta::factory_capability>();
    if (factory) {
      any value = factory.construct(resource);
      if (value.valid()) {
        return value;
      }
    }
  }

  const meta::constructor_capability constructor =
      get_capability<meta::constructor_capability>();
  if (constructor.matches(arguments.size())) {
    any value = constructor.construct(arguments, resource);
    if (value.valid()) {
      return value;
    }
  }

  return arguments.empty() ? create_value(resource) : any{};
}

any type::create_value(memory::memory_resource* resource) const {
  return valid() ? any::default_construct(*this, resource) : any{};
}

} // namespace caors::meta
