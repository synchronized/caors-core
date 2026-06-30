// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <atomic>

namespace caors {
namespace {

std::atomic<std::uint64_t> next_object_id{1};

object_registry default_object_registry;

[[nodiscard]] object_type make_root_object_type() {
  return object_type{
      make_meta_type_data<object>(object::static_type_name(), object_type_id::invalid()),
      nullptr,
      nullptr,
      nullptr,
  };
}

[[nodiscard]] meta_type_data to_meta_type_data(meta_type type) noexcept {
  return meta_type_data{
      type.id(), type.name(), type.guid(), type.size(), type.alignment(), type.parent_id(),
  };
}

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
    return type_id_of<object>();
  } catch (...) {
    return object_type_id::invalid();
  }
#else
  return type_id_of<object>();
#endif
}

std::string_view object::static_type_name() noexcept { return "caors.object"; }

object_registry::object_registry() { (void)register_type_for<object>(make_root_object_type()); }

object_registry::~object_registry() = default;

error object_registry::register_type(const object_type& type) {
  return register_type(nullptr, type);
}

error object_registry::register_type(detail::cpp_type_key key, const object_type& type) {
  const error type_status = key != nullptr ? get_meta_registry().register_cpp_type(key, type.data)
                                           : get_meta_registry().register_type(type.data);
  if (type_status && type_status.value() != errc::duplicate_id) {
    return rethrow_as(type_status, error_domain::object);
  }

  std::optional<meta_type> stored_meta = key != nullptr
                                             ? get_meta_registry().find_cpp_type(key)
                                             : get_meta_registry().find_type(type.data.name());
  if (!stored_meta.has_value()) {
    return {error_domain::object, errc::invalid_type};
  }

  object_type stored_type = type;
  stored_type.data = to_meta_type_data(*stored_meta);

  return rethrow_as(get_meta_registry().emplace<object_type>(stored_type.data.id(), stored_type),
                    error_domain::object, errc::duplicate_component, errc::duplicate_id);
}

bool object_registry::contains(object_type_id id) const {
  return get_meta_registry().has<object_type>(id);
}

std::optional<meta_type> object_registry::find_registered_type(detail::cpp_type_key key) const {
  return get_meta_registry().find_cpp_type(key);
}

std::optional<object_type> object_registry::find_type(object_type_id id) const {
  return get_meta_registry().find<object_type>(id);
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
  return get_meta_registry().count<object_type>();
}

object* object_registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  std::optional<object_type> type = get_meta_registry().find<object_type>(id);
  if (!type.has_value()) {
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

  return try_create(type->data.id(), resource);
}

object* object_registry::create(object_type_id id, memory_resource* resource) const {
  std::optional<object_type> type = get_meta_registry().find<object_type>(id);
  if (!type.has_value()) {
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

  return create(type->data.id(), resource);
}

bool object_registry::destroy(object* instance, memory_resource* resource) const noexcept {
  if (instance == nullptr) {
    return false;
  }

  std::optional<object_type> type = get_meta_registry().find<object_type>(instance->type_id());
  if (!type.has_value()) {
    type = find_type(instance->type_name());
  }
  if (!type.has_value()) {
    return false;
  }

  if (type->destroy == nullptr) {
    return false;
  }

  type->destroy(instance, resource);
  return true;
}

object_registry& get_object_registry() noexcept { return default_object_registry; }

}
