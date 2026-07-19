// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/source.h"

#include <utility>

namespace caors::resources {
namespace {

[[nodiscard]] core::string make_location(io::path_view location, memory_resource* resource) {
  const std::string_view text = location.string_view();
  core::string result{allocator<char>{resource}};
  result.assign(text.begin(), text.end());
  return result;
}

} // namespace

source::~source() = default;

io::input_stream_result source::open_read(io::path_view location, memory_resource* resource) {
  io::byte_buffer_result bytes = read(location, resource);
  if (bytes.failed()) {
    return fail<io::input_stream_ptr>(bytes.error());
  }

  return ok(io::make_input_stream(std::move(bytes.value), resource));
}

error source::write(io::path_view, std::span<const std::byte>) {
  return {error_domain::resources, errc::unsupported};
}

file_source::file_source(io::file_system& file_system) noexcept : file_system_(&file_system) {}

io::file_system& file_source::file_system() const noexcept { return *file_system_; }

bool file_source::exists(io::path_view location) const {
  return file_system_ != nullptr && file_system_->exists(location);
}

io::byte_buffer_result file_source::read(io::path_view location, memory_resource* resource) {
  if (file_system_ == nullptr) {
    return fail<io::byte_buffer>({error_domain::resources, errc::backend_failure});
  }

  return file_system_->read_all(location, resource);
}

io::input_stream_result file_source::open_read(io::path_view location, memory_resource* resource) {
  if (file_system_ == nullptr) {
    return fail<io::input_stream_ptr>({error_domain::resources, errc::backend_failure});
  }

  return file_system_->open_read(location, resource);
}

error file_source::write(io::path_view location, std::span<const std::byte> bytes) {
  if (file_system_ == nullptr) {
    return {error_domain::resources, errc::backend_failure};
  }

  return file_system_->write_all(location, bytes);
}

provider::provider(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      locations_(typename location_map::allocator_type{resource_}) {}

memory_resource* provider::resource() const noexcept { return resource_; }

error provider::bind(resource_guid guid, resources::source& source, io::path_view location) {
  if (!guid || location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }
  if (contains(guid)) {
    return {error_domain::resources, errc::already_exists};
  }

  locations_.emplace(guid, entry{&source, make_location(location, resource_)});
  return {};
}

error provider::rebind(resource_guid guid, resources::source& source, io::path_view location) {
  if (!guid || location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  entry updated{&source, make_location(location, resource_)};
  const auto position = locations_.find(guid);
  if (position == locations_.end()) {
    locations_.emplace(guid, std::move(updated));
    return {};
  }

  position->second = std::move(updated);
  return {};
}

bool provider::unbind(resource_guid guid) { return locations_.erase(guid) != 0; }

void provider::clear() noexcept { locations_.clear(); }

std::size_t provider::size() const noexcept { return locations_.size(); }

bool provider::empty() const noexcept { return locations_.empty(); }

bool provider::contains(resource_guid guid) const {
  return guid && locations_.find(guid) != locations_.end();
}

source_location provider::find(resource_guid guid) const {
  const auto position = locations_.find(guid);
  if (position == locations_.end()) {
    return {};
  }

  return {position->second.source, io::path_view{position->second.location}};
}

bool provider::exists(resource_guid guid) const {
  const source_location location = find(guid);
  return location && location.source->exists(location.location);
}

io::byte_buffer_result provider::read(resource_guid guid, memory_resource* resource) const {
  if (!guid) {
    return fail<io::byte_buffer>({error_domain::resources, errc::invalid_argument});
  }

  const source_location location = find(guid);
  if (!location) {
    return fail<io::byte_buffer>({error_domain::resources, errc::not_found});
  }

  return location.source->read(location.location, resource);
}

io::input_stream_result provider::open_read(resource_guid guid, memory_resource* resource) const {
  if (!guid) {
    return fail<io::input_stream_ptr>({error_domain::resources, errc::invalid_argument});
  }

  const source_location location = find(guid);
  if (!location) {
    return fail<io::input_stream_ptr>({error_domain::resources, errc::not_found});
  }

  return location.source->open_read(location.location, resource);
}

error provider::write(resource_guid guid, std::span<const std::byte> bytes) const {
  if (!guid) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const source_location location = find(guid);
  if (!location) {
    return {error_domain::resources, errc::not_found};
  }

  return location.source->write(location.location, bytes);
}

} // namespace caors::resources
