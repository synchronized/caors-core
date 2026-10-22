// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_ASSET_MANAGER_H_
#define CAORS_ASSETS_ASSET_MANAGER_H_

#include "caors/assets/config_schema.h"
#include "caors/assets/project_config.h"
#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/resources/cache.h"
#include "caors/resources/database.h"
#include "caors/resources/dependency_graph.h"
#include "caors/resources/dependency_list.h"
#include "caors/resources/io.h"
#include "caors/resources/manifest.h"
#include "caors/resources/source.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::assets {

class asset_group;
class project_builder;

/** @brief 运行期加载调度组合；只保存句柄，不拥有线程或队列，不参与 project_config 序列化。 */
struct asset_execution_options {
  tasks::executor worker;
  tasks::executor completion;
};

using resources::dependency_graph;
using resources::handle_result;
using resources::load_request;
using resources::preload_plan;
using resources::preload_request;
using resources::preload_result;

/** @brief asset_manager 构建失败所在阶段。 */
enum class asset_build_stage {
  none,
  clear_runtime,
  file_system,
  source,
  resource_type,
  resource,
  dependency,
};

/** @brief asset_manager 构建结果和诊断信息。 */
struct asset_build_result {
  using reason_list = core::vector<core::string>;
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

  explicit asset_build_result(memory_resource* resource = nullptr)
      : reasons(typename reason_list::allocator_type{resource}) {}

  [[nodiscard]] bool succeeded() const noexcept { return !status; }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }

  /** @brief 追加一条可读诊断原因。 */
  void add_reason(std::string_view reason) {
    core::string text{allocator<char>{reasons.get_allocator().resource()}};
    text.assign(reason.begin(), reason.end());
    reasons.push_back(std::move(text));
  }

  error status{};
  asset_build_stage stage = asset_build_stage::none;
  std::size_t index = npos;
  core::string_name name{};
  core::string_name type{};
  resource_guid guid{};
  reason_list reasons;
};

/**
 * @brief 拥有式资源运行期外壳。
 *
 * `asset_manager` 持有 manifest、provider、dependency graph 和 database，解决 `database`
 * 非拥有指针的生命周期拼装问题。工程配置装配由 project_builder 执行；
 * 当前仍持有其创建的对象以及来源/类型注册状态，后续按迁移计划继续收敛。
 */
class CAORS_API asset_manager final {
private:
  /** @brief source 工厂调用时收到的运行期上下文。 */
  struct source_factory_context {
    /** @brief 正在构建资源环境的 asset_manager。 */
    asset_manager& manager;
    /** @brief asset_manager 内部对象默认使用的内存资源。 */
    memory_resource* resource = nullptr;
    /** @brief 可选构建诊断列表；仅在 build_detailed() 路径中有效。 */
    asset_build_result::reason_list* reasons = nullptr;
    /** @brief 取得本次装配的非拥有操作入口；不得超过 manager 寿命。 */
    [[nodiscard]] CAORS_API project_builder project() const;
  };

  /** @brief 资源类型工厂调用时收到的运行期上下文。 */
  struct type_factory_context {
    /** @brief 正在构建资源环境的 asset_manager。 */
    asset_manager& manager;
    /** @brief asset_manager 内部对象默认使用的内存资源。 */
    memory_resource* resource = nullptr;
    /** @brief asset_manager 内部 provider，loader 通常需要保存它的引用或指针。 */
    resources::provider& provider;
    /** @brief asset_manager 内部 database，type factory 应把创建出的 cache/loader 注册到这里。 */
    resources::database& database;
    /** @brief 可选构建诊断列表；仅在 build_detailed() 路径中有效。 */
    asset_build_result::reason_list* reasons = nullptr;
    /** @brief 取得本次装配的非拥有操作入口；不得超过 manager 寿命。 */
    [[nodiscard]] CAORS_API project_builder project() const;
  };

