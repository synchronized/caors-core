// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/asset_manager.h"

#include "caors/io/memory_file_system.h"
#include "caors/io/native_file_system.h"

namespace caors::resources {

error asset_manager::make_memory_file_source(asset_manager& manager,
                                             const project_source_config& config) {
  auto* file_system = manager.make_owned<io::memory_file_system>(manager.resource());
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = manager.make_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return manager.bind_source(config.name, *source);
}

error asset_manager::make_native_file_source(asset_manager& manager,
                                             const project_source_config& config) {
  auto* file_system =
      config.root.empty()
          ? manager.make_owned<io::native_file_system>(manager.resource())
          : manager.make_owned<io::native_file_system>(config.root, manager.resource());
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = manager.make_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return manager.bind_source(config.name, *source);
}

asset_manager::owned_runtime_object::owned_runtime_object(memory_resource* resource, void* value,
                                                          destroy_fn destroy) noexcept
    : resource(resource), value(value), destroy(destroy) {}

asset_manager::owned_runtime_object::~owned_runtime_object() { reset(); }

asset_manager::owned_runtime_object::owned_runtime_object(owned_runtime_object&& other) noexcept
    : resource(other.resource), value(other.value), destroy(other.destroy) {
  other.resource = nullptr;
  other.value = nullptr;
  other.destroy = nullptr;
}

asset_manager::owned_runtime_object&
asset_manager::owned_runtime_object::operator=(owned_runtime_object&& other) noexcept {
  if (this != &other) {
    reset();
    resource = other.resource;
    value = other.value;
    destroy = other.destroy;
    other.resource = nullptr;
    other.value = nullptr;
    other.destroy = nullptr;
  }

  return *this;
}

void asset_manager::owned_runtime_object::reset() noexcept {
  if (value != nullptr && destroy != nullptr) {
    destroy(resource, value);
  }

  resource = nullptr;
  value = nullptr;
  destroy = nullptr;
}

asset_manager::asset_manager(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      manifest_(resource_), provider_(resource_), dependency_graph_(resource_),
      database_(resource_), sources_(typename source_map::allocator_type{resource_}),
      source_factories_(typename source_factory_map::allocator_type{resource_}),
      type_factories_(typename type_factory_map::allocator_type{resource_}),
      owned_objects_(typename owned_object_list::allocator_type{resource_}),
      owned_types_(typename owned_type_map::allocator_type{resource_}) {
  database_.set_manifest(manifest_);
  database_.set_provider(provider_);
  database_.set_dependency_graph(dependency_graph_);
  (void)register_builtin_source_factories();
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

error asset_manager::register_source_factory(core::string_name type, source_factory_fn factory) {
  if (!type || factory == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = source_factories_.find(type);
  if (position != source_factories_.end()) {
    return position->second == factory ? error{}
                                       : error{error_domain::resources, errc::already_exists};
  }

  source_factories_.emplace(type, factory);
  return {};
}

error asset_manager::register_type_factory(core::string_name resource_type,
                                           type_factory_fn factory) {
  if (!resource_type || factory == nullptr) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = type_factories_.find(resource_type);
  if (position != type_factories_.end()) {
    return position->second == factory ? error{}
                                       : error{error_domain::resources, errc::already_exists};
  }

  type_factories_.emplace(resource_type, factory);
  return {};
}

error asset_manager::build(const project_config& config) {
  manifest_.clear();
  provider_.clear();
  dependency_graph_.clear();

  for (const project_source_config& source_config : config.sources) {
    const error status = ensure_source_from_config(source_config);
    if (status) {
      return status;
    }
  }

  for (const project_resource_type_config& type_config : config.resource_types) {
    const error status = ensure_type_from_config(type_config);
    if (status) {
      return status;
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

error asset_manager::ensure_source_from_config(const project_source_config& config) {
  if (!config.name) {
    return {error_domain::resources, errc::invalid_argument};
  }

  if (find_source(config.name) != nullptr) {
    return {};
  }

  const auto position = source_factories_.find(config.type);
  if (position == source_factories_.end()) {
    return {error_domain::resources, errc::not_found};
  }

  return position->second(*this, config);
}

error asset_manager::ensure_type_from_config(const project_resource_type_config& config) {
  if (!config.type) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = type_factories_.find(config.type);
  if (position == type_factories_.end()) {
    return {};
  }

  return position->second(*this, config);
}

error asset_manager::register_builtin_source_factories() {
  error status = register_source_factory(core::string_name{"memory_file_system"},
                                         &asset_manager::make_memory_file_source);
  if (status) {
    return status;
  }

  status =
      register_source_factory(core::string_name{"memory"}, &asset_manager::make_memory_file_source);
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"native_file_system"},
                                   &asset_manager::make_native_file_source);
  if (status) {
    return status;
  }

  return register_source_factory(core::string_name{"native"},
                                 &asset_manager::make_native_file_source);
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
