// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/index.h"

namespace caors::objects {

object_index::object_index(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      ids_by_guid_(guid_map::allocator_type{resource_}),
      guids_by_id_(id_map::allocator_type{resource_}) {}

object_index::~object_index() = default;

std::size_t object_index::size() const noexcept { return ids_by_guid_.size(); }

bool object_index::contains(object_guid guid) const { return find(guid) != object_id::invalid(); }

bool object_index::contains(object_id id) const { return static_cast<bool>(guid_of(id)); }

bool object_index::bind(object_guid guid, object_id id) {
  if (!guid || !id) {
    return false;
  }

  const auto guid_position = ids_by_guid_.find(guid);
  if (guid_position != ids_by_guid_.end()) {
    return guid_position->second == id;
  }

  const auto id_position = guids_by_id_.find(id);
  if (id_position != guids_by_id_.end()) {
    return id_position->second == guid;
  }

  ids_by_guid_.emplace(guid, id);
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    guids_by_id_.emplace(id, guid);
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    ids_by_guid_.erase(guid);
    throw;
  }
#endif

  return true;
}

bool object_index::unbind(object_guid guid) noexcept {
  const auto guid_position = ids_by_guid_.find(guid);
  if (guid_position == ids_by_guid_.end()) {
    return false;
  }

  const object_id id = guid_position->second;
  ids_by_guid_.erase(guid_position);
  guids_by_id_.erase(id);
  return true;
}

bool object_index::unbind(object_id id) noexcept {
  const auto id_position = guids_by_id_.find(id);
  if (id_position == guids_by_id_.end()) {
    return false;
  }

  const object_guid guid = id_position->second;
  guids_by_id_.erase(id_position);
  ids_by_guid_.erase(guid);
  return true;
}

void object_index::clear() noexcept {
  ids_by_guid_.clear();
  guids_by_id_.clear();
}

object_id object_index::find(object_guid guid) const {
  if (!guid) {
    return object_id::invalid();
  }

  const auto position = ids_by_guid_.find(guid);
  return position != ids_by_guid_.end() ? position->second : object_id::invalid();
}

object_guid object_index::guid_of(object_id id) const {
  if (!id) {
    return object_guid::nil();
  }

  const auto position = guids_by_id_.find(id);
  return position != guids_by_id_.end() ? position->second : object_guid::nil();
}

} // namespace caors::objects
