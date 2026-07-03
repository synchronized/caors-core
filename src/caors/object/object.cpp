// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <atomic>

namespace caors::objects {
namespace {

std::atomic<std::uint64_t> next_object_id{1};

[[nodiscard]] meta::detail::type_data make_root_object_meta_type_data() {
  return meta::detail::make_type_data<object>(object::static_type_name(),
                                              object_type_id::invalid());
}

[[nodiscard]] detail::type_data make_root_object_type_data() { return {}; }

} // namespace

object_id generate_object_id() noexcept {
  std::uint64_t value = next_object_id.fetch_add(1, std::memory_order_relaxed);
  if (value == 0) {
    value = next_object_id.fetch_add(1, std::memory_order_relaxed);
  }

  return object_id{value};
}

object::object() noexcept : id_(generate_object_id()) {}

object::~object() = default;

object_id object::id() const noexcept { return id_; }

object_type_id object::type_id() const noexcept { return static_type_id(); }

std::string_view object::type_name() const noexcept { return static_type_name(); }

object_type_id object::static_type_id() noexcept {
#if CAORS_HAS_EXCEPTIONS
  try {
    return caors::meta::find<object>().id();
  } catch (...) {
    return object_type_id::invalid();
  }
#else
  return caors::meta::find<object>().id();
#endif
}

std::string_view object::static_type_name() noexcept { return "caors.object"; }

type::type(meta::type type, const detail::type_data* data) noexcept : type_(type), data_(data) {}

bool type::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta::type type::info() const noexcept { return valid() ? type_ : meta::type{}; }

object_type_id type::id() const noexcept { return type_.id(); }

core::string_name type::name() const noexcept { return type_.name(); }

core::type_guid type::guid() const noexcept { return type_.guid(); }

std::size_t type::size() const noexcept { return type_.size(); }

std::size_t type::alignment() const noexcept { return type_.alignment(); }

object_type_id type::parent_id() const noexcept { return type_.parent_id(); }

bool type::constructible() const noexcept { return data_ != nullptr && data_->constructible(); }

const detail::type_data* type::data() const noexcept { return valid() ? data_ : nullptr; }

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

  const detail::type_data* data = object_type.data();
  if (data == nullptr || data->destroy == nullptr) {
    return false;
  }

  data->destroy(instance, resource);
  return true;
}

registry& get_registry() noexcept {
  static registry registry;
  return registry;
}

} // namespace caors::objects