  /** @brief file_system 工厂调用时收到的运行期上下文。 */
  struct file_system_factory_context {
    /** @brief 正在构建资源环境的 asset_manager。 */
    asset_manager& manager;
    /** @brief asset_manager 内部对象默认使用的内存资源。 */
    memory_resource* resource = nullptr;
    /** @brief 可选构建诊断列表；仅在 build_detailed() 路径中有效。 */
    asset_build_result::reason_list* reasons = nullptr;
    /** @brief 取得本次装配的非拥有操作入口；不得超过 manager 寿命。 */
    [[nodiscard]] CAORS_API project_builder project() const;
  };

  /** @brief 带强类型 options 的 file_system factory 函数签名。 */
  template <typename Options>
  using typed_file_system_factory_fn = error (*)(file_system_factory_context& context,
                                                 const project_file_system_config& config,
                                                 const Options& options, void* user_data);

  /** @brief 带强类型 options 的 source factory 函数签名。 */
  template <typename Options>
  using typed_source_factory_fn = error (*)(source_factory_context& context,
                                            const project_source_config& config,
                                            const Options& options, void* user_data);

  /** @brief 带强类型 options 的 resource type factory 函数签名。 */
  template <typename Options>
  using typed_type_factory_fn = error (*)(type_factory_context& context,
                                          const project_resource_type_config& config,
                                          const Options& options, void* user_data);

  /** @brief file_system 策略名到运行期 file_system 构造逻辑的函数表。 */
  struct file_system_factory {
    using create_fn = error (*)(file_system_factory_context& context,
                                const project_file_system_config& config, void* user_data);

    file_system_factory() = default;
    file_system_factory(create_fn value, void* data = nullptr) : create(value), user_data(data) {}

    create_fn create = nullptr;
    /** @brief 传给 create 的外部状态；asset_manager 不拥有它。 */
    void* user_data = nullptr;

    [[nodiscard]] constexpr bool valid() const noexcept { return create != nullptr; }
    [[nodiscard]] error operator()(asset_manager& manager, const project_file_system_config& config,
                                   asset_build_result::reason_list* reasons = nullptr) const {
      file_system_factory_context context{manager, manager.resource(), reasons};
      return create(context, config, user_data);
    }
  };

  /** @brief source 策略名到运行期 source 构造逻辑的函数表。 */
  struct source_factory {
    using create_fn = error (*)(source_factory_context& context,
                                const project_source_config& config, void* user_data);

    source_factory() = default;
    source_factory(create_fn value, void* data = nullptr) : create(value), user_data(data) {}

    create_fn create = nullptr;
    /** @brief 传给 create 的外部状态；asset_manager 不拥有它。 */
    void* user_data = nullptr;

    [[nodiscard]] constexpr bool valid() const noexcept { return create != nullptr; }
    [[nodiscard]] error operator()(asset_manager& manager, const project_source_config& config,
                                   asset_build_result::reason_list* reasons = nullptr) const {
      source_factory_context context{manager, manager.resource(), reasons};
      return create(context, config, user_data);
    }
  };

  /** @brief manifest 资源类型到 cache/loader 构造逻辑的函数表。 */
  struct type_factory {
    using create_fn = error (*)(type_factory_context& context,
                                const project_resource_type_config& config, void* user_data);

    type_factory() = default;
    type_factory(create_fn value, void* data = nullptr) : create(value), user_data(data) {}

    create_fn create = nullptr;
    /** @brief 传给 create 的外部状态；asset_manager 不拥有它。 */
    void* user_data = nullptr;
    /** @brief 非空时要求配置中的 cache 策略名与该值一致。 */
    core::string_name cache_strategy{};
    /** @brief 非空时要求配置中的 loader 策略名与该值一致。 */
    core::string_name loader_strategy{};

