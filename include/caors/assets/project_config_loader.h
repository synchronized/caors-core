// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_PROJECT_CONFIG_LOADER_H_
#define CAORS_ASSETS_PROJECT_CONFIG_LOADER_H_

#include "caors/assets/project_config.h"
#include "caors/resources/source.h"

namespace caors::assets {

/**
 * @brief 工程资源配置读取器接口。
 *
 * `project_config_loader` 只负责把某种外部格式读入 `project_config`，不拥有 source，也不构建
 * 运行期资源环境。JSON、TOML、YAML、二进制配置或编辑器数据库都可以各自实现该接口。
 */
class CAORS_API project_config_loader {
public:
  project_config_loader() = default;
  virtual ~project_config_loader() = default;

  project_config_loader(const project_config_loader&) = delete;
  project_config_loader& operator=(const project_config_loader&) = delete;
  project_config_loader(project_config_loader&&) = delete;
  project_config_loader& operator=(project_config_loader&&) = delete;

  /** @brief 从指定 source/location 读入工程资源配置。 */
  [[nodiscard]] virtual error load(project_config& target, resources::source& source,
                                   io::location_view location,
                                   memory_resource* scratch_resource = nullptr) = 0;
};

/** @brief 使用格式无关配置读取器读入工程资源配置。 */
[[nodiscard]] inline error load_project_config(project_config& target,
                                               project_config_loader& loader,
                                               resources::source& source,
                                               io::location_view location,
                                               memory_resource* scratch_resource = nullptr) {
  return loader.load(target, source, location, scratch_resource);
}

} // namespace caors::assets

#endif
