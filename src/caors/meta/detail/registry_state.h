// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_STATE_H_
#define CAORS_META_DETAIL_REGISTRY_STATE_H_

#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/detail/type_data.h"

#include <mutex>
#include <typeindex>

namespace caors::meta::detail {

struct type_descriptor;

class registry_state {
public:
  explicit registry_state(memory::memory_resource* internal_resource = nullptr)
      : resource(internal_resource != nullptr ? internal_resource : &memory::library_resource()),
        registrations(core::vector<registration_fn>::allocator_type{resource}),
        types(
            core::hash_map<std::uint64_t, memory::unique_ptr<type_data>>::allocator_type{resource}),
        cpp_types(core::hash_map<std::type_index, type_data*>::allocator_type{resource}),
        names(core::hash_map<core::string_name, type_data*>::allocator_type{resource}),
        guids(core::hash_map<core::guid, type_data*, core::guid_hash>::allocator_type{resource}),
        components(core::hash_map<meta_component_key,
                                  memory::unique_ptr<meta_component_storage_base>>::allocator_type{
            resource}) {}

  mutable std::mutex mutex;
  memory::memory_resource* resource = nullptr;
  bool registrations_ran = false;
  core::vector<registration_fn> registrations;
  core::hash_map<std::uint64_t, memory::unique_ptr<type_data>> types;
  core::hash_map<std::type_index, type_data*> cpp_types;
  core::hash_map<core::string_name, type_data*> names;
  core::hash_map<core::guid, type_data*, core::guid_hash> guids;
  core::hash_map<meta_component_key, memory::unique_ptr<meta_component_storage_base>> components;
};

[[nodiscard]] type_data* find_type_data(registry_state& state, type_id id);
[[nodiscard]] const type_data* find_type_data(const registry_state& state, type_id id);
[[nodiscard]] const parent_cast_ops* find_parent_cast(const registry_state& state, type_id id);
[[nodiscard]] std::optional<type_id> find_existing_type_id(const registry_state& state,
                                                           const type_data& type, error& conflict);
[[nodiscard]] error duplicate_identity_error(const registry_state& state, const type_data& type,
                                             type_id existing_id);
[[nodiscard]] error merge_type_data(registry_state& state, type_data& stored,
                                    const type_data& incoming);
[[nodiscard]] error store_new_type(registry_state& state, type_data&& incoming,
                                   type_id* stored_id = nullptr);
[[nodiscard]] error store_type_descriptor(registry_state& state, const type_descriptor& descriptor);
[[nodiscard]] error validate_registry(const registry_state& state);

} // namespace caors::meta::detail

#endif
