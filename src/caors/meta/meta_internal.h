// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_INTERNAL_H_
#define CAORS_META_INTERNAL_H_

#include "caors/meta.h"

#include <mutex>

namespace caors::meta {

class registry_state {
public:
  mutable std::mutex mutex;
  std::uint64_t next_type_id = 1;
  bool frozen = false;
  bool registrations_ran = false;
  core::vector<registration_fn> registrations;
  core::hash_map<std::uint64_t, std::unique_ptr<type_data>> types;
  core::hash_map<core::string_name, type_id> names;
  core::hash_map<core::guid, type_id, core::guid_hash> guids;
  core::hash_map<caors::detail::cpp_type_key, type_id> cpp_types;
  core::hash_map<detail::meta_component_key, std::unique_ptr<detail::meta_component_storage_base>>
      components;
};

namespace detail {

[[nodiscard]] type_id allocate_type_id(registry_state& state);
[[nodiscard]] type_data* find_type_data(registry_state& state, type_id id);
[[nodiscard]] const type_data* find_type_data(const registry_state& state, type_id id);
[[nodiscard]] std::optional<type_id> find_existing_type_id(const registry_state& state,
                                                           const type_data& type, error& conflict);
[[nodiscard]] error duplicate_identity_error(const registry_state& state, const type_data& type,
                                             type_id existing_id);
[[nodiscard]] bool has_cpp_type(const registry_state& state, type_id id);
[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);
[[nodiscard]] error store_new_type(registry_state& state, const type_data& incoming,
                                   type_id* stored_id = nullptr);
[[nodiscard]] error validate_registry_for_freeze(const registry_state& state);

} // namespace detail

} // namespace caors::meta

#endif