    [[nodiscard]] constexpr bool valid() const noexcept { return create != nullptr; }
    [[nodiscard]] error operator()(asset_manager& manager,
                                   const project_resource_type_config& config,
                                   asset_build_result::reason_list* reasons = nullptr) const {
      if ((cache_strategy && config.cache != cache_strategy) ||
          (loader_strategy && config.loader != loader_strategy)) {
        return {error_domain::resources, errc::unsupported};
      }

      type_factory_context context{manager, manager.resource(), manager.project_provider(),
                                   manager.database(), reasons};
      return create(context, config, user_data);
    }
  };

public:
  explicit asset_manager(memory_resource* resource = nullptr);
  /** @brief 初始化默认调度；未配置有效组合时便捷异步入口返回 invalid_argument。 */
  explicit asset_manager(asset_execution_options execution, memory_resource* resource = nullptr)
      : asset_manager(resource) {
    execution_configured_ = execution.worker.valid() && execution.completion.valid();
    execution_ = std::move(execution);
  }
  /** @brief 在所属线程配置默认调度；实际操作未清理时返回 frozen，不迁移 cache 所属线程。 */
  [[nodiscard]] error set_execution_options(asset_execution_options execution) {
    if (database_.has_active_operations()) {
      return {error_domain::resources, errc::frozen};
    }
    if (!execution.worker.valid() || !execution.completion.valid()) {
      return {error_domain::resources, errc::invalid_argument};
    }
    execution_ = std::move(execution);
    execution_configured_ = true;
    return {};
  }
  /** @brief 创建绑定本 manager 与固定调度组合的非拥有加载上下文；manager 必须覆盖其访问寿命。 */
  [[nodiscard]] asset_group create_group(tasks::executor worker, tasks::executor completion);

  /** @brief 按安全顺序释放内部运行期绑定和由 asset_manager 创建的对象。 */
  ~asset_manager();

  asset_manager(const asset_manager&) = delete;
  asset_manager& operator=(const asset_manager&) = delete;
  asset_manager(asset_manager&&) = delete;
  asset_manager& operator=(asset_manager&&) = delete;

  /** @brief 返回内部容器使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  [[nodiscard]] resources::manifest& manifest() noexcept { return manifest_; }
  [[nodiscard]] const resources::manifest& manifest() const noexcept { return manifest_; }

  [[nodiscard]] resources::dependency_graph& dependency_graph() noexcept {
    return dependency_graph_;
  }
  [[nodiscard]] const resources::dependency_graph& dependency_graph() const noexcept {
    return dependency_graph_;
  }

  [[nodiscard]] resources::database& database() noexcept { return database_; }
  [[nodiscard]] const resources::database& database() const noexcept { return database_; }

private:
  /** @brief 按配置名称绑定运行期 file_system。asset_manager 不拥有 file_system。 */
  [[nodiscard]] error bind_file_system(core::string_name name, io::file_system& file_system);

  /** @brief 拥有式绑定后端及可选模块/宿主凭据；allocator 仍由宿主保证寿命。 */
  [[nodiscard]] error bind_file_system(core::string_name name,
                                       std::shared_ptr<io::file_system> file_system,
                                       std::shared_ptr<void> lifetime = {});
  /** @brief 查询拥有式后端；借用绑定或缺失返回空指针。 */
  [[nodiscard]] std::shared_ptr<io::file_system>
  find_owned_file_system(core::string_name name) const;

  /** @brief 解除 file_system 名称绑定。 */
  [[nodiscard]] bool unbind_file_system(core::string_name name);

  /** @brief 查找已绑定 file_system。 */
  [[nodiscard]] io::file_system* find_file_system(core::string_name name) const;

  /** @brief 清空 file_system 名称绑定。 */
  void clear_file_systems() noexcept;

  /** @brief 查询 file_system 绑定数量。 */
  [[nodiscard]] std::size_t file_system_count() const noexcept;

  /** @brief 注册 file_system 策略名到运行期 file_system 构造函数。 */
  [[nodiscard]] error register_file_system_factory(core::string_name type,
                                                   file_system_factory factory);
  /** @brief 注册带强类型 options 的 file_system 策略构造函数。 */
  template <typename Options, typed_file_system_factory_fn<Options> Create>
  [[nodiscard]] error register_typed_file_system_factory(core::string_name type,
                                                         void* user_data = nullptr) {
    return register_file_system_factory(
        type, file_system_factory{&asset_manager::make_typed_file_system_factory<Options, Create>,
                                  user_data});
  }

