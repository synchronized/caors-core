// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets/project_config.h"

#include "caors/io/location.h"
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

template <typename Reader>
[[nodiscard]] error read_array(const variant& root, std::string_view name, Reader&& reader);

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
  source_config.file_system = read_optional_name(value, "file_system");
  if (!source_config.type && source_config.file_system) {
    source_config.type = core::string_name{"file_system"};
  }

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

[[nodiscard]] error read_file_system_route_config(project_config& target,
                                                  project_file_system_config& file_system_config,
                                                  const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const core::string* prefix_text = find_string_member(value, "prefix");
  const core::string* target_text = find_string_member(value, "target");
  if (prefix_text == nullptr || target_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  project_file_system_route_config route_config{target.resource};
  route_config.prefix = io::normalize_location(string_view_of(*prefix_text), target.resource);
  route_config.target = core::string_name{string_view_of(*target_text)};

  if (const core::string* target_prefix_text = find_string_member(value, "target_prefix")) {
    route_config.target_prefix =
        io::normalize_path(string_view_of(*target_prefix_text), target.resource);
  }

  if (route_config.prefix.empty() || !route_config.target) {
    return {error_domain::resources, errc::invalid_argument};
  }

  file_system_config.routes.push_back(std::move(route_config));
  return {};
}

[[nodiscard]] error read_file_system_config(project_config& target, const variant& value) {
  if (!value.is_object()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const core::string* name_text = find_string_member(value, "name");
  const core::string* type_text = find_string_member(value, "type");
  if (name_text == nullptr || type_text == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  project_file_system_config file_system_config{target.resource};
  file_system_config.name = core::string_name{string_view_of(*name_text)};
  file_system_config.type = core::string_name{string_view_of(*type_text)};

  if (const core::string* root_text = find_string_member(value, "root")) {
    file_system_config.root = io::normalize_path(string_view_of(*root_text), target.resource);
  }

  error status = read_options(value, file_system_config.options);
  if (status) {
    return status;
  }

  status = read_array(value, "routes", [&](const variant& entry) {
    return read_file_system_route_config(target, file_system_config, entry);
  });
  if (status) {
    return status;
  }

  if (!file_system_config.name || !file_system_config.type) {
    return {error_domain::resources, errc::invalid_argument};
  }

  target.file_systems.push_back(std::move(file_system_config));
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
  resource_config.location =
      io::normalize_location(string_view_of(*location_text), target.resource);
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

  status = read_array(value, "file_systems",
                      [&](const variant& entry) { return read_file_system_config(target, entry); });
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

void add_member(variant::object_type& members, std::string_view name, variant value) {
  members.push_back(meta::variant_member{core::string_name{name}, 0, std::move(value)});
}

void add_name_member(variant::object_type& members, std::string_view name,
                     core::string_name value) {
  if (value) {
    add_member(members, name, variant::from(value.view()));
  }
}

void add_path_member(variant::object_type& members, std::string_view name, io::path_view value,
                     memory_resource* resource) {
  (void)resource;
  if (value.empty()) {
    return;
  }

  add_member(members, name, variant::from(value.string_view()));
}

void add_location_member(variant::object_type& members, std::string_view name,
                         io::location_view value, memory_resource* resource) {
  (void)resource;
  if (value.empty()) {
    return;
  }

  add_member(members, name, variant::from(value.string_view()));
}

void add_options_member(variant::object_type& members, const variant& options) {
  if (!options.is_null()) {
    add_member(members, "options", options);
  }
}

[[nodiscard]] variant make_route_value(const project_file_system_route_config& route,
                                       memory_resource* resource) {
  variant::object_type members{typename variant::object_type::allocator_type{resource}};
  add_location_member(members, "prefix", route.prefix, resource);
  add_name_member(members, "target", route.target);
  add_path_member(members, "target_prefix", route.target_prefix, resource);
  return variant::object(std::move(members));
}

[[nodiscard]] variant make_file_system_value(const project_file_system_config& file_system,
                                             memory_resource* resource) {
  variant::object_type members{typename variant::object_type::allocator_type{resource}};
  add_name_member(members, "name", file_system.name);
  add_name_member(members, "type", file_system.type);
  add_path_member(members, "root", file_system.root, resource);

  if (!file_system.routes.empty()) {
    variant::array_type routes{typename variant::array_type::allocator_type{resource}};
    routes.reserve(file_system.routes.size());
    for (const project_file_system_route_config& route : file_system.routes) {
      routes.push_back(make_route_value(route, resource));
    }
    add_member(members, "routes", variant::array(std::move(routes)));
  }

  add_options_member(members, file_system.options);
  return variant::object(std::move(members));
}

[[nodiscard]] variant make_source_value(const project_source_config& source,
                                        memory_resource* resource) {
  variant::object_type members{typename variant::object_type::allocator_type{resource}};
  add_name_member(members, "name", source.name);
  add_name_member(members, "type", source.type);
  add_name_member(members, "file_system", source.file_system);
  add_path_member(members, "root", source.root, resource);
  add_options_member(members, source.options);
  return variant::object(std::move(members));
}

[[nodiscard]] variant make_resource_type_value(const project_resource_type_config& type,
                                               memory_resource* resource) {
  variant::object_type members{typename variant::object_type::allocator_type{resource}};
  add_name_member(members, "type", type.type);
  add_name_member(members, "loader", type.loader);
  add_name_member(members, "cache", type.cache);
  add_options_member(members, type.options);
  return variant::object(std::move(members));
}

[[nodiscard]] variant make_resource_value(const project_resource_config& resource_config,
                                          memory_resource* resource) {
  variant::object_type members{typename variant::object_type::allocator_type{resource}};
  const std::string guid_text = resource_config.guid.string();
  add_member(members, "guid", variant::from(std::string_view{guid_text.data(), guid_text.size()}));
  add_name_member(members, "source", resource_config.source);
  add_location_member(members, "location", resource_config.location, resource);
  add_name_member(members, "type", resource_config.type);
  if (!resource_config.hash.empty()) {
    add_member(members, "hash", variant::from(string_view_of(resource_config.hash)));
  }
  if (resource_config.version != 0) {
    add_member(members, "version", variant::from(resource_config.version));
  }
  return variant::object(std::move(members));
}

[[nodiscard]] variant make_dependency_value(const project_dependency_config& dependency,
                                            memory_resource* resource) {
  variant::object_type members{typename variant::object_type::allocator_type{resource}};
  const std::string resource_text = dependency.resource.string();
  const std::string dependency_text = dependency.dependency.string();
  add_member(members, "resource",
             variant::from(std::string_view{resource_text.data(), resource_text.size()}));
  add_member(members, "dependency",
             variant::from(std::string_view{dependency_text.data(), dependency_text.size()}));
  return variant::object(std::move(members));
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

variant to_value(const project_config& config, memory_resource* resource) {
  memory_resource* output_resource = resource != nullptr ? resource : config.resource;
  variant::object_type members{typename variant::object_type::allocator_type{output_resource}};
  add_member(members, "schema_version",
             variant::from(static_cast<std::uint64_t>(config.schema_version)));

  if (!config.file_systems.empty()) {
    variant::array_type values{typename variant::array_type::allocator_type{output_resource}};
    values.reserve(config.file_systems.size());
    for (const project_file_system_config& file_system : config.file_systems) {
      values.push_back(make_file_system_value(file_system, output_resource));
    }
    add_member(members, "file_systems", variant::array(std::move(values)));
  }

  if (!config.sources.empty()) {
    variant::array_type values{typename variant::array_type::allocator_type{output_resource}};
    values.reserve(config.sources.size());
    for (const project_source_config& source : config.sources) {
      values.push_back(make_source_value(source, output_resource));
    }
    add_member(members, "sources", variant::array(std::move(values)));
  }

  if (!config.resource_types.empty()) {
    variant::array_type values{typename variant::array_type::allocator_type{output_resource}};
    values.reserve(config.resource_types.size());
    for (const project_resource_type_config& type : config.resource_types) {
      values.push_back(make_resource_type_value(type, output_resource));
    }
    add_member(members, "resource_types", variant::array(std::move(values)));
  }

  if (!config.resources.empty()) {
    variant::array_type values{typename variant::array_type::allocator_type{output_resource}};
    values.reserve(config.resources.size());
    for (const project_resource_config& resource_config : config.resources) {
      values.push_back(make_resource_value(resource_config, output_resource));
    }
    add_member(members, "resources", variant::array(std::move(values)));
  }

  if (!config.dependencies.empty()) {
    variant::array_type values{typename variant::array_type::allocator_type{output_resource}};
    values.reserve(config.dependencies.size());
    for (const project_dependency_config& dependency : config.dependencies) {
      values.push_back(make_dependency_value(dependency, output_resource));
    }
    add_member(members, "dependencies", variant::array(std::move(values)));
  }

  if (!config.preload_roots.empty()) {
    variant::array_type values{typename variant::array_type::allocator_type{output_resource}};
    values.reserve(config.preload_roots.size());
    for (resource_guid guid : config.preload_roots) {
      const std::string guid_text = guid.string();
      values.push_back(variant::from(std::string_view{guid_text.data(), guid_text.size()}));
    }
    add_member(members, "preload", variant::array(std::move(values)));
  }

  return variant::object(std::move(members));
}

error write_project_config(const project_config& config, serialization::writer& writer) {
  return serialization::write_value_to(writer, to_value(config));
}

} // namespace caors::assets
