// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace caors {
namespace {

class string_name_table {
public:
  [[nodiscard]] std::uint64_t intern(std::string_view text) {
    if (text.empty()) {
      return 0;
    }

    std::lock_guard lock(mutex_);
    const auto existing = ids_by_text_.find(std::string(text));
    if (existing != ids_by_text_.end()) {
      return existing->second;
    }

    std::uint64_t id = next_id_++;
    if (id == 0) {
      id = next_id_++;
    }

    std::string owned_text{text};
    ids_by_text_.emplace(owned_text, id);
    text_by_id_.emplace(id, std::move(owned_text));
    return id;
  }

  [[nodiscard]] std::optional<std::uint64_t> find_id(std::string_view text) const {
    std::lock_guard lock(mutex_);
    const auto position = ids_by_text_.find(std::string(text));
    if (position == ids_by_text_.end()) {
      return std::nullopt;
    }

    return position->second;
  }

  [[nodiscard]] std::string_view find_text(std::uint64_t id) const noexcept {
    std::lock_guard lock(mutex_);
    const auto position = text_by_id_.find(id);
    if (position == text_by_id_.end()) {
      return {};
    }

    return position->second;
  }

private:
  mutable std::mutex mutex_;
  std::uint64_t next_id_ = 1;
  std::unordered_map<std::string, std::uint64_t> ids_by_text_;
  std::unordered_map<std::uint64_t, std::string> text_by_id_;
};

[[nodiscard]] string_name_table& get_string_name_table() {
  static string_name_table table;
  return table;
}

struct type_info_entry {
  type_info info;
};

meta_registry default_meta_registry;

} // namespace

string_name::string_name(const char* name)
    : string_name(name != nullptr ? std::string_view{name} : std::string_view{}) {}

string_name::string_name(std::string_view name) : id_(get_string_name_table().intern(name)) {}

string_name::string_name(const std::string& name) : string_name(std::string_view{name}) {}

std::optional<string_name> string_name::find(std::string_view name) {
  std::optional<std::uint64_t> id = get_string_name_table().find_id(name);
  if (!id.has_value()) {
    return std::nullopt;
  }

  return string_name{*id};
}

std::string_view string_name::view() const noexcept {
  return get_string_name_table().find_text(id_);
}

std::string string_name::string() const { return std::string{view()}; }

bool operator==(string_name lhs, std::string_view rhs) { return lhs.view() == rhs; }

bool operator==(std::string_view lhs, string_name rhs) { return lhs == rhs.view(); }

bool operator!=(string_name lhs, std::string_view rhs) { return !(lhs == rhs); }

bool operator!=(std::string_view lhs, string_name rhs) { return !(lhs == rhs); }

class meta_registry_state {
public:
  mutable std::mutex mutex;
  std::unordered_map<std::uint64_t, type_info_entry> types;
  std::unordered_map<std::uint64_t, type_id> names;
  std::unordered_map<detail::meta_component_key,
                     std::unique_ptr<detail::meta_component_storage_base>>
      components;
};

meta_registry::meta_registry() : state_(new meta_registry_state) {}

meta_registry::~meta_registry() { delete state_; }

type_register_result meta_registry::register_type(const type_info& type) {
  if (!type.valid()) {
    return type_register_result::invalid_type;
  }

  std::lock_guard lock(state_->mutex);
  const auto id_position = state_->types.find(type.id.value());
  if (id_position != state_->types.end()) {
    if (id_position->second.info.name == type.name) {
      return type_register_result::duplicate_id;
    }

    return type_register_result::hash_collision;
  }

  if (state_->names.contains(type.name.id())) {
    return type_register_result::duplicate_name;
  }

  if (type.id != make_type_id(type.name)) {
    return type_register_result::invalid_type;
  }

  if (type.parent_id && !state_->types.contains(type.parent_id.value())) {
    return type_register_result::missing_parent;
  }

  auto [position, inserted] = state_->types.try_emplace(type.id.value());
  if (!inserted) {
    return type_register_result::duplicate_id;
  }

  position->second.info = type;
  state_->names.emplace(type.name.id(), type.id);

  return type_register_result::success;
}

bool meta_registry::contains(type_id id) const {
  std::lock_guard lock(state_->mutex);
  return state_->types.contains(id.value());
}

std::optional<type_info> meta_registry::find_type(type_id id) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->types.find(id.value());
  if (position == state_->types.end()) {
    return std::nullopt;
  }

  return position->second.info;
}

std::optional<type_info> meta_registry::find_type(std::string_view name) const {
  std::optional<string_name> interned_name = string_name::find(name);
  if (!interned_name.has_value()) {
    return std::nullopt;
  }

  return find_type(*interned_name);
}

std::optional<type_info> meta_registry::find_type(string_name name) const {
  std::lock_guard lock(state_->mutex);
  const auto name_position = state_->names.find(name.id());
  if (name_position == state_->names.end()) {
    return std::nullopt;
  }

  const auto type_position = state_->types.find(name_position->second.value());
  if (type_position == state_->types.end()) {
    return std::nullopt;
  }

  return type_position->second.info;
}

bool meta_registry::is_a(type_id id, type_id base_id) const {
  if (!id || !base_id) {
    return false;
  }

  std::lock_guard lock(state_->mutex);
  type_id current_id = id;
  while (current_id) {
    if (current_id == base_id) {
      return true;
    }

    const auto position = state_->types.find(current_id.value());
    if (position == state_->types.end()) {
      return false;
    }

    current_id = position->second.info.parent_id;
  }

  return false;
}

std::size_t meta_registry::size() const noexcept {
  std::lock_guard lock(state_->mutex);
  return state_->types.size();
}

void meta_registry::clear() noexcept {
  std::lock_guard lock(state_->mutex);
  state_->types.clear();
  state_->names.clear();
  for (auto& [key, storage] : state_->components) {
    (void)key;
    storage->clear();
  }
}

detail::meta_component_storage_base*
meta_registry::find_component_storage(detail::meta_component_key key) {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

const detail::meta_component_storage_base*
meta_registry::find_component_storage(detail::meta_component_key key) const {
  std::lock_guard lock(state_->mutex);
  const auto position = state_->components.find(key);
  if (position == state_->components.end()) {
    return nullptr;
  }

  return position->second.get();
}

detail::meta_component_storage_base& meta_registry::ensure_component_storage(
    detail::meta_component_key key, std::unique_ptr<detail::meta_component_storage_base> storage) {
  std::lock_guard lock(state_->mutex);
  auto [position, inserted] = state_->components.try_emplace(key, nullptr);
  if (inserted) {
    position->second = std::move(storage);
  }

  return *position->second;
}

std::vector<type_id> meta_registry::view_component_ids(const detail::meta_component_key* keys,
                                                       std::size_t count) const {
  if (keys == nullptr || count == 0) {
    return {};
  }

  std::lock_guard lock(state_->mutex);
  const auto first_position = state_->components.find(keys[0]);
  if (first_position == state_->components.end()) {
    return {};
  }

  std::vector<type_id> candidate_ids = first_position->second->ids();
  std::vector<type_id> result;
  result.reserve(candidate_ids.size());

  for (const type_id id : candidate_ids) {
    bool matched = true;
    for (std::size_t index = 1; index < count; ++index) {
      const auto position = state_->components.find(keys[index]);
      if (position == state_->components.end() || !position->second->contains(id)) {
        matched = false;
        break;
      }
    }

    if (matched) {
      result.push_back(id);
    }
  }

  return result;
}

meta_registry& get_meta_registry() noexcept { return default_meta_registry; }

}
