// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/manager.h"

namespace caors::objects {

manager::manager(memory_resource* resource) : manager(meta::default_context(), resource) {}

manager::manager(meta::context& type_context, memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      type_context_(&type_context), entries_(entry_map::allocator_type{resource_}),
      pending_destroy_(entry_queue::allocator_type{resource_}) {}

manager::manager(object_index& index, memory_resource* resource)
    : manager(meta::default_context(), index, resource) {}

manager::manager(meta::context& type_context, object_index& index, memory_resource* resource)
    : resource_(resource != nullptr ? resource : index.resource()), type_context_(&type_context),
      index_(&index), entries_(entry_map::allocator_type{resource_}),
      pending_destroy_(entry_queue::allocator_type{resource_}) {}

manager::~manager() { clear(); }

std::size_t manager::size() const noexcept { return entries_.size(); }

std::size_t manager::pending_destroy_count() const noexcept { return pending_destroy_.size(); }

bool manager::contains(object_id id) const { return find_entry(id) != nullptr; }

object* manager::find(object_id id) const {
  const entry* stored = find_entry(id);
  return stored != nullptr ? stored->instance : nullptr;
}

object* manager::find(object_guid guid) const {
  if (index_ == nullptr) {
    return nullptr;
  }

  return find(index_->find(guid));
}

object* manager::find(handle target) const {
  if (!target) {
    return nullptr;
  }

  const entry* stored = find_entry(target.id());
  if (stored == nullptr) {
    return nullptr;
  }

  if (!objects::is_a(*type_context_, stored->type, target.type())) {
    return nullptr;
  }

  return stored->instance;
}

object* manager::create(object_type_id id) {
  if (!id) {
    return nullptr;
  }

  object* instance = objects::create(*type_context_, id, resource_);
  if (instance == nullptr) {
    return nullptr;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    if (!store(instance, id, resource_)) {
      (void)objects::destroy(*type_context_, id, instance, resource_);
      return nullptr;
    }

    return instance;
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    (void)objects::destroy(*type_context_, id, instance, resource_);
    throw;
  }
#endif
}

object* manager::create_with_guid(object_type_id id, object_guid guid) {
  if (index_ == nullptr || !guid) {
    return nullptr;
  }

  object* instance = create(id);
  if (instance == nullptr) {
    return nullptr;
  }

  if (!index_->bind(guid, instance->id())) {
    (void)destroy(instance);
    return nullptr;
  }

  return instance;
}

bool manager::destroy(object_id id) noexcept {
  auto position = entries_.find(id);
  if (position == entries_.end()) {
    return false;
  }

  const entry stored = position->second;
  entries_.erase(position);
  if (index_ != nullptr) {
    (void)index_->unbind(stored.id);
  }

  destroy_entry(stored);
  return true;
}

bool manager::destroy(object* instance) noexcept {
  if (instance == nullptr) {
    return false;
  }

  const entry* stored = find_entry(instance->id());
  if (stored == nullptr || stored->instance != instance) {
    return false;
  }

  return destroy(stored->id);
}

bool manager::destroy_later(object_id id) {
  auto position = entries_.find(id);
  if (position == entries_.end()) {
    return false;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    pending_destroy_.push_back(position->second);
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    return false;
  }
#endif

  const object_id stored_id = position->second.id;
  entries_.erase(position);
  if (index_ != nullptr) {
    (void)index_->unbind(stored_id);
  }

  return true;
}

bool manager::destroy_later(object* instance) {
  if (instance == nullptr) {
    return false;
  }

  const entry* stored = find_entry(instance->id());
  if (stored == nullptr || stored->instance != instance) {
    return false;
  }

  return destroy_later(stored->id);
}

void manager::flush_destroy_queue() noexcept {
  while (!pending_destroy_.empty()) {
    const entry stored = pending_destroy_.back();
    pending_destroy_.pop_back();
    destroy_entry(stored);
  }
}

void manager::clear() noexcept {
  while (!entries_.empty()) {
    auto position = entries_.begin();
    const entry stored = position->second;
    entries_.erase(position);
    if (index_ != nullptr) {
      (void)index_->unbind(stored.id);
    }
    destroy_entry(stored);
  }

  flush_destroy_queue();
}

handle manager::handle_of(object_id id) const {
  const entry* stored = find_entry(id);
  return stored != nullptr ? handle{stored->id, stored->type} : handle{};
}

bool manager::bind_guid(object_id id, object_guid guid) {
  if (index_ == nullptr || find_entry(id) == nullptr) {
    return false;
  }

  return index_->bind(guid, id);
}

object_guid manager::guid_of(object_id id) const {
  return index_ != nullptr ? index_->guid_of(id) : object_guid::nil();
}

const manager::entry* manager::find_entry(object_id id) const {
  if (!id) {
    return nullptr;
  }

  const auto position = entries_.find(id);
  return position != entries_.end() ? &position->second : nullptr;
}

manager::entry* manager::find_entry(object_id id) {
  if (!id) {
    return nullptr;
  }

  const auto position = entries_.find(id);
  return position != entries_.end() ? &position->second : nullptr;
}

bool manager::store(object* instance, object_type_id type, memory_resource* resource) {
  if (instance == nullptr || !type) {
    return false;
  }

  const object_id id = instance->id();
  if (!id) {
    return false;
  }

  const auto result = entries_.emplace(id, entry{id, type, instance, resource});
  return result.second;
}

void manager::destroy_entry(const entry& stored) noexcept {
  (void)objects::destroy(*type_context_, stored.type, stored.instance, stored.resource);
}

} // namespace caors::objects
