// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/asset_manager.h"

#include "caors/io/memory_file_system.h"
#include "caors/io/native_file_system.h"

#include <algorithm>

namespace caors::resources {

error asset_manager::make_memory_file_source(source_factory_context& context,
                                             const project_source_config& config, void* user_data) {
  (void)user_data;
  auto* file_system = context.manager.create_owned<io::memory_file_system>(context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = context.manager.create_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

error asset_manager::make_native_file_source(source_factory_context& context,
                                             const project_source_config& config, void* user_data) {
  (void)user_data;
  auto* file_system =
      config.root.empty()
          ? context.manager.create_owned<io::native_file_system>(context.resource)
          : context.manager.create_owned<io::native_file_system>(config.root, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = context.manager.create_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

asset_manager::owned_runtime_object::owned_runtime_object(memory_resource* resource, void* value,
                                                          destroy_fn destroy,
                                                          bool config_created) noexcept
    : resource(resource), value(value), destroy(destroy), config_created(config_created) {}

asset_manager::owned_runtime_object::~owned_runtime_object() { reset(); }

asset_manager::owned_runtime_object::owned_runtime_object(owned_runtime_object&& other) noexcept
    : resource(other.resource), value(other.value), destroy(other.destroy),
      config_created(other.config_created) {
  other.resource = nullptr;
  other.value = nullptr;
  other.destroy = nullptr;
  other.config_created = false;
}

asset_manager::owned_runtime_object&
asset_manager::owned_runtime_object::operator=(owned_runtime_object&& other) noexcept {
  if (this != &other) {
    reset();
    resource = other.resource;
    value = other.value;
    destroy = other.destroy;
    config_created = other.config_created;
    other.resource = nullptr;
    other.value = nullptr;
    other.destroy = nullptr;
    other.config_created = false;
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
  config_created = false;
}

asset_manager::asset_manager(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      manifest_(resource_), provider_(resource_), dependency_graph_(resource_),
      database_(resource_), sources_(typename source_map::allocator_type{resource_}),
      source_factories_(typename source_factory_map::allocator_type{resource_}),
      type_factories_(typename type_factory_map::allocator_type{resource_}),
      owned_objects_(typename owned_object_list::allocator_type{resource_}),
      owned_types_(typename owned_type_map::allocator_type{resource_}),
      manual_type_bindings_(typename manual_type_map::allocator_type{resource_}) {
  database_.set_manifest(manifest_);
  database_.set_provider(provider_);
  database_.set_dependency_graph(dependency_graph_);
  (void)register_builtin_source_factories();
}

asset_manager::~asset_manager() {
  database_.clear_registered_types();
  provider_.clear();
  sources_.clear();
  type_factories_.clear();
  source_factories_.clear();
  owned_types_.clear();
  manual_type_bindings_.clear();
  owned_objects_.clear();
}

error asset_manager::bind_source(core::string_name name, resources::source& source) {
  if (!name) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = sources_.find(name);
  if (position != sources_.end()) {
    if (position->second.source != &source) {
      return {error_domain::resources, errc::already_exists};
    }

    return {};
  }

  sources_.emplace(name, source_binding{&source, building_config_runtime_});
  return {};
}

bool asset_manager::unbind_source(core::string_name name) { return sources_.erase(name) != 0; }

resources::source* asset_manager::find_source(core::string_name name) const {
  const auto position = sources_.find(name);
  return position != sources_.end() ? position->second.source : nullptr;
}

error asset_manager::register_source_factory(core::string_name type, source_factory_ops factory) {
  if (!type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = source_factories_.find(type);
  if (position != source_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  source_factories_.emplace(type, factory);
  return {};
}

error asset_manager::register_type_factory(core::string_name resource_type,
                                           type_factory_ops factory) {
  if (!resource_type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = type_factories_.find(resource_type);
  if (position != type_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  type_factories_.emplace(resource_type, factory);
  return {};
}

error asset_manager::build(const project_config& config) {
  error status = clear_config_runtime();
  if (status) {
    return status;
  }

  manifest_.clear();
  provider_.clear();
  dependency_graph_.clear();

  for (const project_source_config& source_config : config.sources) {
    status = ensure_source_from_config(source_config);
    if (status) {
      return status;
    }
  }

  for (const project_resource_type_config& type_config : config.resource_types) {
    status = ensure_type_from_config(type_config);
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

    status =
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

  const bool previous = building_config_runtime_;
  building_config_runtime_ = true;
  const error status = position->second(*this, config);
  building_config_runtime_ = previous;
  return status;
}

error asset_manager::ensure_type_from_config(const project_resource_type_config& config) {
  if (!config.type) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = type_factories_.find(config.type);
  if (position == type_factories_.end()) {
    return {};
  }

  const bool previous = building_config_runtime_;
  building_config_runtime_ = true;
  const error status = position->second(*this, config);
  building_config_runtime_ = previous;
  return status;
}

error asset_manager::register_builtin_source_factories() {
  error status =
      register_source_factory(core::string_name{"memory_file_system"},
                              source_factory_ops{&asset_manager::make_memory_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"memory"},
                                   source_factory_ops{&asset_manager::make_memory_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"native_file_system"},
                                   source_factory_ops{&asset_manager::make_native_file_source});
  if (status) {
    return status;
  }

  return register_source_factory(core::string_name{"native"},
                                 source_factory_ops{&asset_manager::make_native_file_source});
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
    return sources_.begin()->second.source;
  }

  return nullptr;
}

error asset_manager::clear_config_runtime() {
  for (auto position = sources_.begin(); position != sources_.end();) {
    if (position->second.config_created) {
      position = sources_.erase(position);
    } else {
      ++position;
    }
  }

  database_.clear_registered_types();
  owned_types_.clear();

  const auto remove_config_object = [](const owned_runtime_object& object) {
    return object.config_created;
  };
  owned_objects_.erase(
      std::remove_if(owned_objects_.begin(), owned_objects_.end(), remove_config_object),
      owned_objects_.end());

  return rebuild_manual_type_bindings();
}

error asset_manager::rebuild_manual_type_bindings() {
  for (const auto& pair : manual_type_bindings_) {
    const manual_type_binding& binding = pair.second;
    if (binding.bind == nullptr || binding.cache == nullptr || binding.loader == nullptr) {
      return {error_domain::resources, errc::invalid_argument};
    }

    const error status = binding.bind(*this, binding.cache, binding.loader, pair.first);
    if (status) {
      return status;
    }
  }

  return {};
}

} // namespace caors::resources
