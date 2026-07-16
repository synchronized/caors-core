// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_PROJECT_CONFIG_H_
#define CAORS_RESOURCES_PROJECT_CONFIG_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/io/path.h"
#include "caors/meta/value/variant.h"
#include "caors/resources/id.h"
#include "caors/resources/source.h"

#include <cstdint>
#include <string_view>

namespace caors::resources {

/** @brief 工程资源来源描述。运行期 source 对象由 asset_manager 工厂或上层代码创建。 */
struct project_source_config {
  explicit project_source_config(memory_resource* resource = nullptr) : root(resource) {}

  core::string_name name{};
  core::string_name type{};
  io::path root;
};

/** @brief 工程资源条目描述。比 manifest 多保存一个 source 名称。 */
struct project_resource_config {
  explicit project_resource_config(memory_resource* resource = nullptr)
      : location(resource), hash(allocator<char>{resource}) {}

  resource_guid guid{};
  core::string_name source{};
  io::path location;
  core::string_name type{};
  core::string hash;
  std::uint64_t version = 0;
};

/** @brief 资源类型到运行期加载/缓存策略名的描述。 */
struct project_resource_type_config {
  core::string_name type{};
  core::string_name loader{};
  core::string_name cache{};
};

/** @brief 资源依赖描述，表示 resource 依赖 dependency。 */
struct project_dependency_config {
  resource_guid resource{};
  resource_guid dependency{};
};

/**
 * @brief 工程资源系统配置描述。
 *
 * `project_config` 只保存可反序列化的数据描述，不拥有运行期 `provider`、`source`、`cache`、
 * `loader` 或 `database`。运行期对象应由 `asset_manager` 或上层 builder 根据这些描述创建和绑定。
 */
struct project_config {
  using source_list = core::vector<project_source_config>;
  using resource_list = core::vector<project_resource_config>;
  using resource_type_list = core::vector<project_resource_type_config>;
  using dependency_list = core::vector<project_dependency_config>;
  using preload_list = core::vector<resource_guid>;

  explicit project_config(memory_resource* memory = nullptr)
      : resource(memory != nullptr ? memory : get_default_memory_resource()),
        sources(typename source_list::allocator_type{resource}),
        resources(typename resource_list::allocator_type{resource}),
        resource_types(typename resource_type_list::allocator_type{resource}),
        dependencies(typename dependency_list::allocator_type{resource}),
        preload_roots(typename preload_list::allocator_type{resource}) {}

  /** @brief 清空全部配置描述。 */
  void clear() noexcept {
    sources.clear();
    resources.clear();
    resource_types.clear();
    dependencies.clear();
    preload_roots.clear();
  }

  /** @brief 配置内部容器和字符串使用的内存资源。 */
  memory_resource* resource = nullptr;
  source_list sources;
  resource_list resources;
  resource_type_list resource_types;
  dependency_list dependencies;
  preload_list preload_roots;
};

/** @brief 从 DOM 配置读入资源工程描述。 */
[[nodiscard]] CAORS_API error load_project_config(project_config& target, const variant& value);

/** @brief 从 JSON 文本读入资源工程描述。 */
[[nodiscard]] CAORS_API error load_project_config_json(project_config& target,
                                                       std::string_view text);

/** @brief 从资源字节来源读取 JSON 文本并读入资源工程描述。 */
[[nodiscard]] CAORS_API error load_project_config(project_config& target, source& resource_source,
                                                  io::path_view location,
                                                  memory_resource* resource = nullptr);

} // namespace caors::resources

#endif
