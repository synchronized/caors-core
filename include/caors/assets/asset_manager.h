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
 * 非拥有指针的生命周期拼装问题。它可以根据 `project_config` 中的策略名创建内置 source，并可通过
 * 类型工厂把资源类型描述映射到运行期 cache/loader。
 */
class CAORS_API asset_manager final {
public:
  /** @brief source 工厂调用时收到的运行期上下文。 */
  struct source_factory_context {
    /** @brief 正在构建资源环境的 asset_manager。 */
    asset_manager& manager;
    /** @brief asset_manager 内部对象默认使用的内存资源。 */
    memory_resource* resource = nullptr;
    /** @brief 可选构建诊断列表；仅在 build_detailed() 路径中有效。 */
    asset_build_result::reason_list* reasons = nullptr;
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
  };

  /** @brief file_system 工厂调用时收到的运行期上下文。 */
  struct file_system_factory_context {
    /** @brief 正在构建资源环境的 asset_manager。 */
    asset_manager& manager;
    /** @brief asset_manager 内部对象默认使用的内存资源。 */
    memory_resource* resource = nullptr;
    /** @brief 可选构建诊断列表；仅在 build_detailed() 路径中有效。 */
    asset_build_result::reason_list* reasons = nullptr;
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

      type_factory_context context{manager, manager.resource(), manager.provider(),
                                   manager.database(), reasons};
      return create(context, config, user_data);
    }
  };

  explicit asset_manager(memory_resource* resource = nullptr);
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

  /** @brief 按配置名称绑定运行期 file_system。asset_manager 不拥有 file_system。 */
  [[nodiscard]] error bind_file_system(core::string_name name, io::file_system& file_system);

  /** @brief 解除 file_system 名称绑定。 */
  [[nodiscard]] bool unbind_file_system(core::string_name name);

  /** @brief 查找已绑定 file_system。 */
  [[nodiscard]] io::file_system* find_file_system(core::string_name name) const;

  /** @brief 清空 file_system 名称绑定。 */
  void clear_file_systems() noexcept { file_systems_.clear(); }

  /** @brief 查询 file_system 绑定数量。 */
  [[nodiscard]] std::size_t file_system_count() const noexcept { return file_systems_.size(); }

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
  void clear_sources() noexcept { sources_.clear(); }

  /** @brief 查询 source 绑定数量。 */
  [[nodiscard]] std::size_t source_count() const noexcept { return sources_.size(); }

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

  /**
   * @brief 根据工程配置重建 manifest、provider 和 dependency graph。
   *
   * 该函数会清空上一轮配置创建的 source/cache/loader 等运行期对象，并重建
   * manifest/provider/dependency graph。用户手动 `bind_source()` 和 `register_type()`
   * 的外部绑定会保留。
   */
  [[nodiscard]] error build(const project_config& config);

  /**
   * @brief 根据工程配置重建运行期资源环境，并返回详细诊断。
   *
   * 普通调用可以继续使用 `build()`。工具链、编辑器和测试需要定位失败阶段时使用该入口。
   */
  [[nodiscard]] asset_build_result build_detailed(const project_config& config);

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

    return building_config_runtime_ ? error{}
                                    : remember_manual_type_binding<T>(cache, loader, resource_type);
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

  /** @brief 在后台 executor 重载资源，并在完成 executor 替换对应 cache 绑定。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_reload(resource_guid guid,
                                               tasks::executor background_executor,
                                               tasks::executor completion_executor) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return database_.request_reload<T>(guid, std::move(background_executor),
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

  /**
   * @brief 使用 asset_manager 的内存资源创建对象，并由 asset_manager 负责销毁。
   *
   * 该入口主要面向 source/type factory 扩展。返回对象会在 asset_manager 析构时统一释放，
   * 调用方不应手动 delete。
   */
  template <typename T, typename... Args> [[nodiscard]] T* create_owned(Args&&... args) {
    T* value = memory::make<T>(resource_, std::forward<Args>(args)...);
    if (value == nullptr) {
      return nullptr;
    }

    owned_objects_.emplace_back(resource_, value, &asset_manager::destroy_owned<T>,
                                building_config_runtime_);
    return value;
  }

private:
  using file_system_factory_map = core::hash_map<core::string_name, file_system_factory>;
  using source_factory_map = core::hash_map<core::string_name, source_factory>;
  using type_factory_map = core::hash_map<core::string_name, type_factory>;

  struct file_system_binding {
    io::file_system* file_system = nullptr;
    bool config_created = false;
  };

  struct source_binding {
    resources::source* source = nullptr;
    bool config_created = false;
  };

  using file_system_map = core::hash_map<core::string_name, file_system_binding>;
  using source_map = core::hash_map<core::string_name, source_binding>;

  struct CAORS_API owned_runtime_object {
    using destroy_fn = void (*)(memory_resource* resource, void* value) noexcept;

    owned_runtime_object() noexcept = default;
    owned_runtime_object(memory_resource* resource, void* value, destroy_fn destroy,
                         bool config_created) noexcept;
    ~owned_runtime_object();

    owned_runtime_object(const owned_runtime_object&) = delete;
    owned_runtime_object& operator=(const owned_runtime_object&) = delete;

    owned_runtime_object(owned_runtime_object&& other) noexcept;
    owned_runtime_object& operator=(owned_runtime_object&& other) noexcept;

    void reset() noexcept;

    memory_resource* resource = nullptr;
    void* value = nullptr;
    destroy_fn destroy = nullptr;
    bool config_created = false;
  };

  struct manual_type_binding {
    using bind_fn = error (*)(asset_manager& manager, void* cache, void* loader,
                              core::string_name resource_type);

    void* cache = nullptr;
    void* loader = nullptr;
    bind_fn bind = nullptr;
  };

  using owned_object_list = core::vector<owned_runtime_object>;
  using manual_type_map = core::hash_map<core::string_name, manual_type_binding>;

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
                                                core::string_name resource_type) {
    return manager.database_.register_type(*static_cast<resources::cache<T>*>(cache),
                                           *static_cast<resources::loader<T>*>(loader),
                                           resource_type);
  }

  template <typename T>
  [[nodiscard]] error remember_manual_type_binding(resources::cache<T>& cache,
                                                   resources::loader<T>& loader,
                                                   core::string_name resource_type) {
    manual_type_binding binding{&cache, &loader, &asset_manager::rebind_manual_type<T>};
    const auto position = manual_type_bindings_.find(resource_type);
    if (position != manual_type_bindings_.end()) {
      return position->second.cache == binding.cache && position->second.loader == binding.loader &&
                     position->second.bind == binding.bind
                 ? error{}
                 : error{error_domain::resources, errc::already_exists};
    }

    manual_type_bindings_.emplace(resource_type, binding);
    return {};
  }

  memory_resource* resource_ = nullptr;
  resources::manifest manifest_;
  resources::provider provider_;
  resources::dependency_graph dependency_graph_;
  resources::database database_;
  file_system_map file_systems_;
  source_map sources_;
  file_system_factory_map file_system_factories_;
  source_factory_map source_factories_;
  type_factory_map type_factories_;
  owned_object_list owned_objects_;
  manual_type_map manual_type_bindings_;
  bool building_config_runtime_ = false;
};

} // namespace caors::assets

#endif
