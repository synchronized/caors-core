// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_STATE_H_
#define CAORS_META_DETAIL_REGISTRY_STATE_H_

#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/detail/type_record.h"
#include "caors/meta/registration.h"

#include <memory>
#include <mutex>
#include <typeindex>

namespace caors::meta::detail {

class registry_state {
public:
  explicit registry_state(memory::memory_resource* internal_resource = nullptr)
      : resource(internal_resource != nullptr ? internal_resource : &memory::library_resource()),
        registrations(core::vector<registration_fn>::allocator_type{resource}),
        types(core::hash_map<std::uint64_t, memory::unique_ptr<type_record>>::allocator_type{
            resource}),
        cpp_types(core::hash_map<std::type_index, type_record*>::allocator_type{resource}),
        names(core::hash_map<core::string_name, type_record*>::allocator_type{resource}),
        guids(core::hash_map<core::guid, type_record*, core::guid_hash>::allocator_type{resource}) {
  }

  mutable std::mutex mutex;
  memory::memory_resource* resource = nullptr;
  bool registrations_ran = false;
  core::vector<registration_fn> registrations;
  core::hash_map<std::uint64_t, memory::unique_ptr<type_record>> types;
  core::hash_map<std::type_index, type_record*> cpp_types;
  core::hash_map<core::string_name, type_record*> names;
  core::hash_map<core::guid, type_record*, core::guid_hash> guids;
};

/** @brief 一次 registry 查询固定持有的状态快照所有者。 */
class registry_snapshot final {
public:
  explicit registry_snapshot(memory::memory_resource* resource);
  ~registry_snapshot();

  registry_snapshot(const registry_snapshot&) = delete;
  registry_snapshot& operator=(const registry_snapshot&) = delete;

  [[nodiscard]] registry_state& state() noexcept { return *state_; }
  [[nodiscard]] const registry_state& state() const noexcept { return *state_; }

private:
  memory::memory_resource* resource_ = nullptr;
  registry_state* state_ = nullptr;
};

[[nodiscard]] type_record* find_type_record(registry_state& state, type_id id);
[[nodiscard]] const type_record* find_type_record(const registry_state& state, type_id id);
[[nodiscard]] error validate_registry(const registry_state& state);

} // namespace caors::meta::detail

#endif
