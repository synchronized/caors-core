// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/string_name.h"

#include "caors/core/containers.h"

#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

namespace caors::core {

namespace detail {

struct string_name_entry {
  string text;
};

} // namespace detail

namespace {

class string_name_table {
public:
  [[nodiscard]] const detail::string_name_entry* intern(std::string_view text) {
    if (text.empty()) {
      return nullptr;
    }

    std::lock_guard lock(mutex_);
    const auto existing = entries_by_text_.find(string{text});
    if (existing != entries_by_text_.end()) {
      return existing->second.get();
    }

    auto entry = std::make_unique<detail::string_name_entry>();
    entry->text = string{text};
    const detail::string_name_entry* stored = entry.get();
    entries_by_text_.emplace(stored->text, std::move(entry));
    return stored;
  }

  [[nodiscard]] const detail::string_name_entry* find_entry(std::string_view text) const {
    std::lock_guard lock(mutex_);
    const auto position = entries_by_text_.find(string{text});
    if (position == entries_by_text_.end()) {
      return nullptr;
    }

    return position->second.get();
  }

private:
  mutable std::mutex mutex_;
  hash_map<string, std::unique_ptr<detail::string_name_entry>> entries_by_text_;
};

[[nodiscard]] string_name_table& get_string_name_table() {
  static string_name_table table;
  return table;
}

} // namespace

string_name::string_name(const char* name)
    : string_name(name != nullptr ? std::string_view{name} : std::string_view{}) {}

string_name::string_name(std::string_view name) : entry_(get_string_name_table().intern(name)) {}

string_name::string_name(const std::string& name) : string_name(std::string_view{name}) {}

std::optional<string_name> string_name::find(std::string_view name) {
  const detail::string_name_entry* entry = get_string_name_table().find_entry(name);
  if (entry == nullptr) {
    return std::nullopt;
  }

  return string_name{entry};
}

std::string_view string_name::view() const noexcept {
  return entry_ != nullptr ? std::string_view{entry_->text} : std::string_view{};
}

std::string string_name::string() const { return std::string{view()}; }

bool operator==(string_name lhs, std::string_view rhs) { return lhs.view() == rhs; }

bool operator==(std::string_view lhs, string_name rhs) { return lhs == rhs.view(); }

bool operator!=(string_name lhs, std::string_view rhs) { return !(lhs == rhs); }

bool operator!=(std::string_view lhs, string_name rhs) { return !(lhs == rhs); }

} // namespace caors::core
