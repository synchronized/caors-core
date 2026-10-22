// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets/project_builder.h"

#include <algorithm>
#include <vector>

namespace caors::assets {
namespace {
void set_build_failure(asset_build_result& result, asset_build_stage stage, std::size_t index,
                       error status, core::string_name name, core::string_name type,
                       resource_guid guid, std::string_view reason) {
  result.status = status;
  result.stage = stage;
  result.index = index;
  result.name = name;
  result.type = type;
  result.guid = guid;
  if (!reason.empty()) {
    result.add_reason(reason);
  }
}

[[nodiscard]] error validate_route_graph(const project_config& config) {
  std::vector<core::string_name> active;
  std::vector<core::string_name> done;
  auto visit = [&](auto&& self, core::string_name name) -> error {
    if (std::find(active.begin(), active.end(), name) != active.end())
      return {error_domain::resources, errc::cycle_detected};
    if (std::find(done.begin(), done.end(), name) != done.end())
      return {};
    if (active.size() >= 256)
      return {error_domain::resources, errc::recursion_limit};
    active.push_back(name);
    for (const auto& entry : config.file_systems) {
      if (entry.name != name)
        continue;
      for (const auto& route : entry.routes) {
        if (auto status = self(self, route.target))
          return status;
      }
    }
    active.pop_back();
    done.push_back(name);
    return {};
  };
  for (const auto& entry : config.file_systems) {
    if (auto status = visit(visit, entry.name))
      return status;
  }
  return {};
}

} // namespace

project_builder asset_manager::source_factory_context::project() const {
  return project_builder{manager};
}
project_builder asset_manager::type_factory_context::project() const {
  return project_builder{manager};
}
project_builder asset_manager::file_system_factory_context::project() const {
  return project_builder{manager};
}

error project_builder::build(const project_config& config) { return build_detailed(config).status; }

asset_build_result project_builder::build_detailed(const project_config& config) {
  asset_build_result result{manager_.resource_};
  error status = manager_.clear_config_runtime();
  if (status) {
    set_build_failure(result, asset_build_stage::clear_runtime, asset_build_result::npos, status,
                      {}, {}, {}, "清理上一轮配置运行期对象失败");
    return result;
  }

  manager_.manifest_.clear();
  manager_.project_provider().clear();
  manager_.dependency_graph_.clear();

  for (std::size_t index = 0; index < config.file_systems.size(); ++index) {
    const project_file_system_config& file_system_config = config.file_systems[index];
    status = manager_.ensure_file_system_from_config(file_system_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::file_system, index, status,
                        file_system_config.name, file_system_config.type, {}, {});
      return result;
    }
  }

  status = validate_route_graph(config);
  if (status) {
    set_build_failure(result, asset_build_stage::file_system, asset_build_result::npos, status, {},
                      {}, {}, "file_system 路由存在循环或超出递归深度");
    return result;
  }

  for (std::size_t index = 0; index < config.file_systems.size(); ++index) {
    const project_file_system_config& file_system_config = config.file_systems[index];
    status = manager_.configure_file_system_from_config(file_system_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::file_system, index, status,
                        file_system_config.name, file_system_config.type, {}, {});
      return result;
    }
  }

  for (std::size_t index = 0; index < config.sources.size(); ++index) {
    const project_source_config& source_config = config.sources[index];
    status = manager_.ensure_source_from_config(source_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::source, index, status, source_config.name,
                        source_config.type, {}, {});
      return result;
    }
  }

  for (std::size_t index = 0; index < config.resource_types.size(); ++index) {
    const project_resource_type_config& type_config = config.resource_types[index];
    status = manager_.ensure_type_from_config(type_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::resource_type, index, status, type_config.type,
                        type_config.type, {}, {});
      return result;
    }
  }

  for (std::size_t index = 0; index < config.resources.size(); ++index) {
    const project_resource_config& resource_config = config.resources[index];
    if (!resource_config.guid || resource_config.location.empty()) {
      set_build_failure(result, asset_build_stage::resource, index,
                        {error_domain::resources, errc::invalid_argument}, resource_config.source,
                        resource_config.type, resource_config.guid,
                        "resource 配置缺少 guid 或 location");
      return result;
    }

    resources::source* source = manager_.resolve_resource_source(config, resource_config);
    if (source == nullptr) {
      set_build_failure(result, asset_build_stage::resource, index,
                        {error_domain::resources, errc::not_found}, resource_config.source,
                        resource_config.type, resource_config.guid, "resource 配置无法解析 source");
      return result;
    }

    status = manager_.manifest_.set(
        resource_config.guid, resource_config.location, resource_config.type,
        std::string_view{resource_config.hash.data(), resource_config.hash.size()},
        resource_config.version);
    if (status) {
      set_build_failure(result, asset_build_stage::resource, index, status, resource_config.source,
                        resource_config.type, resource_config.guid, "写入 manifest 失败");
      return result;
    }

    status =
        manager_.project_provider().rebind(resource_config.guid, *source, resource_config.location);
    if (status) {
      set_build_failure(result, asset_build_stage::resource, index, status, resource_config.source,
                        resource_config.type, resource_config.guid, "绑定 provider source 失败");
      return result;
    }
  }

  for (std::size_t index = 0; index < config.dependencies.size(); ++index) {
    const project_dependency_config& dependency = config.dependencies[index];
    if (!manager_.dependency_graph_.add(dependency.resource, dependency.dependency)) {
      set_build_failure(result, asset_build_stage::dependency, index,
                        {error_domain::resources, errc::invalid_argument}, {}, {},
                        dependency.resource, "dependency 配置无效或存在自依赖");
      return result;
    }
  }

  return result;
}

} // namespace caors::assets
