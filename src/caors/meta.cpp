// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <mutex>
#include <string_view>
#include <unordered_map>

namespace caors {

class meta_registry_state {
public:
  mutable std::mutex mutex;
  std::uint64_t next_type_id = 1;
  std::unordered_map<std::uint64_t, std::unique_ptr<meta_type_data>> types;
  std::unordered_map<std::uint64_t, type_id> names;
  std::unordered_map<guid, type_id, guid_hash> guids;
  std::unordered_map<detail::cpp_type_key, type_id> cpp_types;
  std::unordered_map<detail::meta_component_key,
                     std::unique_ptr<detail::meta_component_storage_base>>
      components;
};

namespace {

[[nodiscard]] type_id allocate_type_id(meta_registry_state& state) {
  type_id id{};
  do {
    id = type_id{state.next_type_id++};
    if (state.next_type_id == 0) {
      ++state.next_type_id;
    }
  } while (!id || state.types.contains(id.value()));

  return id;
}

[[nodiscard]] meta_type_data* find_type_data(meta_registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

[[nodiscard]] std::optional<type_id> find_existing_type_id(const meta_registry_state& state,
                                                           const meta_type_data& type,
                                                           error& conflict) {
  std::optional<type_id> existing_id;

  const auto merge_candidate = [&](type_id candidate, error result) {
    if (existing_id.has_value() && *existing_id != candidate) {
      conflict = result;
      return false;
    }

    existing_id = candidate;
    return true;
  };

  if (type.id()) {
    if (!state.types.contains(type.id().value())) {
      conflict = {error_domain::meta, errc::invalid_type};
      return std::nullopt;
    }

    if (!merge_candidate(type.id(), {error_domain::meta, errc::duplicate_id})) {
      return std::nullopt;
    }
  }

  if (type.name()) {
    const auto name_position = state.names.find(type.name().id());
    if (name_position != state.names.end() &&
        !merge_candidate(name_position->second, {error_domain::meta, errc::duplicate_name})) {
      return std::nullopt;
    }
  }

  if (type.guid()) {
    const auto guid_position = state.guids.find(type.guid().value);
    if (guid_position != state.guids.end() &&
        !merge_candidate(guid_position->second, {error_domain::meta, errc::duplicate_guid})) {
      return std::nullopt;
    }
  }

  return existing_id;
}

[[nodiscard]] error duplicate_identity_error(const meta_registry_state& state,
                                             const meta_type_data& type, type_id existing_id) {
  if (type.name()) {
    const auto name_position = state.names.find(type.name().id());
    if (name_position != state.names.end() && name_position->second == existing_id) {
      return {error_domain::meta, errc::duplicate_name};
    }
  }

  if (type.guid()) {
    const auto guid_position = state.guids.find(type.guid().value);
    if (guid_position != state.guids.end() && guid_position->second == existing_id) {
      return {error_domain::meta, errc::duplicate_guid};
    }
  }

  return {error_domain::meta, errc::invalid_type};
}

[[nodiscard]] bool has_cpp_type(const meta_registry_state& state, type_id id) {
  for (const auto& [key, mapped_id] : state.cpp_types) {
    (void)key;
    if (mapped_id == id) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] error merge_type_data(meta_registry_state& state, meta_type_data& stored,
                                    const meta_type_data& incoming) {
  if (!incoming.valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (incoming.id() && incoming.id() != stored.id()) {
    return {error_domain::meta, errc::invalid_type};
  }
  const bool explicit_id = incoming.id() == stored.id();

  if (stored.size() != incoming.size() || stored.alignment() != incoming.alignment()) {
    return {error_domain::meta, errc::invalid_type};
  }

  bool changed = false;

  if (incoming.parent_id()) {
    if (!state.types.contains(incoming.parent_id().value())) {
      return {error_domain::meta, errc::missing_parent};
    }

    if (stored.parent_id() && stored.parent_id() != incoming.parent_id()) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!stored.parent_id()) {
      stored.set_parent_id(incoming.parent_id());
      changed = true;
    }
  }

  if (incoming.name()) {
    const auto name_position = state.names.find(incoming.name().id());
    if (name_position != state.names.end() && name_position->second != stored.id()) {
      return {error_domain::meta, errc::duplicate_name};
    }

    if (stored.name() && stored.name() != incoming.name()) {
      return explicit_id ? error{error_domain::meta, errc::invalid_type}
                         : error{error_domain::meta, errc::duplicate_name};
    }

    if (!stored.name()) {
      stored.set_name(incoming.name());
      state.names.emplace(stored.name().id(), stored.id());
      changed = true;
    }
  }

  if (incoming.guid()) {
    const auto guid_position = state.guids.find(incoming.guid().value);
    if (guid_position != state.guids.end() && guid_position->second != stored.id()) {
      return {error_domain::meta, errc::duplicate_guid};
    }

    if (stored.guid() && stored.guid() != incoming.guid()) {
      return explicit_id ? error{error_domain::meta, errc::invalid_type}
                         : error{error_domain::meta, errc::duplicate_guid};
    }

    if (!stored.guid()) {
      stored.set_guid(incoming.guid());
      state.guids.emplace(stored.guid().value, stored.id());
      changed = true;
    }
  }

  return changed ? error{} : error{error_domain::meta, errc::duplicate_id};
}

[[nodiscard]] error store_new_type(meta_registry_state& state, const meta_type_data& incoming,
                                   type_id* stored_id = nullptr) {
  if (!incoming.valid() || incoming.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (incoming.parent_id() && !state.types.contains(incoming.parent_id().value())) {
    return {error_domain::meta, errc::missing_parent};
  }

  if (incoming.name() && state.names.contains(incoming.name().id())) {
    return {error_domain::meta, errc::duplicate_name};
  }

  if (incoming.guid() && state.guids.contains(incoming.guid().value)) {
    return {error_domain::meta, errc::duplicate_guid};
  }

  meta_type_data stored = incoming;
  stored.set_id(allocate_type_id(state));
  const type_id id = stored.id();

  auto [position, inserted] =
      state.types.try_emplace(id.value(), std::make_unique<meta_type_data>(stored));
  if (!inserted) {
    return {error_domain::meta, errc::duplicate_id};
  }

  if (stored.name()) {
    state.names.emplace(stored.name().id(), id);
  }

  if (stored.guid()) {
    state.guids.emplace(stored.guid().value, id);
  }

  if (stored_id != nullptr) {
    *stored_id = position->second->id();
  }

  return {};
}

} // namespace

bool meta_type::valid() const noexcept { return data_ != nullptr && data_->registered(); }

type_id meta_type::id() const noexcept {
  return data_ != nullptr ? data_->id() : type_id::invalid();
}

string_name meta_type::name() const noexcept {
  return data_ != nullptr ? data_->name() : string_name{};
}

type_guid meta_type::guid() const noexcept {
  return data_ != nullptr ? data_->guid() : type_guid::nil();
}

std::size_t meta_type::size() const noexcept { return data_ != nullptr ? data_->size() : 0; }

std::size_t meta_type::alignment() const noexcept {
  return data_ != nullptr ? data_->alignment() : 0;
}

type_id meta_type::parent_id() const noexcept {
  return data_ != nullptr ? data_->parent_id() : type_id::invalid();
}

meta_type meta_type::parent() const noexcept {
  if (registry_ == nullptr || !parent_id()) {
    return {};
  }

  const std::optional<meta_type> parent_type = registry_->find_type(parent_id());
  return parent_type.value_or(meta_type{});
}

meta_registry::meta_registry() : state_(new meta_registry_state) {}

meta_registry::~meta_registry() { delete state_; }

meta_registry& meta_registry::instance() noexcept {
  static meta_registry registry;
  return registry;
}

error meta_registry::register_type(const meta_type_data& type) {
  if (!type.valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);

  error conflict{};
  const std::optional<type_id> existing_id = find_existing_type_id(*state_, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    meta_type_data* existing = find_type_data(*state_, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (has_cpp_type(*state_, *existing_id)) {
      return duplicate_identity_error(*state_, type, *existing_id);
    }

    const error status = merge_type_data(*state_, *existing, type);
    if (status.value() == errc::invalid_type) {
      return duplicate_identity_error(*state_, type, *existing_id);
    }

    return status;
  }

  return store_new_type(*state_, type);
}

error meta_registry::register_cpp_type(detail::cpp_type_key key, const meta_type_data& type) {
  if (key == nullptr) {
    return register_type(type);
  }

  if (!type.valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);

  const auto key_position = state_->cpp_types.find(key);
  if (key_position != state_->cpp_types.end()) {
    meta_type_data* existing = find_type_data(*state_, key_position->second);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    const error status = merge_type_data(*state_, *existing, type);
    if (status.value() == errc::invalid_type) {
      return {error_domain::meta, errc::invalid_type};
    }

    return status;
  }

  error conflict{};
  const std::optional<type_id> existing_id = find_existing_type_id(*state_, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    meta_type_data* existing = find_type_data(*state_, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (has_cpp_type(*state_, *existing_id)) {
      return duplicate_identity_error(*state_, type, *existing_id);
    }

    const error status = merge_type_data(*state_, *existing, type);
    if (status.value() == errc::invalid_type) {
      return duplicate_identity_error(*state_, type, *existing_id);
    }

    state_->cpp_types.emplace(key, *existing_id);
    return status.value() == errc::duplicate_id ? error{} : status;
  }

  type_id stored_id{};
  const error status = store_new_type(*state_, type, &stored_id);
  if (status.succeeded()) {
    state_->cpp_types.emplace(key, stored_id);
  }

  return status;
}

meta_type meta_registry::ensure_cpp_type(detail::cpp_type_key key, std::size_t size,
                                         std::size_t alignment) {
  if (key == nullptr || size == 0 || alignment == 0) {
    return {};
  }

  std::lock_guard lock(state_->mutex);
  const auto key_position = state_->cpp_types.find(key);
  if (key_position != state_->cpp_types.end()) {
    const meta_type_data* existing = find_type_data(*state_, key_position->second);
    return meta_type{this, existing};
  }

  meta_type_data data{
      allocate_type_id(*state_), string_name{}, type_guid::nil(), size, alignment,
      type_id::invalid(),
  };
  const type_id id = data.id();

  auto [position, inserted] =
      state_->types.emplace(id.value(), std::make_unique<meta_type_data>(data));
  if (!inserted) {
    return {};
  }

  state_->cpp_types.emplace(key, id);
  return meta_type{this, position->second.get()};
}

bool meta_registry::contains(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->types.contains(id.value());
}

std::optional<meta_type> meta_registry::find_cpp_type(detail::cpp_type_key key) const {
  if (key == nullptr) {
    return std::nullopt;
  }

  std::lock_guard lock(state_->mutex);
  const auto key_position = state_->cpp_types.find(key);
  if (key_position == state_->cpp_types.end()) {
    return std::nullopt;
  }

  const meta_type_data* type = find_type_data(*state_, key_position->second);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

std::optional<meta_type> meta_registry::find_type(type_id id) const {
  std::lock_guard lock(state_->mutex);
  const meta_type_data* type = find_type_data(*state_, id);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

std::optional<meta_type> meta_registry::find_type(type_guid guid) const {
  if (!guid) {
    return std::nullopt;
  }

  std::lock_guard lock(state_->mutex);
  const auto guid_position = state_->guids.find(guid.value);
  if (guid_position == state_->guids.end()) {
    return std::nullopt;
  }

  const meta_type_data* type = find_type_data(*state_, guid_position->second);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

std::optional<meta_type> meta_registry::find_type(std::string_view name) const {
  std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_type(*interned_name);
}

std::optional<meta_type> meta_registry::find_type(string_name name) const {
  std::lock_guard lock(state_->mutex);
  const auto name_position = state_->names.find(name.id());
  if (name_position == state_->names.end()) {
    return std::nullopt;
  }

  const meta_type_data* type = find_type_data(*state_, name_position->second);
  if (type == nullptr) {
    return std::nullopt;
  }

  return meta_type{this, type};
}

bool meta_registry::is_a(type_id id, type_id base_id) const {
  if (!id || !base_id) {
    return false;
  }

  std::lock_guard lock(state_->mutex);
  type_id current_id = id;
  while (current_id) {
    if (current_id == base_id) {
      return true;
    }

    const meta_type_data* type = find_type_data(*state_, current_id);
    if (type == nullptr) {
      return false;
    }

    current_id = type->parent_id();
  }

  return false;
}

std::size_t meta_registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

detail::meta_component_storage_base*
meta_registry::find_component_storage(detail::meta_component_key key) {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

const detail::meta_component_storage_base*
meta_registry::find_component_storage(detail::meta_component_key key) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

detail::meta_component_storage_base& meta_registry::ensure_component_storage(
    detail::meta_component_key key, std::unique_ptr<detail::meta_component_storage_base> storage) {
  std::lock_guard lock(state_->mutex);
  auto [position, inserted] = state_->components.try_emplace(key, nullptr);
  if (inserted) {
    position->second = std::move(storage);
  }

  return *position->second;
}

std::vector<type_id> meta_registry::view_component_ids(const detail::meta_component_key* keys,
                                                       std::size_t count) const {
  if (keys == nullptr || count == 0) {
    return {};
  }

  std::lock_guard lock(state_->mutex);
  const auto first_position = state_->components.find(keys[0]);
  if (first_position == state_->components.end()) {
    return {};
  }

  std::vector<type_id> candidate_ids = first_position->second->ids();
  std::vector<type_id> result;
  result.reserve(candidate_ids.size());

  for (const type_id id : candidate_ids) {
    bool matched = true;
    for (std::size_t index = 1; index < count; ++index) {
      const auto position = state_->components.find(keys[index]);
      if (position == state_->components.end() || !position->second->contains(id)) {
        matched = false;
        break;
      }
    }

    if (matched) {
      result.push_back(id);
    }
  }

  return result;
}

meta_registry& get_meta_registry() noexcept { return meta_registry::instance(); }

type_id make_type_id(string_name name) noexcept {
  if (!name) {
    return type_id::invalid();
  }

  const std::optional<meta_type> type = get_meta_registry().find_type(name);
  return type.has_value() ? type->id() : type_id::invalid();
}

type_id make_type_id(std::string_view name) {
  const std::optional<meta_type> type = get_meta_registry().find_type(name);
  return type.has_value() ? type->id() : type_id::invalid();
}

std::optional<meta_type> find_type(type_id id) { return get_meta_registry().find_type(id); }

std::optional<meta_type> find_type(type_guid guid) { return get_meta_registry().find_type(guid); }

std::optional<meta_type> find_type(std::string_view name) {
  return get_meta_registry().find_type(name);
}

std::optional<meta_type> find_type(string_name name) { return get_meta_registry().find_type(name); }

bool is_a(type_id id, type_id base_id) { return get_meta_registry().is_a(id, base_id); }

} // namespace caors
