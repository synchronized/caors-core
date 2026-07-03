// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <atomic>

namespace caors {
namespace {

std::atomic<std::uint64_t> next_object_id{1};

[[nodiscard]] meta::type_data make_root_object_meta_type_data() {
  return meta::make_type_data<object>(object::static_type_name(), object_type_id::invalid());
}

[[nodiscard]] objects::type_data make_root_object_type_data() { return {}; }

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
    return caors::meta::get<object>().id();
  } catch (...) {
    return object_type_id::invalid();
  }
#else
  return caors::meta::get<object>().id();
#endif
}

std::string_view object::static_type_name() noexcept { return "caors.object"; }

objects::type::type(meta::type_view type, const objects::type_data* data) noexcept
    : type_(type), data_(data) {}

bool objects::type::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta::type_view objects::type::info() const noexcept { return valid() ? type_ : meta::type_view{}; }

object_type_id objects::type::id() const noexcept { return type_.id(); }

string_name objects::type::name() const noexcept { return type_.name(); }

type_guid objects::type::guid() const noexcept { return type_.guid(); }

std::size_t objects::type::size() const noexcept { return type_.size(); }

std::size_t objects::type::alignment() const noexcept { return type_.alignment(); }

object_type_id objects::type::parent_id() const noexcept { return type_.parent_id(); }

bool objects::type::constructible() const noexcept {
  return data_ != nullptr && data_->constructible();
}

const objects::type_data* objects::type::data() const noexcept { return valid() ? data_ : nullptr; }

objects::registry::registry() {
  (void)register_type_for<object>(make_root_object_meta_type_data(), make_root_object_type_data());
}

objects::registry::~registry() = default;

error objects::registry::register_type(const meta::type_data& type,
                                       const objects::type_data& object) {
  return register_type(nullptr, type, object);
}

error objects::registry::register_type(caors::detail::cpp_type_key key, const meta::type_data& type,
                                       const objects::type_data& object) {
  const error type_status = key != nullptr ? meta::get_registry().register_cpp_type(key, type)
                                           : meta::get_registry().register_type(type);
  if (type_status && type_status.value() != errc::duplicate_id) {
    return rethrow_as(type_status, error_domain::object);
  }

  std::optional<meta::type_view> stored_meta = key != nullptr
                                                   ? meta::get_registry().find_cpp_type(key)
                                                   : meta::get_registry().find_type(type.name());
  if (!stored_meta.has_value()) {
    return {error_domain::object, errc::invalid_type};
  }

  return rethrow_as(meta::get_registry().emplace<objects::type_data>(stored_meta->id(), object),
                    error_domain::object, errc::duplicate_component, errc::duplicate_id);
}

bool objects::registry::contains(object_type_id id) const {
  return meta::get_registry().has<objects::type_data>(id);
}

std::optional<meta::type_view>
objects::registry::find_registered_type(caors::detail::cpp_type_key key) const {
  return meta::get_registry().find_cpp_type(key);
}

const objects::type_data* objects::registry::find_type_data(object_type_id id) const {
  return meta::get_registry().find_ptr<objects::type_data>(id);
}

std::optional<objects::type> objects::registry::find_type(object_type_id id) const {
  const std::optional<meta::type_view> type = meta::get_registry().find_type(id);
  if (!type.has_value()) {
    return std::nullopt;
  }

  const objects::type_data* data = find_type_data(id);
  if (data == nullptr) {
    return std::nullopt;
  }

  return objects::type{*type, data};
}

std::optional<objects::type> objects::registry::find_type(std::string_view name) const {
  std::optional<meta::type_view> type = meta::get_registry().find_type(name);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

bool objects::registry::is_a(object_type_id type_id, object_type_id base_type_id) const {
  return meta::get_registry().is_a(type_id, base_type_id);
}

std::size_t objects::registry::size() const noexcept {
  return meta::get_registry().count<objects::type_data>();
}

object* objects::registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  const objects::type_data* type = find_type_data(id);
  if (type == nullptr) {
    return nullptr;
  }

  if (type->try_create == nullptr) {
    return nullptr;
  }

  return type->try_create(resource);
}

object* objects::registry::try_create(std::string_view name,
                                      memory_resource* resource) const noexcept {
  const std::optional<objects::type> type = find_type(name);
  if (!type.has_value()) {
    return nullptr;
  }

  return try_create(type->id(), resource);
}

object* objects::registry::create(object_type_id id, memory_resource* resource) const {
  const objects::type_data* type = find_type_data(id);
  if (type == nullptr) {
    return nullptr;
  }

  if (type->create == nullptr) {
    return nullptr;
  }

  return type->create(resource);
}

object* objects::registry::create(std::string_view name, memory_resource* resource) const {
  const std::optional<objects::type> type = find_type(name);
  if (!type.has_value()) {
    return nullptr;
  }

  return create(type->id(), resource);
}

bool objects::registry::destroy(object* instance, memory_resource* resource) const noexcept {
  if (instance == nullptr) {
    return false;
  }

  std::optional<objects::type> type = find_type(instance->type_id());
  if (!type.has_value()) {
    type = find_type(instance->type_name());
  }
  if (!type.has_value()) {
    return false;
  }

  const objects::type_data* data = type->data();
  if (data == nullptr || data->destroy == nullptr) {
    return false;
  }

  data->destroy(instance, resource);
  return true;
}

objects::registry& objects::get_registry() noexcept {
  static objects::registry registry;
  return registry;
}

}
