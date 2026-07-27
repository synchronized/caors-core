// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/registry_state.h"

#include "caors/meta/detail/pending_type_record.h"
#include "caors/meta/registry.h"

#include <mutex>
#include <optional>
#include <typeindex>
#include <utility>

namespace caors::meta {
namespace {

[[nodiscard]] std::optional<type_id> find_existing_type_id(const detail::registry_state& state,
                                                           const detail::type_record& type,
                                                           error& conflict) {
  std::optional<type_id> existing_id;

  const auto merge_candidate = [&](const detail::type_record* candidate, error result) {
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

[[nodiscard]] error duplicate_identity_error(const detail::registry_state& state,
                                             const detail::type_record& type, type_id existing_id) {
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

[[nodiscard]] error merge_type_record(detail::registry_state& state, detail::type_record& stored,
                                      const detail::type_record& incoming,
                                      bool dependencies_valid) {
  if (!incoming.valid() || !incoming.id() || incoming.id() != stored.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  bool changed = false;

  if (incoming.parent_id()) {
    if (!state.types.contains(incoming.parent_id().value()) && !dependencies_valid) {
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

[[nodiscard]] error store_new_type(detail::registry_state& state, detail::type_record&& incoming,
                                   type_id* stored_id = nullptr, bool dependencies_valid = false) {
  if (!incoming.valid() || !incoming.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (state.types.contains(incoming.id().value())) {
    return {error_domain::meta, errc::duplicate_id};
  }

  if (incoming.parent_id() && !state.types.contains(incoming.parent_id().value()) &&
      !dependencies_valid) {
    return {error_domain::meta, errc::missing_parent};
  }

  for (std::size_t index = 0; index < incoming.nested_type_count(); ++index) {
    const detail::nested_type_data* nested_type = incoming.nested_type_at(index);
    if (nested_type == nullptr || !nested_type->valid()) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!state.types.contains(nested_type->nested_type.value()) && !dependencies_valid) {
      return {error_domain::meta, errc::missing_type};
    }
  }

  if (incoming.name() && state.names.contains(incoming.name())) {
    return {error_domain::meta, errc::duplicate_name};
  }

  if (incoming.guid() && state.guids.contains(incoming.guid().value)) {
    return {error_domain::meta, errc::duplicate_guid};
  }

  const type_id id = incoming.id();
  auto [position, inserted] = state.types.try_emplace(
      id.value(), memory::make_unique<detail::type_record>(state.resource, std::move(incoming)));
  if (!inserted) {
    return {error_domain::meta, errc::duplicate_id};
  }

  detail::type_record* stored = position->second.get();
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

[[nodiscard]] error store_static_type_record(detail::registry_state& state,
                                             const detail::type_record& fallback) {
  if (!fallback.valid() || !fallback.id() || !fallback.value_ops().valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  detail::type_record* stored = detail::find_type_record(state, fallback.id());
  if (stored == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  if (stored->name() && fallback.name() && stored->name() != fallback.name()) {
    return {error_domain::meta, errc::inconsistent_type};
  }
  if (stored->guid() && fallback.guid() && stored->guid() != fallback.guid()) {
    return {error_domain::meta, errc::inconsistent_type};
  }

  if (!stored->name() && fallback.name()) {
    if (state.names.contains(fallback.name())) {
      return {error_domain::meta, errc::duplicate_name};
    }
    stored->set_name(fallback.name());
    state.names.emplace(stored->name(), stored);
  }
  if (!stored->guid() && fallback.guid()) {
    if (state.guids.contains(fallback.guid().value)) {
      return {error_domain::meta, errc::duplicate_guid};
    }
    stored->set_guid(fallback.guid());
    state.guids.emplace(stored->guid().value, stored);
  }

  if (stored->value_ops().valid()) {
    if (!stored->static_model_matches(fallback)) {
      return {error_domain::meta, errc::inconsistent_type_model};
    }

    return {};
  }

  stored->set_static_data(fallback);
  return {};
}

[[nodiscard]] error register_type_impl(detail::registry_state& state, detail::type_record type,
                                       type_id* stored_id = nullptr,
                                       bool dependencies_valid = false) {
  if (!type.valid() || !type.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state.mutex);
  if (state.frozen) {
    return {error_domain::meta, errc::frozen};
  }
  error conflict{};
  const std::optional<type_id> existing_id = find_existing_type_id(state, type, conflict);
  if (conflict) {
    return conflict;
  }

  if (existing_id.has_value()) {
    detail::type_record* existing = detail::find_type_record(state, *existing_id);
    if (existing == nullptr) {
      return {error_domain::meta, errc::invalid_type};
    }

    const error status = merge_type_record(state, *existing, type, dependencies_valid);
    if (status.value() == errc::invalid_type) {
      return duplicate_identity_error(state, type, *existing_id);
    }

    if (stored_id != nullptr) {
      *stored_id = existing->id();
    }

    return status;
  }

  return store_new_type(state, std::move(type), stored_id, dependencies_valid);
}

[[nodiscard]] error validate_pending_type_locked(const detail::registry_state& state,
                                                 const detail::type_record& candidate,
                                                 const std::type_info& cpp_type,
                                                 bool dependencies_valid) {
  if (!candidate.valid() || !candidate.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (state.frozen) {
    return {error_domain::meta, errc::frozen};
  }

  if (!dependencies_valid) {
    return {error_domain::meta, candidate.parent_id() ? errc::missing_parent : errc::missing_type};
  }

  if (state.types.contains(candidate.id().value())) {
    const detail::type_record* existing = detail::find_type_record(state, candidate.id());
    return existing != nullptr && !existing->static_model_matches(candidate)
               ? error{error_domain::meta, errc::inconsistent_type_model}
               : error{error_domain::meta, errc::duplicate_id};
  }

  const std::type_index cpp_index{cpp_type};
  if (state.cpp_types.contains(cpp_index)) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (candidate.name() && state.names.contains(candidate.name())) {
    return {error_domain::meta, errc::duplicate_name};
  }

  if (candidate.guid() && state.guids.contains(candidate.guid().value)) {
    return {error_domain::meta, errc::duplicate_guid};
  }

  return {};
}

} // namespace

error registry::register_type(type_id id, string_name name, type_guid guid, type_id parent_id) {
  const bool dependencies_valid = !parent_id || contains(parent_id);
  if (!dependencies_valid) {
    return {error_domain::meta, errc::missing_parent};
  }
  return register_type_impl(*state_, detail::type_record{id, name, guid, parent_id}, nullptr,
                            dependencies_valid);
}

error registry::validate_type(const detail::pending_type_record& pending,
                              const std::type_info& cpp_type) const {
  error status = pending.validate();
  if (status) {
    return status;
  }

  const detail::type_record& candidate = pending.data();
  bool dependencies_valid = !candidate.parent_id() || contains(candidate.parent_id());
  for (std::size_t index = 0; dependencies_valid && index < candidate.nested_type_count();
       ++index) {
    const detail::nested_type_data* nested = candidate.nested_type_at(index);
    dependencies_valid = nested != nullptr && nested->valid() && contains(nested->nested_type);
  }

  std::lock_guard lock(state_->mutex);
  return validate_pending_type_locked(*state_, candidate, cpp_type, dependencies_valid);
}

error registry::commit_type(detail::pending_type_record pending, const std::type_info& cpp_type) {
  error status = pending.validate();
  if (status) {
    return status;
  }

  const detail::type_record& candidate = pending.data();
  bool dependencies_valid = !candidate.parent_id() || contains(candidate.parent_id());
  for (std::size_t index = 0; dependencies_valid && index < candidate.nested_type_count();
       ++index) {
    const detail::nested_type_data* nested = candidate.nested_type_at(index);
    dependencies_valid = nested != nullptr && nested->valid() && contains(nested->nested_type);
  }
  if (!dependencies_valid) {
    return {error_domain::meta, candidate.parent_id() && !contains(candidate.parent_id())
                                    ? errc::missing_parent
                                    : errc::missing_type};
  }

  std::lock_guard lock(state_->mutex);
  status = validate_pending_type_locked(*state_, candidate, cpp_type, dependencies_valid);
  if (status) {
    return status;
  }

  const std::type_index cpp_index{cpp_type};
  const type_id expected_id = candidate.id();
  status = store_new_type(*state_, std::move(pending).release(), nullptr, dependencies_valid);
  if (status) {
    return status;
  }

  detail::type_record* stored = detail::find_type_record(*state_, expected_id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::invalid_type};
  }

  state_->cpp_types.emplace(cpp_index, stored);

  return {};
}

error registry::ensure_static_type_record(const detail::type_record& fallback) {
  std::lock_guard lock(state_->mutex);
  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }
  if (!fallback.valid() || !fallback.id()) {
    return {error_domain::meta, errc::invalid_type};
  }

  return store_static_type_record(*state_, fallback);
}

error registry::set_type_guid(type_id id, type_guid guid) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }
  detail::type_record* stored = detail::find_type_record(*state_, id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!guid) {
    return {error_domain::meta, errc::duplicate_id};
  }

  if (stored->guid()) {
    return stored->guid() == guid ? error{error_domain::meta, errc::duplicate_id}
                                  : error{error_domain::meta, errc::invalid_type};
  }

  if (state_->guids.contains(guid.value)) {
    return {error_domain::meta, errc::duplicate_guid};
  }

  stored->set_guid(guid);
  state_->guids.emplace(guid.value, stored);
  return {};
}

error registry::register_cpp_type_info(const std::type_info& info, type_id id) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }
  if (!state_->types.contains(id.value())) {
    return {error_domain::meta, errc::missing_type};
  }

  const std::type_index index{info};
  const auto position = state_->cpp_types.find(index);
  if (position != state_->cpp_types.end()) {
    return position->second != nullptr && position->second->id() == id
               ? error{}
               : error{error_domain::meta, errc::invalid_type};
  }

  detail::type_record* stored = detail::find_type_record(*state_, id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::missing_type};
  }

  state_->cpp_types.emplace(index, stored);
  return {};
}

error registry::set_type_parent(type_id id, type_id parent_id) {
  return set_type_parent(id, parent_id, {});
}

error registry::set_type_parent(type_id id, type_id parent_id, detail::parent_cast_ops cast) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (!parent_id || !contains(parent_id)) {
    return {error_domain::meta, errc::missing_parent};
  }

  std::lock_guard lock(state_->mutex);
  if (state_->frozen) {
    return {error_domain::meta, errc::frozen};
  }
  detail::type_record* stored = detail::find_type_record(*state_, id);
  if (stored == nullptr) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (cast.parent && cast.parent != parent_id) {
    return {error_domain::meta, errc::invalid_type};
  }

  bool changed = false;
  if (stored->parent_id()) {
    if (stored->parent_id() != parent_id) {
      return {error_domain::meta, errc::invalid_type};
    }
  } else {
    stored->set_parent_id(parent_id);
    changed = true;
  }

  if (cast.valid()) {
    const detail::parent_cast_ops* existing_cast = stored->parent_cast();
    if (existing_cast != nullptr) {
      if (existing_cast->parent != parent_id || existing_cast->const_upcast != cast.const_upcast ||
          existing_cast->mutable_upcast != cast.mutable_upcast) {
        return {error_domain::meta, errc::invalid_type};
      }
    } else {
      stored->set_parent_cast(cast);
      changed = true;
    }
  }

  return changed ? error{} : error{error_domain::meta, errc::duplicate_id};
}

} // namespace caors::meta
