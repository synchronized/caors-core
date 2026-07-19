// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/manifest_file_system.h"

#include "caors/io/location.h"

#include <algorithm>
#include <utility>

namespace caors::resources {
namespace {

[[nodiscard]] std::string_view string_view_of(const core::string& value) noexcept {
  return {value.data(), value.size()};
}

[[nodiscard]] core::string normalize_manifest_location(io::path_view value,
                                                       memory_resource* resource) {
  return io::normalize_location(value, resource).to_string(resource);
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

[[nodiscard]] bool compare_entries(const io::directory_entry& lhs,
                                   const io::directory_entry& rhs) noexcept {
  return lhs.path.string_view() < rhs.path.string_view();
}

void add_or_update_entry(io::directory_entry_list& entries, std::string_view name,
                         io::path_view full_path, io::file_kind kind, memory_resource* resource) {
  const auto found =
      std::find_if(entries.begin(), entries.end(), [&](const io::directory_entry& entry) {
        return entry.name.string_view() == name;
      });
  if (found != entries.end()) {
    if (kind == io::file_kind::directory) {
      found->kind = io::file_kind::directory;
    }
    return;
  }

  entries.push_back(io::directory_entry{
      io::path{name, resource},
      io::path{full_path, resource},
      kind,
  });
}

} // namespace

manifest_file_system::manifest_file_system(const manifest& entries, memory_resource* resource)
    : entries_(&entries),
      resource_(resource != nullptr ? resource : get_default_memory_resource()) {}

manifest_file_system::manifest_file_system(const manifest& entries, io::file_system& target,
                                           memory_resource* resource)
    : manifest_file_system(entries, resource) {
  target_ = &target;
}

memory_resource* manifest_file_system::resource() const noexcept { return resource_; }

const manifest& manifest_file_system::entries() const noexcept { return *entries_; }

io::file_system* manifest_file_system::target() const noexcept { return target_; }

bool manifest_file_system::exists(io::path_view path) const {
  if (target_ != nullptr && target_->exists(path)) {
    return true;
  }

  return manifest_contains_file(path) || manifest_contains_directory(path);
}

io::file_system_capabilities manifest_file_system::capabilities() const noexcept {
  io::file_system_capabilities result{};
  if (target_ != nullptr) {
    result = target_->capabilities();
  }

  result.status = result.status || entries_ != nullptr;
  result.list = result.list || entries_ != nullptr;
  return result;
}

io::file_status_result manifest_file_system::status(io::path_view path) const {
  if (target_ != nullptr) {
    io::file_status_result target_status = target_->status(path);
    if (target_status.succeeded()) {
      return target_status;
    }
    if (target_status.error().value() != errc::unsupported &&
        target_status.error().value() != errc::not_found) {
      return target_status;
    }
  }

  if (manifest_contains_file(path)) {
    return ok(io::file_status{io::file_kind::file});
  }
  if (manifest_contains_directory(path)) {
    return ok(io::file_status{io::file_kind::directory});
  }

  return fail<io::file_status>({error_domain::io, errc::not_found});
}

io::directory_entry_list_result manifest_file_system::list(io::path_view directory,
                                                           memory_resource* resource) {
  io::directory_entry_list entries{allocator<io::directory_entry>{resource}};
  const core::string normalized_directory =
      normalize_manifest_location(directory, this->resource());
  const std::string_view directory_text = string_view_of(normalized_directory);
  bool exact_file = false;

  if (entries_ != nullptr) {
    entries_->for_each([&](const manifest_entry& entry) {
      const std::string_view candidate = entry.location.string_view();
      if (candidate == directory_text) {
        exact_file = true;
        return;
      }

      std::string_view suffix;
      if (!child_suffix(directory_text, candidate, suffix)) {
        return;
      }

      const std::size_t separator = suffix.find('/');
      const std::string_view name = suffix.substr(0, separator);
      const io::file_kind kind =
          separator == std::string_view::npos ? io::file_kind::file : io::file_kind::directory;
      const io::location full_path = directory_text.empty()
                                         ? io::normalize_location(name, resource)
                                         : io::join_location(directory_text, name, resource);
      add_or_update_entry(entries, name, full_path, kind, resource);
    });
  }

  if (entries.empty() && target_ != nullptr) {
    io::directory_entry_list_result target_entries = target_->list(directory, resource);
    if (target_entries.succeeded()) {
      return target_entries;
    }
    if (target_entries.error().value() != errc::unsupported &&
        target_entries.error().value() != errc::not_found) {
      return target_entries;
    }
  }

  if (entries.empty()) {
    if (directory_text.empty()) {
      return ok(std::move(entries));
    }
    if (exact_file) {
      return fail<io::directory_entry_list>({error_domain::io, errc::invalid_argument});
    }
    return fail<io::directory_entry_list>({error_domain::io, errc::not_found});
  }

  std::sort(entries.begin(), entries.end(), compare_entries);
  return ok(std::move(entries));
}

io::byte_buffer_result manifest_file_system::read_all(io::path_view path,
                                                      memory_resource* resource) {
  if (target_ == nullptr) {
    return fail<io::byte_buffer>({error_domain::io, errc::unsupported});
  }

  return target_->read_all(path, resource);
}

io::input_stream_result manifest_file_system::open_read(io::path_view path,
                                                        memory_resource* resource) {
  if (target_ == nullptr) {
    return fail<io::input_stream_ptr>({error_domain::io, errc::unsupported});
  }

  return target_->open_read(path, resource);
}

io::output_stream_result manifest_file_system::open_write(io::path_view path,
                                                          memory_resource* resource) {
  if (target_ == nullptr) {
    return fail<io::output_stream_ptr>({error_domain::io, errc::unsupported});
  }

  return target_->open_write(path, resource);
}

error manifest_file_system::write_all(io::path_view path, std::span<const std::byte> bytes) {
  if (target_ == nullptr) {
    return {error_domain::io, errc::unsupported};
  }

  return target_->write_all(path, bytes);
}

bool manifest_file_system::manifest_contains_file(io::path_view path) const {
  if (entries_ == nullptr) {
    return false;
  }

  const core::string normalized = normalize_manifest_location(path, resource_);
  const std::string_view text = string_view_of(normalized);
  bool found = false;
  entries_->for_each(
      [&](const manifest_entry& entry) { found = found || entry.location.string_view() == text; });
  return found;
}

bool manifest_file_system::manifest_contains_directory(io::path_view path) const {
  if (entries_ == nullptr) {
    return false;
  }

  const core::string normalized = normalize_manifest_location(path, resource_);
  const std::string_view text = string_view_of(normalized);
  bool found = text.empty();
  entries_->for_each([&](const manifest_entry& entry) {
    if (found) {
      return;
    }

    std::string_view suffix;
    found = child_suffix(text, entry.location.string_view(), suffix);
  });
  return found;
}

} // namespace caors::resources
