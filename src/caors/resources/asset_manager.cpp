// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/asset_manager.h"

namespace caors::resources {

asset_manager::asset_manager(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      manifest_(resource_), provider_(resource_), dependency_graph_(resource_),
      database_(resource_), sources_(typename source_map::allocator_type{resource_}) {
  database_.set_manifest(manifest_);
  database_.set_provider(provider_);
  database_.set_dependency_graph(dependency_graph_);
}

error asset_manager::bind_source(core::string_name name, resources::source& source) {
  if (!name) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = sources_.find(name);
  if (position != sources_.end()) {
    if (position->second != &source) {
      return {error_domain::resources, errc::already_exists};
    }

    return {};
  }

  sources_.emplace(name, &source);
  return {};
}

bool asset_manager::unbind_source(core::string_name name) { return sources_.erase(name) != 0; }

resources::source* asset_manager::find_source(core::string_name name) const {
  const auto position = sources_.find(name);
  return position != sources_.end() ? position->second : nullptr;
}

error asset_manager::build(const project_config& config) {
  manifest_.clear();
  provider_.clear();
  dependency_graph_.clear();

  for (const project_source_config& source_config : config.sources) {
    if (!source_config.name) {
      return {error_domain::resources, errc::invalid_argument};
    }
    if (find_source(source_config.name) == nullptr) {
      return {error_domain::resources, errc::not_found};
    }
  }

  for (const project_resource_config& resource_config : config.resources) {
    if (!resource_config.guid || resource_config.location.empty()) {
      return {error_domain::resources, errc::invalid_argument};
    }

    resources::source* source = resolve_resource_source(config, resource_config);
    if (source == nullptr) {
      return {error_domain::resources, errc::not_found};
    }

    error status =
        manifest_.set(resource_config.guid, resource_config.location, resource_config.type,
                      std::string_view{resource_config.hash.data(), resource_config.hash.size()},
                      resource_config.version);
    if (status) {
      return status;
    }

    status = provider_.rebind(resource_config.guid, *source, resource_config.location);
    if (status) {
      return status;
    }
  }

  for (const project_dependency_config& dependency : config.dependencies) {
    if (!dependency_graph_.add(dependency.resource, dependency.dependency)) {
      return {error_domain::resources, errc::invalid_argument};
    }
  }

  return {};
}

resources::source*
asset_manager::resolve_resource_source(const project_config& config,
                                       const project_resource_config& resource_config) const {
  if (resource_config.source) {
    return find_source(resource_config.source);
  }

  if (config.sources.size() == 1) {
    return find_source(config.sources.front().name);
  }

  if (sources_.size() == 1) {
    return sources_.begin()->second;
  }

  return nullptr;
}

} // namespace caors::resources
