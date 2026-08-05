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

[[nodiscard]] path make_child_path(path_view directory, std::string_view name,
                                   memory_resource* resource) {
  return directory.empty() ? path{name, resource} : join_path(directory, name, resource);
}

[[nodiscard]] bool mount_child_suffix(std::string_view directory, std::string_view prefix,
                                      std::string_view& suffix) noexcept {
  if (prefix.empty()) {
    return false;
  }
  if (directory.empty()) {
    suffix = prefix;
    return true;
  }
  if (prefix.size() <= directory.size() || !prefix.starts_with(directory) ||
      prefix[directory.size()] != '/') {
    return false;
  }

  suffix = prefix.substr(directory.size() + 1);
  return !suffix.empty();
}

[[nodiscard]] bool is_mount_directory(std::string_view directory,
                                      std::string_view prefix) noexcept {
  if (directory.empty()) {
    return true;
  }
  if (directory == prefix) {
    return true;
  }

  std::string_view suffix;
  return mount_child_suffix(directory, prefix, suffix);
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

file_system_capabilities mount_file_system::capabilities() const noexcept {
  file_system_capabilities result{};
  result.status = !mounts_.empty();
  result.list = !mounts_.empty();

  for (const mount_entry& entry : mounts_) {
    if (entry.target == nullptr) {
      continue;
    }

    const file_system_capabilities target = entry.target->capabilities();
    result.read = result.read || target.read;
    result.write = result.write || target.write;
    result.status = result.status || target.status;
    result.list = result.list || target.list;
    result.watch = result.watch || target.watch;
    result.seek = result.seek || target.seek;
  }

  return result;
}

bool mount_file_system::exists(path_view path) const {
  if (path.empty()) {
    return false;
  }

  const core::string normalized_path = normalize_mount_path(path, resource());
  const mount_entry* entry = find_mount(path_view{normalized_path});
  return entry != nullptr && entry->target != nullptr &&
         entry->target->exists(resolve_path(*entry, path_view{normalized_path}));
}

file_status_result mount_file_system::status(path_view path) const {
  const core::string normalized_path = normalize_mount_path(path, resource());
  const std::string_view path_text = string_view_of(normalized_path);
  if (path_text.empty()) {
    return ok(file_status{file_kind::directory});
  }

  error target_status{};
  bool has_target_status = false;
  const mount_entry* entry = find_mount(path_view{normalized_path});
  if (entry != nullptr && entry->target != nullptr) {
    file_status_result result =
        entry->target->status(resolve_path(*entry, path_view{normalized_path}));
    if (result.succeeded()) {
      return result;
    }

    target_status = result.error();
    has_target_status = true;
  }

  for (const mount_entry& mounted : mounts_) {
    if (is_mount_directory(path_text, string_view_of(mounted.prefix))) {
      return ok(file_status{file_kind::directory});
    }
  }

  if (has_target_status) {
    return fail<file_status>(target_status);
  }
  return fail<file_status>({error_domain::io, errc::not_found});
}

directory_entry_list_result mount_file_system::list(path_view directory,
                                                    memory_resource* result_resource) {
  const core::string normalized_directory = normalize_mount_path(directory, this->resource());
  const std::string_view directory_text = string_view_of(normalized_directory);
  directory_entry_list entries{allocator<directory_entry>{result_resource}};

  error target_status{};
  bool has_target_status = false;
  bool target_succeeded = false;
  const mount_entry* entry = find_mount(path_view{normalized_directory});
  if (entry != nullptr && entry->target != nullptr) {
    directory_entry_list_result target_entries =
        entry->target->list(resolve_path(*entry, path_view{normalized_directory}), result_resource);
    if (target_entries.succeeded()) {
      target_succeeded = true;
      for (const directory_entry& target_entry : *target_entries) {
        const std::string_view name = target_entry.name.string_view();
        if (name.empty()) {
          continue;
        }

        const path full_path =
            make_child_path(path_view{normalized_directory}, name, result_resource);
        add_or_update_entry(entries, name, full_path, target_entry.kind, result_resource);
      }
    } else {
      target_status = target_entries.error();
      has_target_status = true;
    }
  }

  for (const mount_entry& mounted : mounts_) {
    std::string_view suffix;
    if (!mount_child_suffix(directory_text, string_view_of(mounted.prefix), suffix)) {
      continue;
    }

    const std::size_t separator = suffix.find('/');
    const std::string_view name = suffix.substr(0, separator);
    const path full_path = make_child_path(path_view{normalized_directory}, name, result_resource);
    add_or_update_entry(entries, name, full_path, file_kind::directory, result_resource);
  }

  if (entries.empty()) {
    if (target_succeeded || directory_text.empty()) {
      return ok(std::move(entries));
    }
    if (has_target_status) {
      return fail<directory_entry_list>(target_status);
    }
    return fail<directory_entry_list>({error_domain::io, errc::not_found});
  }

  std::sort(entries.begin(), entries.end(), compare_entries);
  return ok(std::move(entries));
}

byte_buffer_result mount_file_system::read_all(path_view path, memory_resource* result_resource) {
  if (path.empty()) {
    return fail<byte_buffer>({error_domain::io, errc::invalid_argument});
  }

  const core::string normalized_path = normalize_mount_path(path, this->resource());
  const mount_entry* entry = find_mount(path_view{normalized_path});
  if (entry == nullptr || entry->target == nullptr) {
    return fail<byte_buffer>({error_domain::io, errc::not_found});
  }

  return entry->target->read_all(resolve_path(*entry, path_view{normalized_path}), result_resource);
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
