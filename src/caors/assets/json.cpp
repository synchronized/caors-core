// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets/json.h"

#include <cstdint>
#include <string_view>

namespace caors::assets::json {
namespace {

[[nodiscard]] std::string_view string_view_of(const core::string& value) noexcept {
  return std::string_view{value.data(), value.size()};
}

[[nodiscard]] const core::string* find_string_member(const variant& object, std::string_view name) {
  const variant* member = object.find_member_variant(core::string_name{name});
  return member != nullptr ? member->try_as_string() : nullptr;
}

[[nodiscard]] error read_version(const variant& object, std::uint64_t& version) {
  const variant* member = object.find_member_variant(core::string_name{"version"});
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

[[nodiscard]] error load_manifest_entry(resources::manifest& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const core::string* guid_text = find_string_member(value, "guid");
  const core::string* location_text = find_string_member(value, "location");
  if (guid_text == nullptr || location_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto parsed_guid = resource_guid::parse(string_view_of(*guid_text));
  if (!parsed_guid) {
    return {error_domain::resources, errc::invalid_argument};
  }

  core::string_name type{};
  if (const core::string* type_text = find_string_member(value, "type")) {
    type = core::string_name{string_view_of(*type_text)};
  }

  std::string_view hash{};
  if (const core::string* hash_text = find_string_member(value, "hash")) {
    hash = string_view_of(*hash_text);
  }

  std::uint64_t version = 0;
  const error version_status = read_version(value, version);
  if (version_status) {
    return version_status;
  }

  return target.set(*parsed_guid, string_view_of(*location_text), type, hash, version);
}

[[nodiscard]] const variant::array_type* manifest_entries(const variant& value) {
  if (const variant::array_type* entries = value.try_as_array()) {
    return entries;
  }

  const variant* resources = value.find_member_variant(core::string_name{"resources"});
  return resources != nullptr ? resources->try_as_array() : nullptr;
}

} // namespace

error project_config_loader::load(project_config& target, resources::source& source,
                                  io::location_view location, memory_resource* resource) {
  return caors::assets::json::load_project_config(target, source, location, resource);
}

error load_manifest(resources::manifest& target, const variant& value) {
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

error load_manifest(resources::manifest& target, std::string_view text) {
  serialization::variant_result parsed = serialization::parse_json(text);
  if (parsed.failed()) {
    return parsed.status;
  }

  return load_manifest(target, parsed.value);
}

error load_manifest(resources::manifest& target, resources::source& resource_source,
                    io::location_view location, memory_resource* resource) {
  if (location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  io::input_stream_result stream =
      resource_source.open_read(location, resource != nullptr ? resource : target.resource());
  if (stream.failed()) {
    return stream.status;
  }

  serialization::variant_result parsed =
      serialization::parse_json(*stream.value, resource != nullptr ? resource : target.resource());
  if (parsed.failed()) {
    return parsed.status;
  }

  return load_manifest(target, parsed.value);
}

error load_project_config(project_config& target, std::string_view text) {
  serialization::variant_result parsed = serialization::parse_json(text);
  if (parsed.failed()) {
    return parsed.status;
  }

  return assets::load_project_config(target, parsed.value);
}

error load_project_config(project_config& target, resources::source& resource_source,
                          io::location_view location, memory_resource* resource) {
  if (location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  io::input_stream_result stream =
      resource_source.open_read(location, resource != nullptr ? resource : target.resource);
  if (stream.failed()) {
    return stream.status;
  }

  serialization::variant_result parsed =
      serialization::parse_json(*stream.value, resource != nullptr ? resource : target.resource);
  if (parsed.failed()) {
    return parsed.status;
  }

  return assets::load_project_config(target, parsed.value);
}

} // namespace caors::assets::json