  /** @brief 按配置名称绑定运行期 source。asset_manager 不拥有 source。 */
  [[nodiscard]] error bind_source(core::string_name name, resources::source& source);

  /** @brief 解除 source 名称绑定。 */
  [[nodiscard]] bool unbind_source(core::string_name name);

  /** @brief 查找已绑定 source。 */
  [[nodiscard]] resources::source* find_source(core::string_name name) const;

  /** @brief 清空 source 名称绑定。 */
  void clear_sources() noexcept;

  /** @brief 查询 source 绑定数量。 */
  [[nodiscard]] std::size_t source_count() const noexcept;

  /** @brief 注册 source 策略名到运行期 source 构造函数。 */
  [[nodiscard]] error register_source_factory(core::string_name type, source_factory factory);
  /** @brief 注册带强类型 options 的 source 策略构造函数。 */
  template <typename Options, typed_source_factory_fn<Options> Create>
  [[nodiscard]] error register_typed_source_factory(core::string_name type,
                                                    void* user_data = nullptr) {
    return register_source_factory(
        type,
        source_factory{&asset_manager::make_typed_source_factory<Options, Create>, user_data});
  }

  /** @brief 注册 manifest 资源类型到运行期 cache/loader 构造函数。 */
  [[nodiscard]] error register_type_factory(core::string_name resource_type, type_factory factory);
  /**
   * @brief 注册带强类型 options 的 manifest 资源类型构造函数。
   *
   * `cache_strategy` / `loader_strategy` 非空时会先匹配配置中的 cache/loader 策略名，
   * 再读取 typed options 并调用用户工厂。
   */
  template <typename Options, typed_type_factory_fn<Options> Create>
  [[nodiscard]] error register_typed_type_factory(core::string_name resource_type,
                                                  core::string_name cache_strategy = {},
                                                  core::string_name loader_strategy = {},
                                                  void* user_data = nullptr) {
    type_factory ops{&asset_manager::make_typed_type_factory<Options, Create>, user_data};
    ops.cache_strategy = cache_strategy;
    ops.loader_strategy = loader_strategy;
    return register_type_factory(resource_type, std::move(ops));
  }

public:
  /** @brief 同时注册某个资源类型的 cache、loader 和 manifest type。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& cache, resources::loader<T>& loader,
                                    core::string_name resource_type) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    error status = database_.register_type(cache, loader, resource_type);
    if (status) {
      return status;
    }

    return is_building_project() ? error{}
                                 : remember_manual_type_binding<T>(cache, loader, resource_type);
  }

  /** @brief 注册异步 loader 与 cache，可与同类型同步 loader 并存。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& cache, resources::async_loader<T>& source,
                                    core::string_name type) {
    if (auto status = database_.register_type(cache, source, type)) {
      return status;
    }
    return is_building_project() ? error{}
                                 : remember_manual_type_binding<T>(cache, source, type, true);
  }

  /** @brief 按资源类型和 GUID 查询已缓存资源。 */
  template <typename T> [[nodiscard]] T* find(resource_guid guid) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.find<T>(guid);
  }

  /** @brief 按 GUID 在全部已注册 cache 中查询资源。 */
  [[nodiscard]] resources::resource* find(resource_guid guid) const { return database_.find(guid); }

  /** @brief 使用已注册 loader 把资源加载进对应 cache，并返回强 handle。 */
  template <typename T> [[nodiscard]] handle_result<T> load_handle(resource_guid guid) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.load_handle<T>(guid);
  }

  /** @brief 使用已注册 loader 把资源加载进对应 cache，并返回非拥有指针。 */
  template <typename T> [[nodiscard]] result<T*> load(resource_guid guid) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.load<T>(guid);
  }

  /** @brief 使用已注册 loader 重新加载并替换对应 cache 中已有资源。 */
  template <typename T> [[nodiscard]] handle_result<T> reload_handle(resource_guid guid) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.reload_handle<T>(guid);
  }

  /** @brief 使用已注册 loader 重新加载并替换对应 cache 中已有资源。 */
  template <typename T> [[nodiscard]] result<T*> reload(resource_guid guid) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.reload<T>(guid);
  }

  /** @brief 为指定根资源生成同类型预加载计划。 */
  template <typename T> [[nodiscard]] preload_plan plan_preload(resource_guid root) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.plan_preload<T>(root);
  }

  /** @brief 生成跨类型预加载计划。 */
  [[nodiscard]] preload_plan plan_preload(resource_guid root) const {
    return database_.plan_preload(root);
  }

  /** @brief 按同一资源类型同步预加载资源。 */
  template <typename T> [[nodiscard]] preload_result preload(resource_guid root) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.preload<T>(root);
  }

  /** @brief 按 manifest type 同步预加载资源。 */
  [[nodiscard]] preload_result preload(resource_guid root) { return database_.preload(root); }

  /** @brief 使用默认调度获取资源；未配置时返回失败请求，缓存命中可立即完成。 */
  template <typename T> [[nodiscard]] load_request<T> request_load(resource_guid guid) {
    if (!execution_configured_) {
      return load_request<T>::failed({error_domain::resources, errc::invalid_argument}, resource_);
    }
    return request_load<T>(guid, execution_.worker, execution_.completion);
  }
  /** @brief 使用默认调度重载资源；符合当前发布资格时替换绑定，失败保留旧对象。 */
  template <typename T> [[nodiscard]] load_request<T> request_reload(resource_guid guid) {
    if (!execution_configured_) {
      return load_request<T>::failed({error_domain::resources, errc::invalid_argument}, resource_);
    }
    return request_reload<T>(guid, execution_.worker, execution_.completion);
  }
  /** @brief 使用默认调度独立刷新单资源；失败保留旧绑定。 */
  template <typename T> [[nodiscard]] load_request<T> request_refresh(resource_guid guid) {
    if (!execution_configured_) {
      return load_request<T>::failed({error_domain::resources, errc::invalid_argument}, resource_);
    }
    return request_refresh<T>(guid, execution_.worker, execution_.completion);
  }

  /** @brief 使用默认调度预加载同类型依赖闭包。 */
  template <typename T> [[nodiscard]] preload_request request_preload(resource_guid root) {
    if (!execution_configured_) {
      return preload_request::failed({error_domain::resources, errc::invalid_argument}, resource_);
    }
    return request_preload<T>(root, execution_.worker, execution_.completion);
  }
  /** @brief 使用默认调度按 manifest type 预加载依赖闭包。 */
  [[nodiscard]] preload_request request_preload(resource_guid root) {
    if (!execution_configured_) {
      return preload_request::failed({error_domain::resources, errc::invalid_argument}, resource_);
    }
    return request_preload(root, execution_.worker, execution_.completion);
  }

  /** @brief 在后台 executor 加载资源，并在完成 executor 提交到对应 cache。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_load(resource_guid guid,
                                             tasks::executor background_executor,
                                             tasks::executor completion_executor) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.request_load<T>(guid, std::move(background_executor),
                                     std::move(completion_executor));
  }

  /** @brief 在后台 executor 重载资源，拥有发布资格时在完成 executor 替换绑定。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_reload(resource_guid guid,
                                               tasks::executor background_executor,
                                               tasks::executor completion_executor) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.request_reload<T>(guid, std::move(background_executor),
                                       std::move(completion_executor));
  }
  /** @brief 在后台 executor 独立刷新单资源，最新意图成功后发布默认绑定。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_refresh(resource_guid guid,
                                                tasks::executor background_executor,
                                                tasks::executor completion_executor) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.request_refresh<T>(guid, std::move(background_executor),
                                        std::move(completion_executor));
  }

  /** @brief 按同一资源类型异步预加载依赖闭包。 */
  template <typename T>
  [[nodiscard]] preload_request request_preload(resource_guid root,
                                                tasks::executor background_executor,
                                                tasks::executor completion_executor) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.request_preload<T>(root, std::move(background_executor),
                                        std::move(completion_executor));
  }

  /** @brief 按 manifest type 异步预加载跨类型依赖闭包。 */
  [[nodiscard]] preload_request request_preload(resource_guid root,
                                                tasks::executor background_executor,
                                                tasks::executor completion_executor) {
    return database_.request_preload(root, std::move(background_executor),
                                     std::move(completion_executor));
  }

  /** @brief 用已注册 cache 解析资源引用。未命中时保留 GUID 并清除已解析指针。 */
  template <typename T> [[nodiscard]] bool resolve(resources::ref<T>& reference) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.resolve(reference);
  }

  /** @brief 判断已解析引用是否仍指向对应 cache 当前绑定版本。 */
  template <typename T> [[nodiscard]] bool is_current(const resources::ref<T>& reference) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.is_current(reference);
  }

  /** @brief 把依赖收集结果追加写入内部依赖图。 */
  [[nodiscard]] std::size_t
  add_dependencies(resource_guid resource,
                   const resources::resource_dependency_list& dependencies) {
    return database_.add_dependencies(resource, dependencies);
  }

  /** @brief 用依赖收集结果替换内部依赖图中某个资源的直接依赖。 */
  [[nodiscard]] std::size_t
  replace_dependencies(resource_guid resource,
                       const resources::resource_dependency_list& dependencies) {
    return database_.replace_dependencies(resource, dependencies);
  }

  /** @brief 查询某个资源变化后直接受影响的资源。 */
  [[nodiscard]] dependency_graph::guid_list affected_by(resource_guid resource) const {
    return database_.affected_by(resource);
  }

  /** @brief 查询某个资源变化后递归受影响的资源。 */
  [[nodiscard]] dependency_graph::guid_list affected_closure(resource_guid resource) const {
    return database_.affected_closure(resource);
  }

