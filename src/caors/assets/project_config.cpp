// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets/project_config.h"

#include "caors/serialization/operations.h"

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace caors::assets {
namespace {

[[nodiscard]] std::string_view string_view_of(const core::string& value) noexcept {
  return std::string_view{value.data(), value.size()};
}

[[nodiscard]] const variant::array_type* find_array_member(const variant& object,
                                                           std::string_view name) {
  const variant* member = object.find_member_variant(core::string_name{name});
  return member != nullptr ? member->try_as_array() : nullptr;
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

[[nodiscard]] error read_schema_version(const variant& object, std::uint32_t& version) {
  const variant* member = object.find_member_variant(core::string_name{"schema_version"});
  if (member == nullptr || member->is_null()) {
    version = 1;
    return {};
  }

  std::uint64_t parsed = 0;
  if (const std::uint64_t* value = member->try_as_u64()) {
    parsed = *value;
  } else if (const std::int64_t* value = member->try_as_i64(); value != nullptr && *value >= 0) {
    parsed = static_cast<std::uint64_t>(*value);
  } else {
    return {error_domain::resources, errc::invalid_argument};
  }

  if (parsed == 0 || parsed > std::numeric_limits<std::uint32_t>::max()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  version = static_cast<std::uint32_t>(parsed);
  return version == 1 ? error{} : error{error_domain::resources, errc::unsupported};
}

[[nodiscard]] error read_guid(const variant& object, std::string_view name, resource_guid& guid) {
  const core::string* guid_text = find_string_member(object, name);
  if (guid_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto parsed = resource_guid::parse(string_view_of(*guid_text));
  if (!parsed) {
    return {error_domain::resources, errc::invalid_argument};
  }

  guid = *parsed;
  return {};
}

[[nodiscard]] core::string_name read_optional_name(const variant& object, std::string_view name) {
  const core::string* text = find_string_member(object, name);
  return text != nullptr ? core::string_name{string_view_of(*text)} : core::string_name{};
}

[[nodiscard]] error read_options(const variant& object, variant& options) {
  const variant* member = object.find_member_variant(core::string_name{"options"});
  if (member == nullptr || member->is_null()) {
    options = variant::null();
    return {};
  }

  if (!member->is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  options = *member;
  return {};
}

[[nodiscard]] error read_source_config(project_config& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const core::string* name_text = find_string_member(value, "name");
  if (name_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  project_source_config source_config{target.resource};
  source_config.name = core::string_name{string_view_of(*name_text)};
  source_config.type = read_optional_name(value, "type");

  if (const core::string* root_text = find_string_member(value, "root")) {
    source_config.root = io::normalize_path(string_view_of(*root_text), target.resource);
  }

  error status = read_options(value, source_config.options);
  if (status) {
    return status;
  }

  target.sources.push_back(std::move(source_config));
  return {};
}

[[nodiscard]] error read_resource_config(project_config& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  project_resource_config resource_config{target.resource};
  error status = read_guid(value, "guid", resource_config.guid);
  if (status) {
    return status;
  }

  const core::string* location_text = find_string_member(value, "location");
  if (location_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  resource_config.source = read_optional_name(value, "source");
  resource_config.location = io::normalize_path(string_view_of(*location_text), target.resource);
  resource_config.type = read_optional_name(value, "type");

  if (const core::string* hash_text = find_string_member(value, "hash")) {
    const std::string_view hash = string_view_of(*hash_text);
    resource_config.hash.assign(hash.begin(), hash.end());
  }

  status = read_version(value, resource_config.version);
  if (status) {
    return status;
  }

  if (!resource_config.guid || resource_config.location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  target.resources.push_back(std::move(resource_config));
  return {};
}

[[nodiscard]] error read_resource_type_config(project_config& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const core::string* type_text = find_string_member(value, "type");
  if (type_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  project_resource_type_config type_config{};
  type_config.type = core::string_name{string_view_of(*type_text)};
  type_config.loader = read_optional_name(value, "loader");
  type_config.cache = read_optional_name(value, "cache");

  error status = read_options(value, type_config.options);
  if (status) {
    return status;
  }

  target.resource_types.push_back(type_config);
  return {};
}

[[nodiscard]] error read_dependency_config(project_config& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  project_dependency_config dependency{};
  error status = read_guid(value, "resource", dependency.resource);
  if (status) {
    return status;
  }

  status = read_guid(value, "dependency", dependency.dependency);
  if (status) {
    return status;
  }

  if (!dependency.resource || !dependency.dependency ||
      dependency.resource == dependency.dependency) {
    return {error_domain::resources, errc::invalid_argument};
  }

  target.dependencies.push_back(dependency);
  return {};
}

[[nodiscard]] error read_preload_root(project_config& target, const variant& value) {
  resource_guid guid{};

  if (const core::string* guid_text = value.try_as_string()) {
    const auto parsed = resource_guid::parse(string_view_of(*guid_text));
    if (!parsed) {
      return {error_domain::resources, errc::invalid_argument};
    }
    guid = *parsed;
  } else if (value.is_object()) {
    const error status = read_guid(value, "guid", guid);
    if (status) {
      return status;
    }
  } else {
    return {error_domain::resources, errc::invalid_argument};
  }

  if (!guid) {
    return {error_domain::resources, errc::invalid_argument};
  }

  target.preload_roots.push_back(guid);
  return {};
}

template <typename Reader>
[[nodiscard]] error read_array(const variant& root, std::string_view name, Reader&& reader) {
  const variant::array_type* entries = find_array_member(root, name);
  if (entries == nullptr) {
    return {};
  }

  for (const variant& entry : *entries) {
    const error status = reader(entry);
    if (status) {
      return status;
    }
  }

  return {};
}

[[nodiscard]] error read_project_config(project_config& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  error status = read_schema_version(value, target.schema_version);
  if (status) {
    return status;
  }

  status = read_array(value, "sources",
                      [&](const variant& entry) { return read_source_config(target, entry); });
  if (status) {
    return status;
  }

  status = read_array(value, "resources",
                      [&](const variant& entry) { return read_resource_config(target, entry); });
  if (status) {
    return status;
  }

  status = read_array(value, "resource_types", [&](const variant& entry) {
    return read_resource_type_config(target, entry);
  });
  if (status) {
    return status;
  }

  status = read_array(value, "dependencies",
                      [&](const variant& entry) { return read_dependency_config(target, entry); });
  if (status) {
    return status;
  }

  return read_array(value, "preload",
                    [&](const variant& entry) { return read_preload_root(target, entry); });
}

} // namespace

error load_project_config(project_config& target, const variant& value) {
  project_config loaded{target.resource};
  const error status = read_project_config(loaded, value);
  if (status) {
    return status;
  }

  target = std::move(loaded);
  return {};
}

error load_project_config_json(project_config& target, std::string_view text) {
  serialization::variant_result parsed = serialization::parse_json(text);
  if (parsed.failed()) {
    return parsed.status;
  }

  return load_project_config(target, parsed.value);
}

error load_project_config(project_config& target, resources::source& resource_source,
                          io::path_view location, memory_resource* resource) {
  if (location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  io::byte_buffer_result bytes =
      resource_source.read(location, resource != nullptr ? resource : target.resource);
  if (bytes.failed()) {
    return bytes.status;
  }

  const std::string_view text{reinterpret_cast<const char*>(bytes.value.data()),
                              bytes.value.size()};
  return load_project_config_json(target, text);
}

} // namespace caors::assets
