// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/memory_file_system.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace caors::io {
namespace {

[[nodiscard]] std::string_view string_view_of(const core::string& value) noexcept {
  return {value.data(), value.size()};
}

[[nodiscard]] bool compare_entries(const directory_entry& lhs,
                                   const directory_entry& rhs) noexcept {
  return lhs.path.string_view() < rhs.path.string_view();
}

void add_or_update_entry(directory_entry_list& entries, std::string_view name, path_view full_path,
                         file_kind kind, memory_resource* resource) {
  const auto found =
      std::find_if(entries.begin(), entries.end(),
                   [&](const directory_entry& entry) { return entry.name.string_view() == name; });
  if (found != entries.end()) {
    if (kind == file_kind::directory) {
      found->kind = file_kind::directory;
    }
    return;
  }

  entries.push_back(directory_entry{
      path{name, resource},
      path{full_path, resource},
      kind,
  });
}

[[nodiscard]] bool child_suffix(std::string_view directory, std::string_view candidate,
                                std::string_view& suffix) noexcept {
  if (directory.empty()) {
    suffix = candidate;
    return !suffix.empty();
  }
  if (candidate.size() <= directory.size() || !candidate.starts_with(directory) ||
      candidate[directory.size()] != '/') {
    return false;
  }

  suffix = candidate.substr(directory.size() + 1);
  return !suffix.empty();
}

} // namespace

memory_file_system::memory_file_system(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      files_(typename file_map::allocator_type{resource_}) {}

memory_resource* memory_file_system::resource() const noexcept { return resource_; }

std::size_t memory_file_system::file_count() const noexcept { return files_.size(); }

bool memory_file_system::empty() const noexcept { return files_.empty(); }

bool memory_file_system::erase(path_view path) {
  if (path.empty()) {
    return false;
  }

  return files_.erase(key_for(path)) != 0;
}

void memory_file_system::clear() noexcept { files_.clear(); }

bool memory_file_system::exists(path_view path) const {
  if (path.empty()) {
    return false;
  }

  return files_.find(key_for(path)) != files_.end();
}

directory_entry_list_result memory_file_system::list(path_view directory,
                                                     memory_resource* resource) {
  const core::string normalized_directory = key_for(directory);
  const std::string_view directory_text = string_view_of(normalized_directory);
  directory_entry_list entries{allocator<directory_entry>{resource}};
  bool exact_file = false;

  for (const auto& [stored_path, bytes] : files_) {
    (void)bytes;

    const std::string_view candidate = string_view_of(stored_path);
    if (candidate == directory_text) {
      exact_file = true;
      continue;
    }

    std::string_view suffix;
    if (!child_suffix(directory_text, candidate, suffix)) {
      continue;
    }

    const std::size_t separator = suffix.find('/');
    const std::string_view name = suffix.substr(0, separator);
    const file_kind kind =
        separator == std::string_view::npos ? file_kind::file : file_kind::directory;
    const path full_path =
        directory_text.empty() ? path{name, resource} : join_path(directory_text, name, resource);
    add_or_update_entry(entries, name, full_path, kind, resource);
  }

  if (entries.empty()) {
    if (directory_text.empty()) {
      return ok(std::move(entries));
    }
    if (exact_file) {
      return fail<directory_entry_list>({error_domain::io, errc::invalid_argument});
    }
    return fail<directory_entry_list>({error_domain::io, errc::not_found});
  }

  std::sort(entries.begin(), entries.end(), compare_entries);
  return ok(std::move(entries));
}

byte_buffer_result memory_file_system::read_all(path_view path, memory_resource* resource) {
  if (path.empty()) {
    return fail<byte_buffer>({error_domain::io, errc::invalid_argument});
  }

  const auto found = files_.find(key_for(path));
  if (found == files_.end()) {
    return fail<byte_buffer>({error_domain::io, errc::not_found});
  }

  byte_buffer bytes = make_byte_buffer(resource);
  bytes.assign(found->second.begin(), found->second.end());
  return ok(std::move(bytes));
}

error memory_file_system::write_all(path_view path, std::span<const std::byte> bytes) {
  if (path.empty()) {
    return {error_domain::io, errc::invalid_argument};
  }

  byte_buffer stored = make_byte_buffer(resource_);
  stored.assign(bytes.begin(), bytes.end());

  const core::string key = key_for(path);
  auto found = files_.find(key);
  if (found != files_.end()) {
    found->second = std::move(stored);
    return {};
  }

  files_.emplace(key, std::move(stored));
  return {};
}

core::string memory_file_system::key_for(path_view path) const {
  return normalize_path(path, resource_).to_string(resource_);
}

} // namespace caors::io
