// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "internal.h"

#include "caors/meta/detail/type_descriptor.h"

namespace caors::meta::detail {

type_data* find_type_data(registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

const type_data* find_type_data(const registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

const type_descriptor* find_type_descriptor(const registry_state& state, type_id id) {
  const auto position = state.descriptors.find(id.value());
  if (position == state.descriptors.end()) {
    return nullptr;
  }

  return position->second;
}

const parent_cast_ops* find_parent_cast(const registry_state& state, type_id id) {
  const auto position = state.parent_casts.find(id.value());
  if (position == state.parent_casts.end()) {
    return nullptr;
  }

  return &position->second;
}

std::optional<type_id> find_existing_type_id(const registry_state& state, const type_data& type,
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

  if (type.id() && state.types.contains(type.id().value()) &&
      !merge_candidate(type.id(), {error_domain::meta, errc::duplicate_id})) {
    return std::nullopt;
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

error duplicate_identity_error(const registry_state& state, const type_data& type,
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

error merge_type_data(registry_state& state, type_data& stored, const type_data& incoming) {
  if (!incoming.valid() || !incoming.id() || incoming.id() != stored.id()) {
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
    const auto name_position = state.names.find(incoming.name());
    if (name_position != state.names.end() && name_position->second != stored.id()) {
      return {error_domain::meta, errc::duplicate_name};
    }

    if (stored.name() && stored.name() != incoming.name()) {
      return {error_domain::meta, errc::invalid_type};
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
      return {error_domain::meta, errc::invalid_type};
    }

    if (!stored.guid()) {
      stored.set_guid(incoming.guid());
      state.guids.emplace(stored.guid().value, stored.id());
      changed = true;
    }
  }

  return changed ? error{} : error{error_domain::meta, errc::duplicate_id};
}

error store_new_type(registry_state& state, const type_data& incoming, type_id* stored_id) {
  if (!incoming.valid() || !incoming.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (state.types.contains(incoming.id().value())) {
    return {error_domain::meta, errc::duplicate_id};
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

  const type_id id = incoming.id();
  auto [position, inserted] =
      state.types.try_emplace(id.value(), memory::make_unique<type_data>(state.resource, incoming));
  if (!inserted) {
    return {error_domain::meta, errc::duplicate_id};
  }

  const type_data& stored = *position->second;
  if (stored.name()) {
    state.names.emplace(stored.name(), id);
  }

  if (stored.guid()) {
    state.guids.emplace(stored.guid().value, id);
  }

  if (stored_id != nullptr) {
    *stored_id = id;
  }

  return {};
}

error store_type_descriptor(registry_state& state, const type_descriptor& descriptor) {
  if (!descriptor.valid() || !descriptor.type) {
    return {error_domain::meta, errc::invalid_type};
  }

  const auto position = state.descriptors.find(descriptor.type.value());
  if (position != state.descriptors.end()) {
    const type_descriptor* existing = position->second;
    if (existing == nullptr || existing->type != descriptor.type ||
        existing->value.layout.size != descriptor.value.layout.size ||
        existing->value.layout.alignment != descriptor.value.layout.alignment) {
      return {error_domain::meta, errc::invalid_type};
    }

    return {};
  }

  state.descriptors.emplace(descriptor.type.value(), &descriptor);
  return {};
}

error validate_registry_for_freeze(const registry_state& state) {
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
  }

  for (const auto& [raw_id, descriptor] : state.descriptors) {
    if (descriptor == nullptr || !descriptor->valid() || descriptor->type.value() != raw_id) {
      return {error_domain::meta, errc::invalid_type};
    }
  }

  for (const auto& [raw_id, cast] : state.parent_casts) {
    if (!cast.valid() || !state.types.contains(raw_id) ||
        !state.types.contains(cast.parent.value())) {
      return {error_domain::meta, errc::invalid_type};
    }

    const type_data* stored = find_type_data(state, type_id{raw_id});
    if (stored == nullptr || stored->parent_id() != cast.parent) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [name, id] : state.names) {
    const type_data* stored = find_type_data(state, id);
    if (stored == nullptr || stored->name() != name) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [guid_value, id] : state.guids) {
    const type_data* stored = find_type_data(state, id);
    if (stored == nullptr || stored->guid().value != guid_value) {
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

} // namespace caors::meta::detail
