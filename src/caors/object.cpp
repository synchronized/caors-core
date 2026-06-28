// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace caors {
namespace {

std::atomic<std::uint64_t> next_object_id{1};

object_registry default_object_registry;

[[nodiscard]] object_type make_root_object_type() noexcept {
  return object_type{
      type_info{
          object::static_type_id(),
          object::static_type_name(),
          sizeof(object),
          alignof(object),
          object_type_id::invalid(),
      },
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
  case type_register_result::hash_collision:
    return object_register_result::hash_collision;
  case type_register_result::missing_parent:
    return object_register_result::missing_parent;
  }

  return object_register_result::invalid_type;
}

} // namespace

class object_registry_state {
public:
  mutable std::mutex mutex;
  type_registry types;
  std::unordered_map<std::uint64_t, object_type> objects;
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
  (void)register_type(make_root_object_type());
}

object_registry::~object_registry() { delete state_; }

object_register_result object_registry::register_type(const object_type& type) {
  const type_register_result type_result = state_->types.register_type(type.info);
  if (!type_register_succeeded(type_result)) {
    return to_object_register_result(type_result);
  }

  std::optional<type_info> stored_info = state_->types.find_type(type.info.id);
  if (!stored_info.has_value()) {
    return object_register_result::invalid_type;
  }

  object_type stored_type = type;
  stored_type.info = *stored_info;

  std::lock_guard lock(state_->mutex);
  const bool inserted = state_->objects.emplace(stored_type.info.id.value(), stored_type).second;
  if (!inserted) {
    return object_register_result::duplicate_id;
  }

  return object_register_result::success;
}

bool object_registry::contains(object_type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->objects.contains(id.value());
}

std::optional<object_type> object_registry::find_type(object_type_id id) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->objects.find(id.value());
  if (position == state_->objects.end()) {
    return std::nullopt;
  }

  return position->second;
}

std::optional<object_type> object_registry::find_type(std::string_view name) const {
  std::optional<type_info> info = state_->types.find_type(name);
  if (!info.has_value()) {
    return std::nullopt;
  }

  return find_type(info->id);
}

bool object_registry::is_a(object_type_id type_id, object_type_id base_type_id) const {
  return state_->types.is_a(type_id, base_type_id);
}

std::size_t object_registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->objects.size();
}

object* object_registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  object_try_create_fn try_create = nullptr;

  {
    std::lock_guard lock(state_->mutex);
    const auto position = state_->objects.find(id.value());
    if (position == state_->objects.end()) {
      return nullptr;
    }

    try_create = position->second.try_create;
  }

  if (try_create == nullptr) {
    return nullptr;
  }

  return try_create(resource);
}

object* object_registry::create(object_type_id id, memory_resource* resource) const {
  object_create_fn create_instance = nullptr;

  {
    std::lock_guard lock(state_->mutex);
    const auto position = state_->objects.find(id.value());
    if (position == state_->objects.end()) {
      return nullptr;
    }

    create_instance = position->second.create;
  }

  if (create_instance == nullptr) {
    return nullptr;
  }

  return create_instance(resource);
}

bool object_registry::destroy(object* instance, memory_resource* resource) const noexcept {
  if (instance == nullptr) {
    return false;
  }

  object_destroy_fn destroy_instance = nullptr;

  {
    std::lock_guard lock(state_->mutex);
    const auto position = state_->objects.find(instance->type_id().value());
    if (position == state_->objects.end()) {
      return false;
    }

    destroy_instance = position->second.destroy;
  }

  if (destroy_instance == nullptr) {
    return false;
  }

  destroy_instance(instance, resource);
  return true;
}

void object_registry::clear() noexcept {
  state_->types.clear();

  std::lock_guard lock(state_->mutex);
  state_->objects.clear();
}

object_registry& get_object_registry() noexcept { return default_object_registry; }

}
