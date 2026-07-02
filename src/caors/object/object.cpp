// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <atomic>

namespace caors {
namespace {

std::atomic<std::uint64_t> next_object_id{1};

[[nodiscard]] meta_type_data make_root_object_meta_type_data() {
  return make_meta_type_data<object>(object::static_type_name(), object_type_id::invalid());
}

[[nodiscard]] object_type_data make_root_object_type_data() { return {}; }

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

object_type::object_type(meta_type type, const object_type_data* data) noexcept
    : type_(type), data_(data) {}

bool object_type::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta_type object_type::info() const noexcept { return valid() ? type_ : meta_type{}; }

object_type_id object_type::id() const noexcept { return type_.id(); }

string_name object_type::name() const noexcept { return type_.name(); }

type_guid object_type::guid() const noexcept { return type_.guid(); }

std::size_t object_type::size() const noexcept { return type_.size(); }

std::size_t object_type::alignment() const noexcept { return type_.alignment(); }

object_type_id object_type::parent_id() const noexcept { return type_.parent_id(); }

bool object_type::constructible() const noexcept {
  return data_ != nullptr && data_->constructible();
}

const object_type_data* object_type::data() const noexcept { return valid() ? data_ : nullptr; }

object_registry::object_registry() {
  (void)register_type_for<object>(make_root_object_meta_type_data(), make_root_object_type_data());
}

object_registry::~object_registry() = default;

error object_registry::register_type(const meta_type_data& type, const object_type_data& object) {
  return register_type(nullptr, type, object);
}

error object_registry::register_type(detail::cpp_type_key key, const meta_type_data& type,
                                     const object_type_data& object) {
  const error type_status = key != nullptr ? get_meta_registry().register_cpp_type(key, type)
                                           : get_meta_registry().register_type(type);
  if (type_status && type_status.value() != errc::duplicate_id) {
    return rethrow_as(type_status, error_domain::object);
  }

  std::optional<meta_type> stored_meta = key != nullptr
                                             ? get_meta_registry().find_cpp_type(key)
                                             : get_meta_registry().find_type(type.name());
  if (!stored_meta.has_value()) {
    return {error_domain::object, errc::invalid_type};
  }

  return rethrow_as(get_meta_registry().emplace<object_type_data>(stored_meta->id(), object),
                    error_domain::object, errc::duplicate_component, errc::duplicate_id);
}

bool object_registry::contains(object_type_id id) const {
  return get_meta_registry().has<object_type_data>(id);
}

std::optional<meta_type> object_registry::find_registered_type(detail::cpp_type_key key) const {
  return get_meta_registry().find_cpp_type(key);
}

const object_type_data* object_registry::find_type_data(object_type_id id) const {
  return get_meta_registry().find_ptr<object_type_data>(id);
}

std::optional<object_type> object_registry::find_type(object_type_id id) const {
  const std::optional<meta_type> type = get_meta_registry().find_type(id);
  if (!type.has_value()) {
    return std::nullopt;
  }

  const object_type_data* data = find_type_data(id);
  if (data == nullptr) {
    return std::nullopt;
  }

  return object_type{*type, data};
}

std::optional<object_type> object_registry::find_type(std::string_view name) const {
  std::optional<meta_type> type = get_meta_registry().find_type(name);
  if (!type.has_value()) {
    return std::nullopt;
  }

  return find_type(type->id());
}

bool object_registry::is_a(object_type_id type_id, object_type_id base_type_id) const {
  return get_meta_registry().is_a(type_id, base_type_id);
}

std::size_t object_registry::size() const noexcept {
  return get_meta_registry().count<object_type_data>();
}

object* object_registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  const object_type_data* type = find_type_data(id);
  if (type == nullptr) {
    return nullptr;
  }

  if (type->try_create == nullptr) {
    return nullptr;
  }

  return type->try_create(resource);
}

object* object_registry::try_create(std::string_view name,
                                    memory_resource* resource) const noexcept {
  const std::optional<object_type> type = find_type(name);
  if (!type.has_value()) {
    return nullptr;
  }

  return try_create(type->id(), resource);
}

object* object_registry::create(object_type_id id, memory_resource* resource) const {
  const object_type_data* type = find_type_data(id);
  if (type == nullptr) {
    return nullptr;
  }

  if (type->create == nullptr) {
    return nullptr;
  }

  return type->create(resource);
}

object* object_registry::create(std::string_view name, memory_resource* resource) const {
  const std::optional<object_type> type = find_type(name);
  if (!type.has_value()) {
    return nullptr;
  }

  return create(type->id(), resource);
}

bool object_registry::destroy(object* instance, memory_resource* resource) const noexcept {
  if (instance == nullptr) {
    return false;
  }

  std::optional<object_type> type = find_type(instance->type_id());
  if (!type.has_value()) {
    type = find_type(instance->type_name());
  }
  if (!type.has_value()) {
    return false;
  }

  const object_type_data* data = type->data();
  if (data == nullptr || data->destroy == nullptr) {
    return false;
  }

  data->destroy(instance, resource);
  return true;
}

object_registry& get_object_registry() noexcept {
  static object_registry registry;
  return registry;
}

}
