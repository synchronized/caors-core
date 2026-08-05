// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/registry.h"

#include <algorithm>
#include <mutex>

namespace caors::meta {
namespace {

[[nodiscard]] bool snapshot_contains(const detail::registry_snapshot& snapshot, type_id id) {
  for (const auto& layer : snapshot.layers()) {
    const detail::registry_state& state = layer->state();
    if (state.types.contains(id.value())) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] error validate_layer(const detail::registry_state& draft,
                                   const detail::registry_snapshot& current) {
  const error local_validation = detail::validate_registry(draft, &current);
  if (local_validation) {
    return local_validation;
  }

  for (const auto& [raw_id, stored] : draft.types) {
    (void)raw_id;
    if (stored == nullptr || !stored->registered() || !stored->members_valid()) {
      return {error_domain::meta, errc::invalid_type};
    }

    for (const auto& layer : current.layers()) {
      const detail::registry_state& state = layer->state();
      if (state.types.contains(stored->id().value())) {
        return {error_domain::meta, errc::duplicate_id};
      }
      if (stored->name() && state.names.contains(stored->name())) {
        return {error_domain::meta, errc::duplicate_name};
      }
      if (stored->guid() && state.guids.contains(stored->guid().value)) {
        return {error_domain::meta, errc::duplicate_guid};
      }
      for (const auto& [cpp_type, existing] : draft.cpp_types) {
        (void)existing;
        if (state.cpp_types.contains(cpp_type)) {
          return {error_domain::meta, errc::inconsistent_type};
        }
      }
    }

    const type_id parent = stored->parent_id();
    if (parent && !draft.types.contains(parent.value()) && !snapshot_contains(current, parent)) {
      return {error_domain::meta, errc::missing_parent};
    }
    for (std::size_t index = 0; index < stored->nested_type_count(); ++index) {
      const detail::nested_type_data* nested = stored->nested_type_at(index);
      if (nested == nullptr || !nested->valid() ||
          (!draft.types.contains(nested->nested_type.value()) &&
           !snapshot_contains(current, nested->nested_type))) {
        return {error_domain::meta, errc::missing_type};
      }
    }
  }
  return {};
}

} // namespace

error registry::publish_layer(registry_layer& layer) {
  if (layer.registry_ == nullptr || layer.published_ || layer.registry_ == this) {
    return {error_domain::meta, errc::invalid_argument};
  }

  detail::registry_state& draft = *layer.registry_->state_;
  std::scoped_lock lock{state_->mutex, draft.mutex};
  auto current = capture_snapshot();
  if (draft.frozen || draft.types.empty()) {
    return {error_domain::meta, errc::invalid_argument};
  }

  const error validation = validate_layer(draft, *current);
  if (validation) {
    return validation;
  }

  draft.frozen = true;
  layer.registry_->primary_layer_->freeze();
  core::vector<std::shared_ptr<detail::registry_layer_state>> layers{
      core::vector<std::shared_ptr<detail::registry_layer_state>>::allocator_type{
          state_resource_}};
  layers.reserve(current->layers().size() + 1);
  layers.push_back(layer.registry_->primary_layer_);
  layers.insert(layers.end(), current->layers().begin(), current->layers().end());
  snapshot_.store(std::make_shared<detail::registry_snapshot>(std::move(layers)),
                  std::memory_order_release);
  layer.published_ = true;
  return {};
}

error registry::detach_layer(registry_layer& layer) {
  if (layer.registry_ == nullptr || !layer.published_ || layer.registry_ == this) {
    return {error_domain::meta, errc::invalid_argument};
  }

  std::lock_guard lock{state_->mutex};
  auto current = capture_snapshot();
  const auto target = layer.registry_->primary_layer_;
  if (target->lifetime() == registry_layer_lifetime::pinned_process) {
    return {error_domain::meta, errc::unsupported};
  }
  const auto position = std::find(current->layers().begin(), current->layers().end(), target);
  if (position == current->layers().end()) {
    return {error_domain::meta, errc::not_found};
  }

  core::vector<std::shared_ptr<detail::registry_layer_state>> layers{
      core::vector<std::shared_ptr<detail::registry_layer_state>>::allocator_type{
          state_resource_}};
  layers.reserve(current->layers().size() - 1);
  for (const auto& candidate : current->layers()) {
    if (candidate != target) {
      layers.push_back(candidate);
    }
  }
  auto replacement = std::make_shared<detail::registry_snapshot>(std::move(layers));
  target->deactivate();
  snapshot_.store(std::move(replacement), std::memory_order_release);
  return {};
}

} // namespace caors::meta
