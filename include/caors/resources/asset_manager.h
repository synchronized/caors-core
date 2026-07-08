// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_ASSET_MANAGER_H_
#define CAORS_RESOURCES_ASSET_MANAGER_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/resources/cache.h"
#include "caors/resources/database.h"
#include "caors/resources/dependency_graph.h"
#include "caors/resources/io.h"
#include "caors/resources/manifest.h"
#include "caors/resources/project_config.h"
#include "caors/resources/source.h"

#include <cstddef>
#include <type_traits>

namespace caors::resources {

/**
 * @brief 拥有式资源运行期外壳。
 *
 * `asset_manager` 持有 manifest、provider、dependency graph 和 database，解决 `database`
 * 非拥有指针的生命周期拼装问题。第一版不自动创建任意 C++ 资源类型的 cache/loader；调用方仍需显式
 * 注册类型化 cache 和 loader，后续再接入工厂注册。
 */
class asset_manager final {
public:
  explicit asset_manager(memory_resource* resource = nullptr);

  asset_manager(const asset_manager&) = delete;
  asset_manager& operator=(const asset_manager&) = delete;
  asset_manager(asset_manager&&) = delete;
  asset_manager& operator=(asset_manager&&) = delete;

  /** @brief 返回内部容器使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  [[nodiscard]] resources::manifest& manifest() noexcept { return manifest_; }
  [[nodiscard]] const resources::manifest& manifest() const noexcept { return manifest_; }

  [[nodiscard]] resources::provider& provider() noexcept { return provider_; }
  [[nodiscard]] const resources::provider& provider() const noexcept { return provider_; }

  [[nodiscard]] resources::dependency_graph& dependency_graph() noexcept {
    return dependency_graph_;
  }
  [[nodiscard]] const resources::dependency_graph& dependency_graph() const noexcept {
    return dependency_graph_;
  }

  [[nodiscard]] resources::database& database() noexcept { return database_; }
  [[nodiscard]] const resources::database& database() const noexcept { return database_; }

  /** @brief 按配置名称绑定运行期 source。asset_manager 不拥有 source。 */
  [[nodiscard]] error bind_source(core::string_name name, resources::source& source);

  /** @brief 解除 source 名称绑定。 */
  [[nodiscard]] bool unbind_source(core::string_name name);

  /** @brief 查找已绑定 source。 */
  [[nodiscard]] resources::source* find_source(core::string_name name) const;

  /** @brief 清空 source 名称绑定。 */
  void clear_sources() noexcept { sources_.clear(); }

  /** @brief 查询 source 绑定数量。 */
  [[nodiscard]] std::size_t source_count() const noexcept { return sources_.size(); }

  /**
   * @brief 根据工程配置重建 manifest、provider 和 dependency graph。
   *
   * 该函数会清空已有 manifest/provider/dependency graph，再按配置重新绑定。cache/loader
   * 注册不会被清除；它们仍保留在内部 database 中。
   */
  [[nodiscard]] error build(const project_config& config);

  /** @brief 同时注册某个资源类型的 cache、loader 和 manifest type。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& cache, resources::loader<T>& loader,
                                    core::string_name resource_type) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.register_type(cache, loader, resource_type);
  }

  /** @brief 生成跨类型预加载计划。 */
  [[nodiscard]] preload_plan plan_preload(resource_guid root) const {
    return database_.plan_preload(root);
  }

  /** @brief 按 manifest type 同步预加载资源。 */
  [[nodiscard]] preload_result preload(resource_guid root) { return database_.preload(root); }

  /** @brief 创建可挂到 serialization::context 的资源 resolver。 */
  [[nodiscard]] serialization::resource_resolver resolver() noexcept {
    return database_.resolver();
  }

private:
  using source_map = core::hash_map<core::string_name, resources::source*>;

  [[nodiscard]] resources::source*
  resolve_resource_source(const project_config& config,
                          const project_resource_config& resource_config) const;

  memory_resource* resource_ = nullptr;
  resources::manifest manifest_;
  resources::provider provider_;
  resources::dependency_graph dependency_graph_;
  resources::database database_;
  source_map sources_;
};

} // namespace caors::resources

namespace caors {

using resource_asset_manager = resources::asset_manager;

} // namespace caors

#endif
