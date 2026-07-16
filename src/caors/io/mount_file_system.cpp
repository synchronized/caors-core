// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/mount_file_system.h"

#include <algorithm>

namespace caors::io {
namespace {

[[nodiscard]] bool is_separator(char value) noexcept { return value == '/' || value == '\\'; }

[[nodiscard]] std::string_view string_view_of(const core::string& value) noexcept {
  return {value.data(), value.size()};
}

[[nodiscard]] core::string make_string(std::string_view text, memory_resource* resource) {
  core::string result{allocator<char>{resource}};
  result.assign(text.begin(), text.end());
  return result;
}

[[nodiscard]] core::string normalize_prefix(path_view value, memory_resource* resource) {
  const std::string_view prefix = value.string_view();
  core::string result{allocator<char>{resource}};
  result.reserve(prefix.size());

  std::size_t begin = 0;
  while (begin < prefix.size() && is_separator(prefix[begin])) {
    ++begin;
  }

  std::size_t end = prefix.size();
  while (end > begin && is_separator(prefix[end - 1])) {
    --end;
  }

  for (std::size_t index = begin; index < end; ++index) {
    result.push_back(is_separator(prefix[index]) ? '/' : prefix[index]);
  }

  return result;
}

[[nodiscard]] core::string normalize_mount_path(path_view value, memory_resource* resource) {
  const std::string_view path = value.string_view();
  core::string result{allocator<char>{resource}};
  result.reserve(path.size());

  std::size_t begin = 0;
  while (begin < path.size() && is_separator(path[begin])) {
    ++begin;
  }

  for (std::size_t index = begin; index < path.size(); ++index) {
    result.push_back(is_separator(path[index]) ? '/' : path[index]);
  }

  return result;
}

[[nodiscard]] core::string trim_target_prefix(path_view value, memory_resource* resource) {
  const std::string_view prefix = value.string_view();
  std::size_t end = prefix.size();
  while (end > 0 && is_separator(prefix[end - 1])) {
    --end;
  }

  return make_string(prefix.substr(0, end), resource);
}

[[nodiscard]] bool matches_prefix(std::string_view prefix, std::string_view path) noexcept {
  if (prefix.empty()) {
    return true;
  }
  if (path == prefix) {
    return true;
  }

  return path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '/';
}

} // namespace

mount_file_system::mount_file_system(memory_resource* resource)
    : mounts_{allocator<mount_entry>{resource}} {}

memory_resource* mount_file_system::resource() const noexcept {
  return mounts_.get_allocator().resource();
}

error mount_file_system::mount(path_view prefix, file_system& target, path_view target_prefix) {
  core::string normalized_prefix = normalize_prefix(prefix, resource());
  if (std::any_of(mounts_.begin(), mounts_.end(),
                  [&](const mount_entry& entry) { return entry.prefix == normalized_prefix; })) {
    return {error_domain::io, errc::already_exists};
  }

  mount_entry entry{
      std::move(normalized_prefix),
      &target,
      trim_target_prefix(target_prefix, resource()),
  };
  mounts_.push_back(std::move(entry));
  return {};
}

bool mount_file_system::unmount(path_view prefix) {
  const core::string normalized_prefix = normalize_prefix(prefix, resource());
  const auto found = std::find_if(mounts_.begin(), mounts_.end(), [&](const mount_entry& entry) {
    return entry.prefix == normalized_prefix;
  });
  if (found == mounts_.end()) {
    return false;
  }

  mounts_.erase(found);
  return true;
}

void mount_file_system::clear() { mounts_.clear(); }

std::size_t mount_file_system::mount_count() const noexcept { return mounts_.size(); }

bool mount_file_system::exists(path_view path) const {
  if (path.empty()) {
    return false;
  }

  const core::string normalized_path = normalize_mount_path(path, resource());
  const mount_entry* entry = find_mount(path_view{normalized_path});
  return entry != nullptr && entry->target != nullptr &&
         entry->target->exists(resolve_path(*entry, path_view{normalized_path}));
}

byte_buffer_result mount_file_system::read_all(path_view path, memory_resource* resource) {
  if (path.empty()) {
    return fail<byte_buffer>({error_domain::io, errc::invalid_argument});
  }

  const core::string normalized_path = normalize_mount_path(path, this->resource());
  const mount_entry* entry = find_mount(path_view{normalized_path});
  if (entry == nullptr || entry->target == nullptr) {
    return fail<byte_buffer>({error_domain::io, errc::not_found});
  }

  return entry->target->read_all(resolve_path(*entry, path_view{normalized_path}), resource);
}

error mount_file_system::write_all(path_view path, std::span<const std::byte> bytes) {
  if (path.empty()) {
    return {error_domain::io, errc::invalid_argument};
  }

  const core::string normalized_path = normalize_mount_path(path, resource());
  const mount_entry* entry = find_mount(path_view{normalized_path});
  if (entry == nullptr || entry->target == nullptr) {
    return {error_domain::io, errc::not_found};
  }

  return entry->target->write_all(resolve_path(*entry, path_view{normalized_path}), bytes);
}

const mount_file_system::mount_entry*
mount_file_system::find_mount(path_view value) const noexcept {
  const std::string_view path = value.string_view();
  const mount_entry* result = nullptr;
  for (const mount_entry& entry : mounts_) {
    if (matches_prefix(string_view_of(entry.prefix), path) &&
        (result == nullptr || entry.prefix.size() > result->prefix.size())) {
      result = &entry;
    }
  }

  return result;
}

core::string mount_file_system::resolve_path(const mount_entry& entry, path_view value) const {
  const std::string_view path = value.string_view();
  std::string_view suffix = path;
  if (!entry.prefix.empty()) {
    suffix = path.size() == entry.prefix.size() ? std::string_view{}
                                                : path.substr(entry.prefix.size() + 1);
  }

  if (entry.target_prefix.empty()) {
    return make_string(suffix, resource());
  }
  if (suffix.empty()) {
    return make_string(string_view_of(entry.target_prefix), resource());
  }

  core::string result = make_string(string_view_of(entry.target_prefix), resource());
  result.push_back('/');
  result.append(suffix.begin(), suffix.end());
  return result;
}

} // namespace caors::io
