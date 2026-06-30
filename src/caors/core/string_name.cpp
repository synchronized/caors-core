// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/string_name.h"

#include "caors/core/containers.h"

#include <mutex>
#include <string_view>

namespace caors {
namespace {

class string_name_table {
public:
  [[nodiscard]] std::uint64_t intern(std::string_view text) {
    if (text.empty()) {
      return 0;
    }

    std::lock_guard lock(mutex_);
    const auto existing = ids_by_text_.find(string{text});
    if (existing != ids_by_text_.end()) {
      return existing->second;
    }

    std::uint64_t id = next_id_++;
    if (id == 0) {
      id = next_id_++;
    }

    string owned_text{text};
    ids_by_text_.emplace(owned_text, id);
    text_by_id_.emplace(id, std::move(owned_text));
    return id;
  }

  [[nodiscard]] std::optional<std::uint64_t> find_id(std::string_view text) const {
    std::lock_guard lock(mutex_);
    const auto position = ids_by_text_.find(string{text});
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
  hash_map<string, std::uint64_t> ids_by_text_;
  hash_map<std::uint64_t, string> text_by_id_;
};

[[nodiscard]] string_name_table& get_string_name_table() {
  static string_name_table table;
  return table;
}

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

}