private:
  /**
   * @brief 使用 asset_manager 的内存资源创建对象，并由 asset_manager 负责销毁。
   *
   * 该入口主要面向 source/type factory 扩展。返回对象会在 asset_manager 析构时统一释放，
   * 调用方不应手动 delete。
   */
  template <typename T, typename... Args> [[nodiscard]] T* create_owned(Args&&... args) {
    auto value = memory::make_unique<T>(resource_, std::forward<Args>(args)...);
    if (!value)
      return nullptr;
    retain_owned(value.get(), &asset_manager::destroy_owned<T>);
    return value.release();
  }

private:
  struct manual_type_binding {
    using bind_fn = error (*)(asset_manager& manager, void* cache, void* loader, void* async_loader,
                              core::string_name resource_type);

    void* cache = nullptr;
    void* loader = nullptr;
    void* async_loader = nullptr;
    bind_fn bind = nullptr;
  };

  [[nodiscard]] resources::source*
  resolve_resource_source(const project_config& config,
                          const project_resource_config& resource_config) const;

  [[nodiscard]] error clear_config_runtime();
  [[nodiscard]] error rebuild_manual_type_bindings();
  [[nodiscard]] error ensure_file_system_from_config(const project_file_system_config& config,
                                                     asset_build_result::reason_list& reasons);
  [[nodiscard]] error configure_file_system_from_config(const project_file_system_config& config,
                                                        asset_build_result::reason_list& reasons);
  [[nodiscard]] error ensure_source_from_config(const project_source_config& config,
                                                asset_build_result::reason_list& reasons);
  [[nodiscard]] error ensure_type_from_config(const project_resource_type_config& config,
                                              asset_build_result::reason_list& reasons);
  [[nodiscard]] error register_builtin_file_system_factories();
  [[nodiscard]] error register_builtin_source_factories();
  [[nodiscard]] static error make_memory_file_system(file_system_factory_context& context,
                                                     const project_file_system_config& config,
                                                     void* user_data);
  [[nodiscard]] static error make_native_file_system(file_system_factory_context& context,
                                                     const project_file_system_config& config,
                                                     void* user_data);
  [[nodiscard]] static error make_resolver_file_system(file_system_factory_context& context,
                                                       const project_file_system_config& config,
                                                       void* user_data);
  [[nodiscard]] static error make_mount_file_system(file_system_factory_context& context,
                                                    const project_file_system_config& config,
                                                    void* user_data);
  [[nodiscard]] static error make_memory_file_source(source_factory_context& context,
                                                     const project_source_config& config,
                                                     void* user_data);
  [[nodiscard]] static error make_native_file_source(source_factory_context& context,
                                                     const project_source_config& config,
                                                     void* user_data);
  [[nodiscard]] static error make_bound_file_source(source_factory_context& context,
                                                    const project_source_config& config,
                                                    void* user_data);

  template <typename Options, typed_file_system_factory_fn<Options> Create>
  [[nodiscard]] static error
  make_typed_file_system_factory(file_system_factory_context& context,
                                 const project_file_system_config& config, void* user_data) {
    auto options = read_typed_factory_options<Options>(config.options, context.resource);
    if (options.failed()) {
      if (context.reasons != nullptr) {
        append_typed_factory_option_reasons(*context.reasons, options);
      }
      return options.status;
    }

    return Create(context, config, options.value, user_data);
  }

  template <typename Options, typed_source_factory_fn<Options> Create>
  [[nodiscard]] static error make_typed_source_factory(source_factory_context& context,
                                                       const project_source_config& config,
                                                       void* user_data) {
    auto options = read_typed_factory_options<Options>(config.options, context.resource);
    if (options.failed()) {
      if (context.reasons != nullptr) {
        append_typed_factory_option_reasons(*context.reasons, options);
      }
      return options.status;
    }

    return Create(context, config, options.value, user_data);
  }

  template <typename Options, typed_type_factory_fn<Options> Create>
  [[nodiscard]] static error make_typed_type_factory(type_factory_context& context,
                                                     const project_resource_type_config& config,
                                                     void* user_data) {
    auto options = read_typed_factory_options<Options>(config.options, context.resource);
    if (options.failed()) {
      if (context.reasons != nullptr) {
        append_typed_factory_option_reasons(*context.reasons, options);
      }
      return options.status;
    }

    return Create(context, config, options.value, user_data);
  }

  template <typename T> static void destroy_owned(memory_resource* resource, void* value) noexcept {
    memory::destroy(static_cast<T*>(value), resource);
  }

  template <typename T>
  [[nodiscard]] static error rebind_manual_type(asset_manager& manager, void* cache, void* loader,
                                                void* async_loader, core::string_name type) {
    if (loader) {
      if (auto status =
              manager.database_.register_type(*static_cast<resources::cache<T>*>(cache),
                                              *static_cast<resources::loader<T>*>(loader), type)) {
        return status;
      }
    }
    return async_loader ? manager.database_.register_type(
                              *static_cast<resources::cache<T>*>(cache),
                              *static_cast<resources::async_loader<T>*>(
                                  static_cast<resources::loader<T>*>(async_loader)),
                              type)
                        : error{};
  }

  template <typename T>
  [[nodiscard]] error
  remember_manual_type_binding(resources::cache<T>& cache, resources::loader<T>& loader,
                               core::string_name type, bool asynchronous = false) {
    manual_type_binding binding{&cache, asynchronous ? nullptr : &loader,
                                asynchronous ? &loader : nullptr,
                                &asset_manager::rebind_manual_type<T>};
    return remember_manual_type_binding_erased(type, binding);
  }

  friend class asset_group;
  friend class project_builder;
  asset_execution_options execution_;
  bool execution_configured_ = false;
  memory_resource* resource_ = nullptr;
  resources::manifest manifest_;
  resources::dependency_graph dependency_graph_;
  resources::database database_;
  // 工程装配状态与日常请求状态分离；具体容器和配置标记不暴露在公共头中。
  struct project_state;
  struct project_deleter {
    memory_resource* resource = nullptr;
    void operator()(project_state* state) const noexcept;
  };
  std::unique_ptr<project_state, project_deleter> project_;

  [[nodiscard]] resources::provider& project_provider() noexcept;
  [[nodiscard]] const resources::provider& project_provider() const noexcept;
  [[nodiscard]] error use_project_provider_snapshot();
  [[nodiscard]] bool is_building_project() const noexcept;
  using owned_destroy_fn = void (*)(memory_resource*, void*) noexcept;
  void retain_owned(void* value, owned_destroy_fn destroy);
  [[nodiscard]] error remember_manual_type_binding_erased(core::string_name type,
                                                          manual_type_binding binding);
};

