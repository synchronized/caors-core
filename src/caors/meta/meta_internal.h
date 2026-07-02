// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_INTERNAL_H_
#define CAORS_META_INTERNAL_H_

#include "caors/meta.h"

#include <mutex>

namespace caors {

class meta_registry_state {
public:
  mutable std::mutex mutex;
  std::uint64_t next_type_id = 1;
  bool frozen = false;
  bool registrations_ran = false;
  vector<meta_registration_fn> registrations;
  hash_map<std::uint64_t, std::unique_ptr<meta_type_data>> types;
  hash_map<string_name, type_id> names;
  hash_map<guid, type_id, guid_hash> guids;
  hash_map<detail::cpp_type_key, type_id> cpp_types;
  hash_map<detail::meta_component_key, std::unique_ptr<detail::meta_component_storage_base>>
      components;
};

namespace detail {

[[nodiscard]] type_id allocate_type_id(meta_registry_state& state);
[[nodiscard]] meta_type_data* find_type_data(meta_registry_state& state, type_id id);
[[nodiscard]] const meta_type_data* find_type_data(const meta_registry_state& state, type_id id);
[[nodiscard]] std::optional<type_id> find_existing_type_id(const meta_registry_state& state,
                                                           const meta_type_data& type,
                                                           error& conflict);
[[nodiscard]] error duplicate_identity_error(const meta_registry_state& state,
                                             const meta_type_data& type, type_id existing_id);
[[nodiscard]] bool has_cpp_type(const meta_registry_state& state, type_id id);
[[nodiscard]] error merge_type_data(meta_registry_state& state, meta_type_data& stored,
                                    const meta_type_data& incoming);
[[nodiscard]] error store_new_type(meta_registry_state& state, const meta_type_data& incoming,
                                   type_id* stored_id = nullptr);
[[nodiscard]] error validate_registry_for_freeze(const meta_registry_state& state);

} // namespace detail

} // namespace caors

#endif
