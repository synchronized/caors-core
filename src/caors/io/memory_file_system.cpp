// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/memory_file_system.h"

#include <utility>

namespace caors::io {

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

byte_buffer_result memory_file_system::read_all(path_view path, memory_resource* resource) {
  if (path.empty()) {
    return {{error_domain::io, errc::invalid_argument}, {}};
  }

  const auto found = files_.find(key_for(path));
  if (found == files_.end()) {
    return {{error_domain::io, errc::not_found}, {}};
  }

  byte_buffer bytes = make_byte_buffer(resource);
  bytes.assign(found->second.begin(), found->second.end());
  return {{}, std::move(bytes)};
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