/**
 * @brief 固定调度组合的轻量非拥有加载上下文。
 *
 * 不拥有 manager/cache/线程池；manager 必须活到全部访问及实际请求清理完成。
 * 复制、移动与析构不取消请求；runtime 重配后旧 group 返回 frozen。
 * 不提供组级取消或独立缓存；共享请求沿用首次操作的调度，等待者不保证在本组线程恢复。
 */
class asset_group final {
public:
  asset_group() = default;
  /** @brief 获取单资源，命中 cache 立即完成，否则订阅共享加载。 */
  template <typename T>
  [[nodiscard]] resources::load_request<T> request_load(resources::resource_guid guid) const {
    if (auto status = check()) {
      return resources::load_request<T>::failed(status, resource());
    }
    return manager_->request_load<T>(guid, execution_.worker, execution_.completion);
  }
  /** @brief 重载单资源；被新刷新取代时可返回未发布的旧结果，失败保留原绑定。 */
  template <typename T>
  [[nodiscard]] resources::load_request<T> request_reload(resources::resource_guid guid) const {
    if (auto status = check()) {
      return resources::load_request<T>::failed(status, resource());
    }
    return manager_->request_reload<T>(guid, execution_.worker, execution_.completion);
  }
  /** @brief 独立刷新单资源，不加入旧操作；不刷新依赖闭包。 */
  template <typename T>
  [[nodiscard]] resources::load_request<T> request_refresh(resources::resource_guid guid) const {
    if (auto status = check()) {
      return resources::load_request<T>::failed(status, resource());
    }
    return manager_->request_refresh<T>(guid, execution_.worker, execution_.completion);
  }

