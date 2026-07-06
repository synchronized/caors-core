// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/io.h"

#include <cstdint>
#include <string_view>

namespace caors::resources {
namespace {

[[nodiscard]] const variant* find_member(const variant& object, std::string_view name) {
  if (!object.is_object()) {
    return nullptr;
  }

  const value::variant_member* member = object.find_member(core::string_name{name});
  return member != nullptr ? &member->value : nullptr;
}

[[nodiscard]] const core::string* find_string_member(const variant& object, std::string_view name) {
  const variant* member = find_member(object, name);
  return member != nullptr ? member->try_as_string() : nullptr;
}

[[nodiscard]] error read_version(const variant& object, std::uint64_t& version) {
  const variant* member = find_member(object, "version");
  if (member == nullptr || member->is_null()) {
    version = 0;
    return {};
  }

  if (const std::uint64_t* value = member->try_as_u64()) {
    version = *value;
    return {};
  }

  if (const std::int64_t* value = member->try_as_i64(); value != nullptr && *value >= 0) {
    version = static_cast<std::uint64_t>(*value);
    return {};
  }

  return {error_domain::resources, errc::invalid_argument};
}

[[nodiscard]] error load_manifest_entry(manifest& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const core::string* guid_text = find_string_member(value, "guid");
  const core::string* location_text = find_string_member(value, "location");
  if (guid_text == nullptr || location_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto parsed_guid =
      resource_guid::parse(std::string_view{guid_text->data(), guid_text->size()});
  if (!parsed_guid) {
    return {error_domain::resources, errc::invalid_argument};
  }

  core::string_name type{};
  if (const core::string* type_text = find_string_member(value, "type")) {
    type = core::string_name{std::string_view{type_text->data(), type_text->size()}};
  }

  std::string_view hash{};
  if (const core::string* hash_text = find_string_member(value, "hash")) {
    hash = std::string_view{hash_text->data(), hash_text->size()};
  }

  std::uint64_t version = 0;
  const error version_status = read_version(value, version);
  if (version_status) {
    return version_status;
  }

  return target.bind(*parsed_guid, std::string_view{location_text->data(), location_text->size()},
                     type, hash, version);
}

[[nodiscard]] const variant::array_type* manifest_entries(const variant& value) {
  if (const variant::array_type* entries = value.try_as_array()) {
    return entries;
  }

  const variant* resources = find_member(value, "resources");
  return resources != nullptr ? resources->try_as_array() : nullptr;
}

} // namespace

error load_manifest(manifest& target, const variant& value) {
  const variant::array_type* entries = manifest_entries(value);
  if (entries == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  for (const variant& entry : *entries) {
    const error status = load_manifest_entry(target, entry);
    if (status) {
      return status;
    }
  }

  return {};
}

error load_manifest_json(manifest& target, std::string_view text) {
  serialization::variant_result parsed = serialization::parse_json(text);
  if (parsed.failed()) {
    return parsed.status;
  }

  return load_manifest(target, parsed.value);
}

error load_manifest(manifest& target, source& resource_source, io::path_view location,
                    memory_resource* resource) {
  if (location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  io::byte_buffer_result bytes =
      resource_source.read(location, resource != nullptr ? resource : target.resource());
  if (bytes.failed()) {
    return bytes.status;
  }

  const std::string_view text{reinterpret_cast<const char*>(bytes.value.data()),
                              bytes.value.size()};
  return load_manifest_json(target, text);
}

} // namespace caors::resources
