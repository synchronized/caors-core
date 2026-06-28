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
      make_type_info<object>(object::static_type_name(), object_type_id::invalid()),
      nullptr,
      nullptr,
      nullptr,
  };
}

[[nodiscard]] object_register_result
to_object_register_result(type_register_result result) noexcept {
  switch (result) {
  case type_register_result::success:
    return object_register_result::success;
  case type_register_result::invalid_type:
    return object_register_result::invalid_type;
  case type_register_result::duplicate_id:
    return object_register_result::duplicate_id;
  case type_register_result::duplicate_name:
    return object_register_result::duplicate_name;
  case type_register_result::duplicate_guid:
    return object_register_result::duplicate_guid;
  case type_register_result::missing_parent:
    return object_register_result::missing_parent;
  }

  return object_register_result::invalid_type;
}

[[nodiscard]] object_register_result
to_object_register_result(meta_component_result result) noexcept {
  switch (result) {
  case meta_component_result::success:
    return object_register_result::success;
  case meta_component_result::invalid_type:
    return object_register_result::invalid_type;
  case meta_component_result::missing_type:
    return object_register_result::invalid_type;
  case meta_component_result::duplicate_component:
    return object_register_result::duplicate_id;
  }

  return object_register_result::invalid_type;
}

} // namespace

class object_registry_state {
public:
  meta_registry meta;
};

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

object_type_id object::static_type_id() noexcept { return make_object_type_id(static_type_name()); }

std::string_view object::static_type_name() noexcept { return "caors.object"; }

object_registry::object_registry() : state_(new object_registry_state) {
  (void)register_type_for<object>(make_root_object_type());
}

object_registry::~object_registry() { delete state_; }

object_register_result object_registry::register_type(const object_type& type) {
  return register_type(nullptr, type);
}

object_register_result object_registry::register_type(detail::cpp_type_key key,
                                                      const object_type& type) {
  const type_register_result type_result = key != nullptr
                                               ? state_->meta.register_cpp_type(key, type.info)
                                               : state_->meta.register_type(type.info);
  if (!type_register_succeeded(type_result) && type_result != type_register_result::duplicate_id) {
    return to_object_register_result(type_result);
  }

  std::optional<type_info> stored_info =
      key != nullptr ? state_->meta.find_cpp_type(key) : state_->meta.find_type(type.info.name);
  if (!stored_info.has_value()) {
    return object_register_result::invalid_type;
  }

  object_type stored_type = type;
  stored_type.info = *stored_info;

  return to_object_register_result(
      state_->meta.emplace<object_type>(stored_type.info.id, stored_type));
}

bool object_registry::contains(object_type_id id) const {
  return state_->meta.has<object_type>(id);
}

std::optional<type_info> object_registry::find_registered_type(detail::cpp_type_key key) const {
  return state_->meta.find_cpp_type(key);
}

std::optional<object_type> object_registry::find_type(object_type_id id) const {
  return state_->meta.find<object_type>(id);
}

std::optional<object_type> object_registry::find_type(std::string_view name) const {
  std::optional<type_info> info = state_->meta.find_type(name);
  if (!info.has_value()) {
    return std::nullopt;
  }

  return find_type(info->id);
}

bool object_registry::is_a(object_type_id type_id, object_type_id base_type_id) const {
  return state_->meta.is_a(type_id, base_type_id);
}

std::size_t object_registry::size() const noexcept { return state_->meta.count<object_type>(); }

object* object_registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  std::optional<object_type> type = state_->meta.find<object_type>(id);
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

  return try_create(type->info.id, resource);
}

object* object_registry::create(object_type_id id, memory_resource* resource) const {
  std::optional<object_type> type = state_->meta.find<object_type>(id);
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

  return create(type->info.id, resource);
}

bool object_registry::destroy(object* instance, memory_resource* resource) const noexcept {
  if (instance == nullptr) {
    return false;
  }

  std::optional<object_type> type = state_->meta.find<object_type>(instance->type_id());
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

void object_registry::clear() noexcept { state_->meta.clear(); }

object_registry& get_object_registry() noexcept { return default_object_registry; }

}
