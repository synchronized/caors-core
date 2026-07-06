// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <atomic>
#include <limits>

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
  std::uint64_t current = next_object_id.load(std::memory_order_relaxed);
  while (current != 0) {
    const std::uint64_t next =
        current == std::numeric_limits<std::uint64_t>::max() ? 0 : current + 1U;
    if (next_object_id.compare_exchange_weak(current, next, std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
      return object_id{current};
    }
  }

  return object_id::invalid();
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

object_index::object_index(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      ids_by_guid_(guid_map::allocator_type{resource_}),
      guids_by_id_(id_map::allocator_type{resource_}) {}

object_index::~object_index() = default;

std::size_t object_index::size() const noexcept { return ids_by_guid_.size(); }

bool object_index::contains(object_guid guid) const { return find(guid) != object_id::invalid(); }

bool object_index::contains(object_id id) const { return static_cast<bool>(guid_of(id)); }

bool object_index::bind(object_guid guid, object_id id) {
  if (!guid || !id) {
    return false;
  }

  const auto guid_position = ids_by_guid_.find(guid);
  if (guid_position != ids_by_guid_.end()) {
    return guid_position->second == id;
  }

  const auto id_position = guids_by_id_.find(id);
  if (id_position != guids_by_id_.end()) {
    return id_position->second == guid;
  }

  ids_by_guid_.emplace(guid, id);
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    guids_by_id_.emplace(id, guid);
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    ids_by_guid_.erase(guid);
    throw;
  }
#endif

  return true;
}

bool object_index::unbind(object_guid guid) noexcept {
  const auto guid_position = ids_by_guid_.find(guid);
  if (guid_position == ids_by_guid_.end()) {
    return false;
  }

  const object_id id = guid_position->second;
  ids_by_guid_.erase(guid_position);
  guids_by_id_.erase(id);
  return true;
}

bool object_index::unbind(object_id id) noexcept {
  const auto id_position = guids_by_id_.find(id);
  if (id_position == guids_by_id_.end()) {
    return false;
  }

  const object_guid guid = id_position->second;
  guids_by_id_.erase(id_position);
  ids_by_guid_.erase(guid);
  return true;
}

void object_index::clear() noexcept {
  ids_by_guid_.clear();
  guids_by_id_.clear();
}

object_id object_index::find(object_guid guid) const {
  if (!guid) {
    return object_id::invalid();
  }

  const auto position = ids_by_guid_.find(guid);
  return position != ids_by_guid_.end() ? position->second : object_id::invalid();
}

object_guid object_index::guid_of(object_id id) const {
  if (!id) {
    return object_guid::nil();
  }

  const auto position = guids_by_id_.find(id);
  return position != guids_by_id_.end() ? position->second : object_guid::nil();
}

manager::manager(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      entries_(entry_map::allocator_type{resource_}),
      pending_destroy_(entry_queue::allocator_type{resource_}) {}

manager::manager(object_index& index, memory_resource* resource)
    : resource_(resource != nullptr ? resource : index.resource()), index_(&index),
      entries_(entry_map::allocator_type{resource_}),
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

  if (!objects::is_a(stored->type, target.type())) {
    return nullptr;
  }

  return stored->instance;
}

object* manager::create(object_type_id id) {
  if (!id) {
    return nullptr;
  }

  object* instance = get_registry().create(id, resource_);
  if (instance == nullptr) {
    return nullptr;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    if (!store(instance, id, resource_)) {
      (void)get_registry().destroy(id, instance, resource_);
      return nullptr;
    }

    return instance;
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    (void)get_registry().destroy(id, instance, resource_);
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
  (void)get_registry().destroy(stored.type, stored.instance, stored.resource);
}

} // namespace caors::objects
