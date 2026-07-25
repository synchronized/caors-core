// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "registry_state.h"

#include <utility>

namespace caors::meta::detail {

registry_layer_state::registry_layer_state(memory::memory_resource* resource)
    : resource_(resource != nullptr ? resource : &memory::library_resource()),
      state_(memory::make<registry_state>(resource_, resource_)) {}

registry_layer_state::~registry_layer_state() { memory::destroy(state_, resource_); }

registry_snapshot::registry_snapshot(std::shared_ptr<registry_layer_state> primary)
    : layers_(core::vector<std::shared_ptr<registry_layer_state>>::allocator_type{
          &memory::library_resource()}) {
  layers_.push_back(std::move(primary));
}

registry_snapshot::registry_snapshot(core::vector<std::shared_ptr<registry_layer_state>> layers)
    : layers_(std::move(layers)) {}

type_record* find_type_record(registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

const type_record* find_type_record(const registry_state& state, type_id id) {
  const auto position = state.types.find(id.value());
  if (position == state.types.end()) {
    return nullptr;
  }

  return position->second.get();
}

namespace {

[[nodiscard]] bool snapshot_contains(const registry_snapshot* snapshot, type_id id) {
  if (snapshot == nullptr) {
    return false;
  }
  for (const auto& layer : snapshot->layers()) {
    if (layer->state().types.contains(id.value())) {
      return true;
    }
  }
  return false;
}

} // namespace

error validate_registry(const registry_state& state, const registry_snapshot* snapshot) {
  for (const auto& [raw_id, stored] : state.types) {
    if (stored == nullptr || !stored->registered() || stored->id().value() != raw_id) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (stored->parent_id() && !state.types.contains(stored->parent_id().value()) &&
        !snapshot_contains(snapshot, stored->parent_id())) {
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

    const parent_cast_ops* cast = stored->parent_cast();
    if (cast != nullptr && (!cast->valid() || cast->parent != stored->parent_id() ||
                            (!state.types.contains(cast->parent.value()) &&
                             !snapshot_contains(snapshot, cast->parent)))) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!stored->members_valid()) {
      return {error_domain::meta, errc::invalid_type};
    }

    for (std::size_t index = 0; index < stored->nested_type_count(); ++index) {
      const nested_type_data* nested_type = stored->nested_type_at(index);
      if (nested_type == nullptr || !nested_type->valid()) {
        return {error_domain::meta, errc::invalid_type};
      }
      if (!state.types.contains(nested_type->nested_type.value()) &&
          !snapshot_contains(snapshot, nested_type->nested_type)) {
        return {error_domain::meta, errc::missing_type};
      }
    }
  }

  for (const auto& [index, stored] : state.cpp_types) {
    (void)index;
    if (stored == nullptr || find_type_record(state, stored->id()) != stored) {
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

  return {};
}

} // namespace caors::meta::detail
