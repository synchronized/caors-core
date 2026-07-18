// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "registry_state.h"

#include "caors/meta/detail/type_descriptor.h"

#include <utility>

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

const parent_cast_ops* find_parent_cast(const registry_state& state, type_id id) {
  const type_data* stored = find_type_data(state, id);
  return stored != nullptr ? stored->parent_cast() : nullptr;
}

std::optional<type_id> find_existing_type_id(const registry_state& state, const type_data& type,
                                             error& conflict) {
  std::optional<type_id> existing_id;

  const auto merge_candidate = [&](const type_data* candidate, error result) {
    if (candidate == nullptr || !candidate->id()) {
      conflict = {error_domain::meta, errc::invalid_type};
      return false;
    }

    const type_id candidate_id = candidate->id();
    if (existing_id.has_value() && *existing_id != candidate_id) {
      conflict = result;
      return false;
    }

    existing_id = candidate_id;
    return true;
  };

  if (type.id()) {
    const auto position = state.types.find(type.id().value());
    if (position != state.types.end() &&
        !merge_candidate(position->second.get(), {error_domain::meta, errc::duplicate_id})) {
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

error duplicate_identity_error(const registry_state& state, const type_data& type,
                               type_id existing_id) {
  if (type.name()) {
    const auto name_position = state.names.find(type.name());
    if (name_position != state.names.end() && name_position->second != nullptr &&
        name_position->second->id() == existing_id) {
      return {error_domain::meta, errc::duplicate_name};
    }
  }

  if (type.guid()) {
    const auto guid_position = state.guids.find(type.guid().value);
    if (guid_position != state.guids.end() && guid_position->second != nullptr &&
        guid_position->second->id() == existing_id) {
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
    if (name_position != state.names.end() && name_position->second != &stored) {
      return {error_domain::meta, errc::duplicate_name};
    }

    if (stored.name() && stored.name() != incoming.name()) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!stored.name()) {
      stored.set_name(incoming.name());
      state.names.emplace(stored.name(), &stored);
      changed = true;
    }
  }

  if (incoming.guid()) {
    const auto guid_position = state.guids.find(incoming.guid().value);
    if (guid_position != state.guids.end() && guid_position->second != &stored) {
      return {error_domain::meta, errc::duplicate_guid};
    }

    if (stored.guid() && stored.guid() != incoming.guid()) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!stored.guid()) {
      stored.set_guid(incoming.guid());
      state.guids.emplace(stored.guid().value, &stored);
      changed = true;
    }
  }

  return changed ? error{} : error{error_domain::meta, errc::duplicate_id};
}

error store_new_type(registry_state& state, type_data&& incoming, type_id* stored_id) {
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
  auto [position, inserted] = state.types.try_emplace(
      id.value(), memory::make_unique<type_data>(state.resource, std::move(incoming)));
  if (!inserted) {
    return {error_domain::meta, errc::duplicate_id};
  }

  type_data* stored = position->second.get();
  if (stored->name()) {
    state.names.emplace(stored->name(), stored);
  }

  if (stored->guid()) {
    state.guids.emplace(stored->guid().value, stored);
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

  type_data* stored = find_type_data(state, descriptor.type);
  if (stored == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  const type_descriptor* existing = stored->descriptor();
  if (existing != nullptr) {
    if (existing->type != descriptor.type ||
        existing->value.layout.size != descriptor.value.layout.size ||
        existing->value.layout.alignment != descriptor.value.layout.alignment) {
      return {error_domain::meta, errc::invalid_type};
    }

    return {};
  }

  stored->set_descriptor(&descriptor);
  return {};
}

error index_type_extension(registry_state& state, type_id id, extension_key key) {
  if (!id || key == nullptr || !state.types.contains(id.value())) {
    return {error_domain::meta, errc::invalid_type};
  }

  auto [position, inserted] = state.extension_index.try_emplace(
      key, core::vector<type_id>{core::vector<type_id>::allocator_type{state.resource}});
  (void)inserted;

  core::vector<type_id>& ids = position->second;
  for (const type_id existing : ids) {
    if (existing == id) {
      return {};
    }
  }

  ids.push_back(id);
  return {};
}

error validate_registry(const registry_state& state) {
  for (const auto& [raw_id, stored] : state.types) {
    if (stored == nullptr || !stored->registered() || stored->id().value() != raw_id) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (stored->parent_id() && !state.types.contains(stored->parent_id().value())) {
      return {error_domain::meta, errc::missing_parent};
    }

    if (stored->name()) {
      const auto name_position = state.names.find(stored->name());
      if (name_position == state.names.end() || name_position->second != stored.get()) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }

    if (stored->guid()) {
      const auto guid_position = state.guids.find(stored->guid().value);
      if (guid_position == state.guids.end() || guid_position->second != stored.get()) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }

    const type_descriptor* descriptor = stored->descriptor();
    if (descriptor != nullptr && (!descriptor->valid() || descriptor->type != stored->id())) {
      return {error_domain::meta, errc::invalid_type};
    }

    const parent_cast_ops* cast = stored->parent_cast();
    if (cast != nullptr && (!cast->valid() || cast->parent != stored->parent_id() ||
                            !state.types.contains(cast->parent.value()))) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!stored->members_valid()) {
      return {error_domain::meta, errc::invalid_type};
    }
  }

  for (const auto& [index, stored] : state.cpp_types) {
    (void)index;
    if (stored == nullptr || find_type_data(state, stored->id()) != stored) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [name, stored] : state.names) {
    if (stored == nullptr || stored->name() != name) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [guid_value, stored] : state.guids) {
    if (stored == nullptr || stored->guid().value != guid_value) {
      return {error_domain::meta, errc::inconsistent_type};
    }
  }

  for (const auto& [key, ids] : state.extension_index) {
    if (key == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    for (const type_id id : ids) {
      const type_data* stored = find_type_data(state, id);
      if (stored == nullptr) {
        return {error_domain::meta, errc::missing_type};
      }
      if (!stored->contains_extension(key)) {
        return {error_domain::meta, errc::inconsistent_type};
      }
    }
  }

  return {};
}

} // namespace caors::meta::detail
