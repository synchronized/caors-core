// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_internal.h"

namespace caors::detail {

type_id allocate_type_id(meta_registry_state& state) {
  type_id id{};
  do {
    id = type_id{state.next_type_id++};
    if (state.next_type_id == 0) {
      ++state.next_type_id;
    }
  } while (!id || state.types.contains(id.value()));

  return id;
}

meta_type_data* find_type_data(meta_registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

const meta_type_data* find_type_data(const meta_registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

std::optional<type_id> find_existing_type_id(const meta_registry_state& state,
                                             const meta_type_data& type, error& conflict) {
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
    const auto name_position = state.names.find(type.name());
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

error duplicate_identity_error(const meta_registry_state& state, const meta_type_data& type,
                               type_id existing_id) {
  if (type.name()) {
    const auto name_position = state.names.find(type.name());
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

bool has_cpp_type(const meta_registry_state& state, type_id id) {
  const meta_type_data* type = find_type_data(state, id);
  return type != nullptr && type->cpp_type() != nullptr;
}

error merge_type_data(meta_registry_state& state, meta_type_data& stored,
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

  if (incoming.cpp_type() != nullptr) {
    const auto cpp_position = state.cpp_types.find(incoming.cpp_type());
    if (cpp_position != state.cpp_types.end() && cpp_position->second != stored.id()) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (stored.cpp_type() != nullptr && stored.cpp_type() != incoming.cpp_type()) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (stored.cpp_type() == nullptr) {
      stored.set_cpp_type(incoming.cpp_type());
      state.cpp_types.emplace(stored.cpp_type(), stored.id());
      changed = true;
    }
  }

  if (incoming.clone_value() != nullptr && stored.clone_value() == nullptr) {
    stored.set_clone_value(incoming.clone_value());
    changed = true;
  }

  if (incoming.default_construct_value() != nullptr &&
      stored.default_construct_value() == nullptr) {
    stored.set_default_construct_value(incoming.default_construct_value());
    changed = true;
  }

  if (incoming.destroy_value() != nullptr && stored.destroy_value() == nullptr) {
    stored.set_destroy_value(incoming.destroy_value());
    changed = true;
  }

  if (incoming.assign_value() != nullptr && stored.assign_value() == nullptr) {
    stored.set_assign_value(incoming.assign_value());
    changed = true;
  }

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
    const auto name_position = state.names.find(incoming.name());
    if (name_position != state.names.end() && name_position->second != stored.id()) {
      return {error_domain::meta, errc::duplicate_name};
    }

    if (stored.name() && stored.name() != incoming.name()) {
      return explicit_id ? error{error_domain::meta, errc::invalid_type}
                         : error{error_domain::meta, errc::duplicate_name};
    }

    if (!stored.name()) {
      stored.set_name(incoming.name());
      state.names.emplace(stored.name(), stored.id());
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

error store_new_type(meta_registry_state& state, const meta_type_data& incoming,
                     type_id* stored_id) {
  if (!incoming.valid() || incoming.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (incoming.parent_id() && !state.types.contains(incoming.parent_id().value())) {
    return {error_domain::meta, errc::missing_parent};
  }

  if (incoming.name() && state.names.contains(incoming.name())) {
    return {error_domain::meta, errc::duplicate_name};
  }

  if (incoming.guid() && state.guids.contains(incoming.guid().value)) {
    return {error_domain::meta, errc::duplicate_guid};
  }

  if (incoming.cpp_type() != nullptr && state.cpp_types.contains(incoming.cpp_type())) {
    return {error_domain::meta, errc::invalid_type};
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
    state.names.emplace(stored.name(), id);
  }

  if (stored.guid()) {
    state.guids.emplace(stored.guid().value, id);
  }

  if (stored.cpp_type() != nullptr) {
    state.cpp_types.emplace(stored.cpp_type(), id);
  }

  if (stored_id != nullptr) {
    *stored_id = position->second->id();
  }

  return {};
}

error validate_registry_for_freeze(const meta_registry_state& state) {
  for (const auto& [raw_id, stored] : state.types) {
    if (stored == nullptr || !stored->registered() || stored->id().value() != raw_id) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (stored->parent_id() && !state.types.contains(stored->parent_id().value())) {
      return {error_domain::meta, errc::missing_parent};
    }

    if (stored->name()) {
      const auto name_position = state.names.find(stored->name());
      if (name_position == state.names.end() || name_position->second != stored->id()) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }

    if (stored->guid()) {
      const auto guid_position = state.guids.find(stored->guid().value);
      if (guid_position == state.guids.end() || guid_position->second != stored->id()) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }

    if (stored->cpp_type() != nullptr) {
      const auto cpp_position = state.cpp_types.find(stored->cpp_type());
      if (cpp_position == state.cpp_types.end() || cpp_position->second != stored->id()) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }
  }

  for (const auto& [name, id] : state.names) {
    const meta_type_data* stored = find_type_data(state, id);
    if (stored == nullptr || stored->name() != name) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [guid_value, id] : state.guids) {
    const meta_type_data* stored = find_type_data(state, id);
    if (stored == nullptr || stored->guid().value != guid_value) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [key, id] : state.cpp_types) {
    const meta_type_data* stored = find_type_data(state, id);
    if (key == nullptr || stored == nullptr || stored->cpp_type() != key) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [key, storage] : state.components) {
    if (key == nullptr || storage == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    for (const type_id id : storage->ids()) {
      if (!state.types.contains(id.value())) {
        return {error_domain::meta, errc::missing_type};
      }
    }
  }

  return {};
}

} // namespace caors::detail
