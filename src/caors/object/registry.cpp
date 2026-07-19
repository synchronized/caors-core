// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/registry.h"
#include "caors/registration.h"

namespace caors::objects {
namespace {

[[nodiscard]] object_factory_capability find_object_factory(meta::type type) noexcept {
  return type ? object_factory_capability{type} : object_factory_capability{};
}

} // namespace

registry::registry() { (void)registration::type_<object>(object::static_type_name()).commit(); }

registry::~registry() = default;

bool registry::contains(object_type_id id) const {
  return find_object_factory(meta::get_registry().find_type(id)).valid();
}

type registry::find_type(object_type_id id) const {
  const meta::type stored_meta = meta::get_registry().find_type(id);
  if (!stored_meta) {
    return {};
  }

  const object_factory_capability factory = find_object_factory(stored_meta);
  if (!factory) {
    return {};
  }

  return type{stored_meta, factory};
}

type registry::find_type(std::string_view name) const {
  const meta::type type = meta::get_registry().find_type(name);
  if (!type) {
    return {};
  }

  return find_type(type.id());
}

bool registry::is_a(object_type_id type_id, object_type_id base_type_id) const {
  return meta::get_registry().is_a(type_id, base_type_id);
}

std::size_t registry::size() const noexcept {
  return meta::capabilities::size<object_factory_ops>();
}

object* registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  const type object_type = find_type(id);
  if (!object_type) {
    return nullptr;
  }

  return object_type.factory().try_create(resource);
}

object* registry::try_create(std::string_view name, memory_resource* resource) const noexcept {
  const type object_type = find_type(name);
  if (!object_type) {
    return nullptr;
  }

  return try_create(object_type.id(), resource);
}

object* registry::create(object_type_id id, memory_resource* resource) const {
  const type object_type = find_type(id);
  if (!object_type) {
    return nullptr;
  }

  return object_type.factory().create(resource);
}

object* registry::create(std::string_view name, memory_resource* resource) const {
  const type object_type = find_type(name);
  if (!object_type) {
    return nullptr;
  }

  return create(object_type.id(), resource);
}

bool registry::destroy(object_type_id id, object* instance,
                       memory_resource* resource) const noexcept {
  if (instance == nullptr) {
    return false;
  }

  const type object_type = find_type(id);
  if (!object_type) {
    return false;
  }

  return object_type.factory().destroy(instance, resource);
}

bool registry::destroy(object* instance, memory_resource* resource) const noexcept {
  if (instance == nullptr) {
    return false;
  }

  type object_type = find_type(instance->type_id());
  if (!object_type) {
    object_type = find_type(instance->type_name());
  }
  if (!object_type) {
    return false;
  }

  return destroy(object_type.id(), instance, resource);
}

registry& get_registry() noexcept {
  static registry registry;
  return registry;
}

} // namespace caors::objects
