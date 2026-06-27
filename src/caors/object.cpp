// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

namespace caors {
namespace {

struct object_type_entry {
  std::string name;
  object_type type;
};

std::atomic<std::uint64_t> next_object_id{1};

object_registry default_object_registry;

} // namespace

class object_registry_state {
public:
  mutable std::mutex mutex;
  std::unordered_map<std::uint64_t, object_type_entry> types;
  std::unordered_map<std::string, object_type_id> names;
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
  (void)register_type(object_type{
      object::static_type_id(),
      object::static_type_name(),
      sizeof(object),
      alignof(object),
      object_type_id::invalid(),
      nullptr,
      nullptr,
      nullptr,
  });
}

object_registry::~object_registry() { delete state_; }

bool object_registry::register_type(const object_type& type) {
  if (!type.valid() || type.name.empty()) {
    return false;
  }

  std::lock_guard lock(state_->mutex);
  if (state_->types.contains(type.id.value()) || state_->names.contains(std::string(type.name))) {
    return false;
  }

  if (type.parent_id && !state_->types.contains(type.parent_id.value())) {
    return false;
  }

  auto [position, inserted] = state_->types.try_emplace(type.id.value());
  if (!inserted) {
    return false;
  }

  object_type_entry& entry = position->second;
  entry.name = std::string(type.name);
  entry.type = type;
  entry.type.name = entry.name;
  state_->names.emplace(entry.name, type.id);

  return true;
}

bool object_registry::contains(object_type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->types.contains(id.value());
}

std::optional<object_type> object_registry::find_type(object_type_id id) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->types.find(id.value());
  if (position == state_->types.end()) {
    return std::nullopt;
  }

  return position->second.type;
}

std::optional<object_type> object_registry::find_type(std::string_view name) const {
  std::lock_guard lock(state_->mutex);
  const auto name_position = state_->names.find(std::string(name));
  if (name_position == state_->names.end()) {
    return std::nullopt;
  }

  const auto type_position = state_->types.find(name_position->second.value());
  if (type_position == state_->types.end()) {
    return std::nullopt;
  }

  return type_position->second.type;
}

bool object_registry::is_a(object_type_id type_id, object_type_id base_type_id) const {
  if (!type_id || !base_type_id) {
    return false;
  }

  std::lock_guard lock(state_->mutex);
  object_type_id current_id = type_id;
  while (current_id) {
    if (current_id == base_type_id) {
      return true;
    }

    const auto position = state_->types.find(current_id.value());
    if (position == state_->types.end()) {
      return false;
    }

    current_id = position->second.type.parent_id;
  }

  return false;
}

std::size_t object_registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

object* object_registry::try_create(object_type_id id, memory_resource* resource) const noexcept {
  object_try_create_fn try_create = nullptr;

  {
    std::lock_guard lock(state_->mutex);
    const auto position = state_->types.find(id.value());
    if (position == state_->types.end()) {
      return nullptr;
    }

    try_create = position->second.type.try_create;
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
    const auto position = state_->types.find(id.value());
    if (position == state_->types.end()) {
      return nullptr;
    }

    create_instance = position->second.type.create;
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
    const auto position = state_->types.find(instance->type_id().value());
    if (position == state_->types.end()) {
      return false;
    }

    destroy_instance = position->second.type.destroy;
  }

  if (destroy_instance == nullptr) {
    return false;
  }

  destroy_instance(instance, resource);
  return true;
}

void object_registry::clear() noexcept {
  std::lock_guard lock(state_->mutex);
  state_->types.clear();
  state_->names.clear();
}

object_registry& get_object_registry() noexcept { return default_object_registry; }

}