  /** @brief 预加载根资源的同类型依赖闭包，不强制刷新已有缓存。 */
  template <typename T>
  [[nodiscard]] resources::preload_request request_preload(resources::resource_guid root) const {
    if (auto status = check()) {
      return resources::preload_request::failed(status, resource());
    }
    return manager_->request_preload<T>(root, execution_.worker, execution_.completion);
  }
  /** @brief 按 manifest type 预加载根资源及跨类型依赖闭包。 */
  [[nodiscard]] resources::preload_request request_preload(resources::resource_guid root) const {
    if (auto status = check()) {
      return resources::preload_request::failed(status, resource());
    }
    return manager_->request_preload(root, execution_.worker, execution_.completion);
  }

private:
  friend class asset_manager;
  asset_group(asset_manager& manager, asset_execution_options execution)
      : manager_(&manager), execution_(std::move(execution)),
        generation_(manager.database_.configuration_generation()),
        configured_(execution_.worker.valid() && execution_.completion.valid()) {}
  [[nodiscard]] error check() const noexcept {
    if (!manager_ || !configured_) {
      return {error_domain::resources, errc::invalid_argument};
    }
    if (generation_ != manager_->database_.configuration_generation()) {
      return {error_domain::resources, errc::frozen};
    }
    return {};
  }
  [[nodiscard]] memory_resource* resource() const noexcept {
    return manager_ ? manager_->resource() : nullptr;
  }
  asset_manager* manager_ = nullptr;
  asset_execution_options execution_;
  std::uint64_t generation_ = 0;
  bool configured_ = false;
};

inline asset_group asset_manager::create_group(tasks::executor worker, tasks::executor completion) {
  return asset_group{*this, {std::move(worker), std::move(completion)}};
}

} // namespace caors::assets

#endif
