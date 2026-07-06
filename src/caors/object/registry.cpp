// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/registry.h"

namespace caors::objects {
namespace {

[[nodiscard]] meta::detail::type_data make_root_object_meta_type_data() {
  return meta::detail::make_type_data<object>(object::static_type_name(),
                                              object_type_id::invalid());
}

[[nodiscard]] detail::type_data make_root_object_type_data() { return {}; }

} // namespace

registry::registry() {
  (void)register_type_for<object>(make_root_object_meta_type_data(), make_root_object_type_data());
}

registry::~registry() = default;

error registry::register_type(const meta::detail::type_data& type,
                              const detail::type_data& object) {
  const error type_status = meta::get_registry().register_type(type);
  if (type_status && type_status.value() != errc::duplicate_id) {
    return rethrow_as(type_status, error_domain::object);
  }

  const meta::type stored_meta = meta::get_registry().find_type(type.id());
  if (!stored_meta) {
    return {error_domain::object, errc::invalid_type};
  }

  return rethrow_as(meta::get_registry().emplace<detail::type_data>(stored_meta.id(), object),
                    error_domain::object, errc::duplicate_component, errc::duplicate_id);
}

bool registry::contains(object_type_id id) const {
  return meta::get_registry().has<detail::type_data>(id);
}

const detail::type_data* registry::find_type_data(object_type_id id) const {
  return meta::get_registry().find_ptr<detail::type_data>(id);
}

type registry::find_type(object_type_id id) const {
  const meta::type stored_meta = meta::get_registry().find_type(id);
  if (!stored_meta) {
    return {};
  }

  const detail::type_data* data = find_type_data(id);
  if (data == nullptr) {
    return {};
  }

  return type{stored_meta, data};
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
  return meta::get_registry().count<detail::type_data>();
}

object* registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  const detail::type_data* type = find_type_data(id);
  if (type == nullptr) {
    return nullptr;
  }

  if (type->try_create == nullptr) {
    return nullptr;
  }

  return type->try_create(resource);
}

object* registry::try_create(std::string_view name, memory_resource* resource) const noexcept {
  const type object_type = find_type(name);
  if (!object_type) {
    return nullptr;
  }

  return try_create(object_type.id(), resource);
}

object* registry::create(object_type_id id, memory_resource* resource) const {
  const detail::type_data* type = find_type_data(id);
  if (type == nullptr) {
    return nullptr;
  }

  if (type->create == nullptr) {
    return nullptr;
  }

  return type->create(resource);
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

  const detail::type_data* data = object_type.data();
  if (data == nullptr || data->destroy == nullptr) {
    return false;
  }

  data->destroy(instance, resource);
  return true;
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
