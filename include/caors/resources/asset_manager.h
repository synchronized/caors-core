// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_ASSET_MANAGER_H_
#define CAORS_RESOURCES_ASSET_MANAGER_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/resources/cache.h"
#include "caors/resources/database.h"
#include "caors/resources/dependency_graph.h"
#include "caors/resources/io.h"
#include "caors/resources/manifest.h"
#include "caors/resources/project_config.h"
#include "caors/resources/source.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::resources {

/**
 * @brief 拥有式资源运行期外壳。
 *
 * `asset_manager` 持有 manifest、provider、dependency graph 和 database，解决 `database`
 * 非拥有指针的生命周期拼装问题。它可以根据 `project_config` 中的策略名创建内置 source，并可通过
 * 类型工厂把资源类型描述映射到运行期 cache/loader。
 */
class asset_manager final {
public:
  struct source_factory_ops {
    using create_fn = error (*)(asset_manager& manager, const project_source_config& config);

    constexpr source_factory_ops() noexcept = default;
    constexpr source_factory_ops(create_fn value) noexcept : create(value) {}

    create_fn create = nullptr;

    [[nodiscard]] constexpr bool valid() const noexcept { return create != nullptr; }
    [[nodiscard]] error operator()(asset_manager& manager,
                                   const project_source_config& config) const {
      return create(manager, config);
    }
  };

  struct type_factory_ops {
    using create_fn = error (*)(asset_manager& manager, const project_resource_type_config& config);

    constexpr type_factory_ops() noexcept = default;
    constexpr type_factory_ops(create_fn value) noexcept : create(value) {}

    create_fn create = nullptr;

    [[nodiscard]] constexpr bool valid() const noexcept { return create != nullptr; }
    [[nodiscard]] error operator()(asset_manager& manager,
                                   const project_resource_type_config& config) const {
      return create(manager, config);
    }
  };

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

  /** @brief 注册 source 策略名到运行期 source 构造函数。 */
  [[nodiscard]] error register_source_factory(core::string_name type, source_factory_ops factory);

  /** @brief 注册 manifest 资源类型到运行期 cache/loader 构造函数。 */
  [[nodiscard]] error register_type_factory(core::string_name resource_type,
                                            type_factory_ops factory);

  /**
   * @brief 注册默认资源类型工厂。
   *
   * 第一版支持 `cache = "memory"` 和 `loader = "json"`，会创建 `cache<T>` 与
   * `json_loader<T>` 并注册到内部 database。
   */
  template <typename T> [[nodiscard]] error register_type_factory(core::string_name resource_type) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return register_type_factory(resource_type,
                                 type_factory_ops{&asset_manager::default_type_factory<T>});
  }

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

  /** @brief 创建可挂到 serialization::context 的外部引用 resolver。 */
  [[nodiscard]] resources::resource_ref_resolver_ops resolver() noexcept {
    return database_.resolver();
  }

private:
  using source_map = core::hash_map<core::string_name, resources::source*>;
  using source_factory_map = core::hash_map<core::string_name, source_factory_ops>;
  using type_factory_map = core::hash_map<core::string_name, type_factory_ops>;

  struct owned_runtime_object {
    using destroy_fn = void (*)(memory_resource* resource, void* value) noexcept;

    owned_runtime_object() noexcept = default;
    owned_runtime_object(memory_resource* resource, void* value, destroy_fn destroy) noexcept;
    ~owned_runtime_object();

    owned_runtime_object(const owned_runtime_object&) = delete;
    owned_runtime_object& operator=(const owned_runtime_object&) = delete;

    owned_runtime_object(owned_runtime_object&& other) noexcept;
    owned_runtime_object& operator=(owned_runtime_object&& other) noexcept;

    void reset() noexcept;

    memory_resource* resource = nullptr;
    void* value = nullptr;
    destroy_fn destroy = nullptr;
  };

  struct owned_type_binding {
    core::string_name loader{};
    core::string_name cache{};
  };

  using owned_object_list = core::vector<owned_runtime_object>;
  using owned_type_map = core::hash_map<core::string_name, owned_type_binding>;

  [[nodiscard]] resources::source*
  resolve_resource_source(const project_config& config,
                          const project_resource_config& resource_config) const;

  [[nodiscard]] error ensure_source_from_config(const project_source_config& config);
  [[nodiscard]] error ensure_type_from_config(const project_resource_type_config& config);
  [[nodiscard]] error register_builtin_source_factories();
  [[nodiscard]] static error make_memory_file_source(asset_manager& manager,
                                                     const project_source_config& config);
  [[nodiscard]] static error make_native_file_source(asset_manager& manager,
                                                     const project_source_config& config);

  template <typename T, typename... Args> [[nodiscard]] T* make_owned(Args&&... args) {
    T* value = memory::make<T>(resource_, std::forward<Args>(args)...);
    if (value == nullptr) {
      return nullptr;
    }

    owned_objects_.emplace_back(resource_, value, &asset_manager::destroy_owned<T>);
    return value;
  }

  template <typename T> static void destroy_owned(memory_resource* resource, void* value) noexcept {
    memory::destroy(static_cast<T*>(value), resource);
  }

  template <typename T>
  [[nodiscard]] static error default_type_factory(asset_manager& manager,
                                                  const project_resource_type_config& config) {
    if (!config.type || config.cache != core::string_name{"memory"} ||
        config.loader != core::string_name{"json"}) {
      return {error_domain::resources, errc::unsupported};
    }

    const auto existing = manager.owned_types_.find(config.type);
    if (existing != manager.owned_types_.end()) {
      return existing->second.cache == config.cache && existing->second.loader == config.loader
                 ? error{}
                 : error{error_domain::resources, errc::already_exists};
    }

    auto* cache = manager.make_owned<resources::cache<T>>(manager.resource_);
    if (cache == nullptr) {
      return {error_domain::resources, errc::out_of_memory};
    }

    auto* loader = manager.make_owned<resources::json_loader<T>>(manager.provider_);
    if (loader == nullptr) {
      return {error_domain::resources, errc::out_of_memory};
    }

    const error status = manager.database_.register_type(*cache, *loader, config.type);
    if (status) {
      return status;
    }

    manager.owned_types_.emplace(config.type, owned_type_binding{config.loader, config.cache});
    return {};
  }

  memory_resource* resource_ = nullptr;
  resources::manifest manifest_;
  resources::provider provider_;
  resources::dependency_graph dependency_graph_;
  resources::database database_;
  source_map sources_;
  source_factory_map source_factories_;
  type_factory_map type_factories_;
  owned_object_list owned_objects_;
  owned_type_map owned_types_;
};

} // namespace caors::resources

namespace caors {

using resource_asset_manager = resources::asset_manager;

} // namespace caors

#endif
