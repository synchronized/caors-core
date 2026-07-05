// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_INTERNAL_H_
#define CAORS_META_DETAIL_INTERNAL_H_

#include "caors/meta.h"
#include "caors/meta/detail/type_capabilities.h"

#include <mutex>
#include <typeindex>

namespace caors::meta::detail {

class registry_state {
public:
  mutable std::mutex mutex;
  bool frozen = false;
  bool registrations_ran = false;
  core::vector<registration_fn> registrations;
  core::hash_map<std::uint64_t, std::unique_ptr<type_data>> types;
  core::hash_map<std::uint64_t, const type_descriptor*> descriptors;
  core::hash_map<std::uint64_t, value_factory_ops> factories;
  core::hash_map<std::type_index, type_id> cpp_type_ids;
  core::hash_map<std::uint64_t, parent_cast_data> parent_casts;
  core::hash_map<core::string_name, type_id> names;
  core::hash_map<core::guid, type_id, core::guid_hash> guids;
  core::hash_map<meta_component_key, std::unique_ptr<meta_component_storage_base>> components;
};

[[nodiscard]] type_data* find_type_data(registry_state& state, type_id id);
[[nodiscard]] const type_data* find_type_data(const registry_state& state, type_id id);
[[nodiscard]] const type_descriptor* find_type_descriptor(const registry_state& state, type_id id);
[[nodiscard]] value_factory_ops find_type_factory(const registry_state& state, type_id id);
[[nodiscard]] const parent_cast_data* find_parent_cast(const registry_state& state, type_id id);
[[nodiscard]] std::optional<type_id> find_existing_type_id(const registry_state& state,
                                                           const type_data& type, error& conflict);
[[nodiscard]] error duplicate_identity_error(const registry_state& state, const type_data& type,
                                             type_id existing_id);
[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);
[[nodiscard]] error store_new_type(registry_state& state, const type_data& incoming,
                                   type_id* stored_id = nullptr);
[[nodiscard]] error store_type_descriptor(registry_state& state, const type_descriptor& descriptor);
[[nodiscard]] error validate_registry_for_freeze(const registry_state& state);

} // namespace caors::meta::detail

#endif
