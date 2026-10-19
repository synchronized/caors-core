// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_DATABASE_H_
#define CAORS_RESOURCES_DATABASE_H_

#include "caors/core/type_guid.h"
#include "caors/meta/query.h"
#include "caors/resources/async.h"
#include "caors/resources/dependency_graph.h"
#include "caors/resources/dependency_list.h"
#include "caors/resources/detail/type_entry_ops.h"
#include "caors/resources/io.h"
#include "caors/resources/manifest.h"
#include "caors/resources/preload.h"
#include "caors/resources/source.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>

namespace caors::resources {
namespace detail {
struct resource_activity final {
  std::atomic_size_t count{0};
};
struct resource_activity_lease final {
  explicit resource_activity_lease(std::shared_ptr<resource_activity> value)
      : owner(std::move(value)) {
    owner->count.fetch_add(1, std::memory_order_relaxed);
  }
  ~resource_activity_lease() { owner->count.fetch_sub(1, std::memory_order_release); }
  std::shared_ptr<resource_activity> owner;
};
} // namespace detail

/**
 * @brief 资源系统的轻量编排入口。
 *
 * `database` 不拥有 `manifest`、`provider`、`cache<T>` 或 `loader<T>`；它只保存这些对象的指针，
 * 把清单同步、类型化加载、重载、引用解析和依赖图查询串成一条运行期主流程。调用方必须保证传入对象
 * 的生命周期长于 database 中对应绑定。
 */
class database final {
public:
  explicit database(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        activity_(std::allocate_shared<detail::resource_activity>(
            allocator<detail::resource_activity>{resource_})),
        entries_(typename entry_map::allocator_type{resource_}),
        entries_by_resource_type_(typename resource_type_map::allocator_type{resource_}),
        entries_by_type_guid_(typename type_guid_map::allocator_type{resource_}) {}

  database(const database&) = delete;
  database& operator=(const database&) = delete;
  database(database&&) = delete;
  database& operator=(database&&) = delete;

  /** @brief 返回 database 内部索引使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询排队、宿主 I/O 或完成提交是否仍持有实际操作。 */
  [[nodiscard]] bool has_active_operations() const noexcept {
    return activity_->count.load(std::memory_order_acquire) != 0;
  }
  /** @brief runtime 绑定代次；用于使重配前的加载上下文失效。 */
  [[nodiscard]] std::uint64_t configuration_generation() const noexcept { return generation_; }
  /** @brief 设置当前清单视图。database 不拥有该对象。 */
  void set_manifest(const resources::manifest* value) noexcept { manifest_ = value; }
  void set_manifest(const resources::manifest& value) noexcept { set_manifest(&value); }
  [[nodiscard]] const resources::manifest* manifest() const noexcept { return manifest_; }

  /** @brief 设置当前字节 provider。database 不拥有该对象。 */
  void set_provider(resources::provider* value) noexcept { provider_ = value; }
  void set_provider(resources::provider& value) noexcept { set_provider(&value); }
  [[nodiscard]] resources::provider* provider() const noexcept { return provider_; }

  /** @brief 设置当前资源依赖图。database 不拥有该对象。 */
  void set_dependency_graph(resources::dependency_graph* value) noexcept {
    dependency_graph_ = value;
  }
  void set_dependency_graph(resources::dependency_graph& value) noexcept {
    set_dependency_graph(&value);
  }
  [[nodiscard]] resources::dependency_graph* dependency_graph() const noexcept {
    return dependency_graph_;
  }

  /**
   * @brief 清空已注册的资源类型绑定。
   *
   * 该函数只清除 cache、loader、manifest type alias、type_guid 和异步请求表绑定，不修改当前
   * manifest、provider 或 dependency graph 指针。
   */
  void clear_registered_types() noexcept {
    ++generation_;
    entries_.clear();
    entries_by_resource_type_.clear();
    entries_by_type_guid_.clear();
  }

  /** @brief 使用当前 manifest 把 GUID/location 绑定到当前 provider。 */
  [[nodiscard]] error bind_manifest(resources::source& source) const {
    if (provider_ == nullptr || manifest_ == nullptr) {
      return {error_domain::resources, errc::invalid_argument};
    }

    return resources::bind_manifest(*provider_, source, *manifest_);
  }

  /** @brief 使用当前 manifest 同步当前 provider 中的 GUID/location 绑定。 */
  [[nodiscard]] error sync_manifest(resources::source& source) const {
    if (provider_ == nullptr || manifest_ == nullptr) {
      return {error_domain::resources, errc::invalid_argument};
    }

    return resources::sync_manifest(*provider_, source, *manifest_);
  }

  /** @brief 使用当前 manifest 替换当前 provider 中的全部 GUID/location 绑定。 */
  [[nodiscard]] error replace_manifest(resources::source& source) const {
    if (provider_ == nullptr || manifest_ == nullptr) {
      return {error_domain::resources, errc::invalid_argument};
    }

    return resources::replace_manifest(*provider_, source, *manifest_);
  }

  /** @brief 注册某个资源类型的拥有式 cache。 */
  template <typename T> [[nodiscard]] error register_cache(resources::cache<T>& target) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    type_entry* entry = nullptr;
    const error status = ensure_entry<T>(entry);
    if (status) {
      return status;
    }

    if (entry->cache != nullptr && entry->cache != &target) {
      return {error_domain::resources, errc::already_exists};
    }

    entry->cache = &target;
    entry->ops.find = &find_in_cache<T>;
    entry->ops.resolve = &resolve_in_cache<T>;
    entry->ops.is_current = &is_current_in_cache<T>;
    entry->ops.load = &load_in_cache<T>;
    entry->ops.request_load = &request_load_in_cache<T>;
    return {};
  }

  /** @brief 注册某个资源类型的拥有式 cache，并绑定 manifest type。 */
  template <typename T>
  [[nodiscard]] error register_cache(resources::cache<T>& target, core::string_name resource_type) {
    error status = register_cache(target);
    if (status) {
      return status;
    }

    return bind_resource_type<T>(resource_type);
  }

  /** @brief 注册某个资源类型的拥有式 cache，并绑定稳定 type_guid。 */
  template <typename T>
  [[nodiscard]] error register_cache(resources::cache<T>& target, type_guid guid) {
    error status = register_cache(target);
    if (status) {
      return status;
    }

    return bind_type_guid<T>(guid);
  }

  /** @brief 注册某个资源类型的加载器。 */
  template <typename T> [[nodiscard]] error register_loader(resources::loader<T>& source) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    type_entry* entry = nullptr;
    const error status = ensure_entry<T>(entry);
    if (status) {
      return status;
    }

    if (entry->loader != nullptr && entry->loader != &source) {
      return {error_domain::resources, errc::already_exists};
    }

    entry->loader = &source;
    entry->ops.load = &load_in_cache<T>;
    entry->ops.request_load = &request_load_in_cache<T>;
    return {};
  }

  /** @brief 注册原生异步能力；不替换已有同步 loader。 */
  template <typename T> [[nodiscard]] error register_loader(resources::async_loader<T>& source) {
    type_entry* entry = nullptr;
    if (auto status = ensure_entry<T>(entry)) {
      return status;
    }
    auto* base = static_cast<resources::loader<T>*>(&source);
    if (entry->async_loader && entry->async_loader != base) {
      return {error_domain::resources, errc::already_exists};
    }
    entry->async_loader = base;
    entry->ops.load = &load_in_cache<T>;
    entry->ops.request_load = &request_load_in_cache<T>;
    return {};
  }
  /** @brief 注册异步 loader 并绑定资源类型。 */
  template <typename T>
  [[nodiscard]] error register_loader(resources::async_loader<T>& source, core::string_name type) {
    if (auto status = register_loader(source)) {
      return status;
    }
    return bind_resource_type<T>(type);
  }
  /** @brief 注册异步 loader 并绑定资源类型。 */
  template <typename T>
  [[nodiscard]] error register_loader(resources::async_loader<T>& source, type_guid type) {
    if (auto status = register_loader(source)) {
      return status;
    }
    return bind_type_guid<T>(type);
  }
  /** @brief 注册 cache 与异步 loader。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& target,
                                    resources::async_loader<T>& source) {
    if (auto status = register_cache(target)) {
      return status;
    }
    return register_loader(source);
  }
  /** @brief 注册 cache、异步 loader 及资源类型。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& target, resources::async_loader<T>& source,
                                    core::string_name type) {
    if (auto status = register_type(target, source)) {
      return status;
    }
    return bind_resource_type<T>(type);
  }
  /** @brief 注册 cache、异步 loader 及资源类型。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& target, resources::async_loader<T>& source,
                                    type_guid type) {
    if (auto status = register_type(target, source)) {
      return status;
    }
    return bind_type_guid<T>(type);
  }
  /** @brief 注册某个资源类型的加载器，并绑定 manifest type。 */
  template <typename T>
  [[nodiscard]] error register_loader(resources::loader<T>& source,
                                      core::string_name resource_type) {
    error status = register_loader(source);
    if (status) {
      return status;
    }

    return bind_resource_type<T>(resource_type);
  }

  /** @brief 注册某个资源类型的加载器，并绑定稳定 type_guid。 */
  template <typename T>
  [[nodiscard]] error register_loader(resources::loader<T>& source, type_guid guid) {
    error status = register_loader(source);
    if (status) {
      return status;
    }

    return bind_type_guid<T>(guid);
  }

  /** @brief 同时注册某个资源类型的 cache 和 loader。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& target, resources::loader<T>& source) {
    error status = register_cache(target);
    if (status) {
      return status;
    }

    return register_loader(source);
  }

  /** @brief 同时注册某个资源类型的 cache、loader 和 manifest type。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& target, resources::loader<T>& source,
                                    core::string_name resource_type) {
    error status = register_type(target, source);
    if (status) {
      return status;
    }

    return bind_resource_type<T>(resource_type);
  }

  /** @brief 同时注册某个资源类型的 cache、loader 和稳定 type_guid。 */
  template <typename T>
  [[nodiscard]] error register_type(resources::cache<T>& target, resources::loader<T>& source,
                                    type_guid guid) {
    error status = register_type(target, source);
    if (status) {
      return status;
    }

    return bind_type_guid<T>(guid);
  }

  /**
   * @brief 把外部资源数据库中的类型 token 绑定到已有 manifest type。
   *
   * 该入口不会创建 cache 或 loader，只让跨类型预加载在读取到 `alias` 时复用
   * `resource_type` 对应的已注册资源类型。
   */
  [[nodiscard]] error bind_resource_type_alias(core::string_name alias,
                                               core::string_name resource_type) {
    if (!alias || !resource_type) {
      return {error_domain::resources, errc::invalid_argument};
    }

    const type_entry* entry = find_entry(resource_type);
    if (entry == nullptr || !entry->id) {
      return {error_domain::resources, errc::not_found};
    }

    const auto position = entries_by_resource_type_.find(alias);
    if (position != entries_by_resource_type_.end() && position->second != entry->id.value()) {
      return {error_domain::resources, errc::already_exists};
    }

    entries_by_resource_type_[alias] = entry->id.value();
    return {};
  }

  /**
   * @brief 把稳定 type_guid 绑定到已有 manifest type。
   *
   * manifest 条目的 `type` 可以保存该 GUID 的字符串形式，跨类型预加载会自动解析并找到对应
   * cache/loader。
   */
  [[nodiscard]] error bind_resource_type_guid(type_guid guid, core::string_name resource_type) {
    if (!guid || !resource_type) {
      return {error_domain::resources, errc::invalid_argument};
    }

    type_entry* entry = find_entry(resource_type);
    if (entry == nullptr || !entry->id) {
      return {error_domain::resources, errc::not_found};
    }

    return bind_entry_type_guid(*entry, guid);
  }

  /** @brief 把指定 C++ 资源类型绑定到稳定 type_guid。 */
  template <typename T> [[nodiscard]] error bind_type_guid(type_guid guid) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    if (!guid) {
      return {error_domain::resources, errc::invalid_argument};
    }

    type_entry* entry = nullptr;
    const error status = ensure_entry<T>(entry);
    if (status) {
      return status;
    }

    return bind_entry_type_guid(*entry, guid);
  }

  /** @brief 按资源类型和 GUID 查询已缓存资源。 */
  template <typename T> [[nodiscard]] T* find(resource_guid guid) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    const type_entry* entry = find_entry<T>();
    if (entry != nullptr && entry->cache != nullptr) {
      return static_cast<resources::cache<T>*>(entry->cache)->find(guid);
    }

    return dynamic_cast<T*>(find(guid));
  }

  /** @brief 按 GUID 在所有已注册 cache 中查找资源。 */
  [[nodiscard]] resources::resource* find(resource_guid guid) const {
    if (!guid) {
      return nullptr;
    }

    for (const auto& pair : entries_) {
      const type_entry& entry = pair.second;
      if (entry.cache == nullptr || entry.ops.find == nullptr) {
        continue;
      }

      resources::resource* value = entry.ops.find(entry.cache, guid);
      if (value != nullptr) {
        return value;
      }
    }

    return nullptr;
  }

  /** @brief 使用已注册 loader 把资源加载进对应 cache，并返回强 handle。 */
  template <typename T> [[nodiscard]] handle_result<T> load_handle(resource_guid guid) {
    if (!guid) {
      return {{error_domain::resources, errc::invalid_argument}, {}};
    }
    type_entry* entry = find_entry<T>();
    if (entry == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
    }

    if (entry->cache == nullptr || entry->request_loader() == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
    }

    auto& target = *static_cast<resources::cache<T>*>(entry->cache);
    if (auto existing = target.find_handle(guid)) {
      return {{}, std::move(existing)};
    }
    if (!entry->loader) {
      return {{error_domain::resources, errc::unsupported}, {}};
    }
    return resources::load_handle_into(*static_cast<resources::cache<T>*>(entry->cache),
                                       *static_cast<resources::loader<T>*>(entry->loader), guid);
  }

  /** @brief 使用已注册 loader 把资源加载进对应 cache，并返回非拥有指针。 */
  template <typename T> [[nodiscard]] result<T*> load(resource_guid guid) {
    handle_result<T> loaded = load_handle<T>(guid);
    return {loaded.status, loaded.value.get()};
  }

  /** @brief 使用已注册 loader 重新加载并替换对应 cache 中已有资源。 */
  template <typename T> [[nodiscard]] handle_result<T> reload_handle(resource_guid guid) {
    if (!guid) {
      return {{error_domain::resources, errc::invalid_argument}, {}};
    }
    type_entry* entry = find_entry<T>();
    if (entry == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
    }

    if (entry->cache == nullptr || entry->request_loader() == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
    }

    if (!entry->loader) {
      return {{error_domain::resources, errc::unsupported}, {}};
    }
    return resources::reload_handle_into(*static_cast<resources::cache<T>*>(entry->cache),
                                         *static_cast<resources::loader<T>*>(entry->loader), guid);
  }

  /** @brief 使用已注册 loader 重新加载并替换对应 cache 中已有资源。 */
  template <typename T> [[nodiscard]] result<T*> reload(resource_guid guid) {
    handle_result<T> reloaded = reload_handle<T>(guid);
    return {reloaded.status, reloaded.value.get()};
  }

  /**
   * @brief 为指定根资源生成同步预加载计划。
   *
   * 第一版按当前 `dependency_graph` 展开 GUID 依赖闭包，并假定该闭包内资源都由同一个 `T`
   * 类型的 cache/loader 处理。若当前 database 设置了 manifest，缺失清单条目会让计划失败。
   */
  template <typename T> [[nodiscard]] preload_plan plan_preload(resource_guid root) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    preload_plan plan{resource_};
    if (!root) {
      plan.status = {error_domain::resources, errc::invalid_argument};
      return plan;
    }

    if (dependency_graph_ != nullptr) {
      plan.cycle = dependency_graph_->find_cycle(root);
      if (!plan.cycle.empty()) {
        plan.status = {error_domain::resources, errc::cycle_detected};
        return plan;
      }
    }

    dependency_graph::guid_set visited{
        typename dependency_graph::guid_set::allocator_type{resource_}};
    append_preload_guid<T>(root, visited, plan);
    if (plan.status.succeeded() && !plan.missing_manifest.empty()) {
      plan.status = {error_domain::resources, errc::not_found};
    }

    return plan;
  }

  /**
   * @brief 按 manifest type 为指定根资源生成跨类型同步预加载计划。
   *
   * 该重载要求 database 设置 manifest。每个资源 GUID 会从 manifest 条目读取 type，再映射到
   * `register_type(cache, loader, type)` 绑定的 cache/loader。
   */
  [[nodiscard]] preload_plan plan_preload(resource_guid root) const {
    preload_plan plan{resource_};
    if (!root) {
      plan.status = {error_domain::resources, errc::invalid_argument};
      return plan;
    }

    if (manifest_ == nullptr) {
      plan.status = {error_domain::resources, errc::invalid_argument};
      return plan;
    }

    if (dependency_graph_ != nullptr) {
      plan.cycle = dependency_graph_->find_cycle(root);
      if (!plan.cycle.empty()) {
        plan.status = {error_domain::resources, errc::cycle_detected};
        return plan;
      }
    }

    dependency_graph::guid_set visited{
        typename dependency_graph::guid_set::allocator_type{resource_}};
    append_dispatched_preload_guid(root, visited, plan);
    finalize_preload_status(plan);
    return plan;
  }

  /**
   * @brief 按依赖顺序同步预加载资源。
   *
   * 该函数先生成 `plan_preload<T>()`，再按计划中的 `load_order` 调用已注册 loader/cache。
   * 已缓存资源不会重复加载。第一版不根据 manifest type 自动选择不同资源类型。
   */
  template <typename T> [[nodiscard]] preload_result preload(resource_guid root) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    preload_result result{resource_};
    result.plan = plan_preload<T>(root);
    if (result.plan.failed()) {
      result.status = result.plan.status;
      detail::append_preload_plan_failures(result);
      return result;
    }

    type_entry* entry = find_entry<T>();
    if (entry == nullptr || entry->cache == nullptr || entry->request_loader() == nullptr) {
      result.status = {error_domain::resources, errc::not_found};
      for (const preload_item& item : result.plan.load_order) {
        detail::append_preload_failure(result, item, result.status);
      }
      return result;
    }

    for (const preload_item& item : result.plan.load_order) {
      handle_result<T> loaded = resources::load_handle_into(
          *static_cast<resources::cache<T>*>(entry->cache),
          *static_cast<resources::loader<T>*>(entry->loader ? entry->loader : entry->async_loader),
          item.guid);
      if (loaded.failed()) {
        result.status = loaded.status;
        detail::append_preload_failure(result, item, loaded.status);
        return result;
      }

      result.loaded.push_back(item);
    }

    return result;
  }

  /**
   * @brief 按 manifest type 同步预加载资源。
   *
   * 该重载支持依赖闭包中存在不同资源类型。每个资源项会按 manifest type 分派到对应已注册
   * cache/loader。
   */
  [[nodiscard]] preload_result preload(resource_guid root) {
    preload_result result{resource_};
    result.plan = plan_preload(root);
    if (result.plan.failed()) {
      result.status = result.plan.status;
      detail::append_preload_plan_failures(result);
      return result;
    }

    for (const preload_item& item : result.plan.load_order) {
      type_entry* entry = find_entry(item.type);
      if (entry == nullptr || entry->cache == nullptr || entry->request_loader() == nullptr ||
          entry->ops.load == nullptr) {
        result.status = {error_domain::resources, errc::not_found};
        detail::append_preload_failure(result, item, result.status);
        return result;
      }

      const error status = entry->ops.load(
          entry->cache, entry->loader ? entry->loader : entry->async_loader, item.guid);
      if (status) {
        result.status = status;
        detail::append_preload_failure(result, item, status);
        return result;
      }

      result.loaded.push_back(item);
    }

    return result;
  }

  /** @brief 异步加载资源，并通过指定执行目标提交到对应 cache。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_load(resource_guid guid, tasks::executor executor,
                                             tasks::executor completion_executor) {
    type_entry* entry = find_entry<T>();
    if (entry == nullptr || entry->cache == nullptr || entry->request_loader() == nullptr) {
      return load_request<T>::failed({error_domain::resources, errc::not_found}, resource_);
    }

    auto* requests = static_cast<resources::load_request_table<T>*>(entry->load_requests.get());
    if (requests == nullptr) {
      return load_request<T>::failed({error_domain::resources, errc::backend_failure}, resource_);
    }

    return detail::request_load_into_impl(
        *static_cast<resources::cache<T>*>(entry->cache),
        *static_cast<resources::loader<T>*>(entry->request_loader()), guid, executor,
        completion_executor, requests, true, {}, {}, make_activity());
  }

  /** @brief 异步重载资源，并通过指定执行目标替换对应 cache 绑定。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_reload(resource_guid guid, tasks::executor executor,
                                               tasks::executor completion_executor) {
    type_entry* entry = find_entry<T>();
    if (entry == nullptr || entry->cache == nullptr || entry->request_loader() == nullptr) {
      return load_request<T>::failed({error_domain::resources, errc::not_found}, resource_);
    }

    auto* requests = static_cast<resources::load_request_table<T>*>(entry->reload_requests.get());
    if (requests == nullptr) {
      return load_request<T>::failed({error_domain::resources, errc::backend_failure}, resource_);
    }

    return detail::request_reload_into_impl(
        *static_cast<resources::cache<T>*>(entry->cache),
        *static_cast<resources::loader<T>*>(entry->request_loader()), guid, executor,
        completion_executor, requests, make_activity());
  }

  /**
   * @brief 按同一资源类型异步预加载依赖闭包。
   *
   * 该函数先生成 `plan_preload<T>()`，再把计划中的每个资源提交到 executor。后台任务只执行
   * loader 阶段，cache 写入会延迟到 completion executor 运行。
   */
  template <typename T>
  [[nodiscard]] preload_request request_preload(resource_guid root, tasks::executor executor,
                                                tasks::executor completion_executor) {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    preload_plan plan = plan_preload<T>(root);
    preload_request request = preload_request::from_plan(plan, resource_);
    auto state = detail::preload_request_access::state(request);
    if (!detail::preload_request_access::loading(state)) {
      return request;
    }

    type_entry* entry = find_entry<T>();
    if (entry == nullptr || entry->cache == nullptr || entry->request_loader() == nullptr ||
        entry->ops.request_load == nullptr) {
      detail::preload_request_access::fail_all_items(state,
                                                     {error_domain::resources, errc::not_found});
      return request;
    }

    for (const preload_item& item : plan.load_order) {
      const error status =
          entry->ops.request_load(entry->cache, entry->request_loader(), entry->load_requests.get(),
                                  item, executor, completion_executor, state, make_activity());
      if (status) {
        return request;
      }
    }

    return request;
  }

  /**
   * @brief 按 manifest type 异步预加载跨类型依赖闭包。
   *
   * 该重载要求 database 设置 manifest，并把每个资源项按 manifest type 分派到对应 cache/loader。
   */
  [[nodiscard]] preload_request request_preload(resource_guid root, tasks::executor executor,
                                                tasks::executor completion_executor) {
    preload_plan plan = plan_preload(root);
    preload_request request = preload_request::from_plan(plan, resource_);
    auto state = detail::preload_request_access::state(request);
    if (!detail::preload_request_access::loading(state)) {
      return request;
    }

    for (const preload_item& item : plan.load_order) {
      type_entry* entry = find_entry(item.type);
      if (entry == nullptr || entry->cache == nullptr || entry->request_loader() == nullptr ||
          entry->ops.request_load == nullptr) {
        detail::preload_request_access::fail_item(state, item,
                                                  {error_domain::resources, errc::not_found});
        return request;
      }

      const error status =
          entry->ops.request_load(entry->cache, entry->request_loader(), entry->load_requests.get(),
                                  item, executor, completion_executor, state, make_activity());
      if (status) {
        return request;
      }
    }

    return request;
  }

  /** @brief 用已注册 cache 解析资源引用。未命中时保留 GUID 并清除已解析指针。 */
  template <typename T> [[nodiscard]] bool resolve(resources::ref<T>& reference) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    const type_entry* entry = find_entry<T>();
    if (entry != nullptr && entry->cache != nullptr && entry->ops.resolve != nullptr) {
      return entry->ops.resolve(entry->cache, &reference);
    }

    T* value = dynamic_cast<T*>(find(reference.guid()));
    if (value == nullptr) {
      reference.unresolve();
      return false;
    }

    return reference.resolve(value);
  }

  /** @brief 判断已解析引用是否仍指向对应 cache 当前绑定版本。 */
  template <typename T> [[nodiscard]] bool is_current(const resources::ref<T>& reference) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    const type_entry* entry = find_entry<T>();
    return entry != nullptr && entry->cache != nullptr && entry->ops.is_current != nullptr &&
           entry->ops.is_current(entry->cache, &reference);
  }

  /** @brief 把依赖收集结果追加写入当前依赖图。未设置依赖图时返回 0。 */
  [[nodiscard]] std::size_t
  add_dependencies(resource_guid resource,
                   const resources::resource_dependency_list& dependencies) {
    return dependency_graph_ != nullptr
               ? resources::add_resource_dependencies(*dependency_graph_, resource, dependencies)
               : 0;
  }

  /** @brief 用依赖收集结果替换当前依赖图中某个资源的直接依赖。未设置依赖图时返回 0。 */
  [[nodiscard]] std::size_t
  replace_dependencies(resource_guid resource,
                       const resources::resource_dependency_list& dependencies) {
    return dependency_graph_ != nullptr ? resources::replace_resource_dependencies(
                                              *dependency_graph_, resource, dependencies)
                                        : 0;
  }

  /** @brief 查询某个资源变化后直接受影响的资源。未设置依赖图时返回空列表。 */
  [[nodiscard]] dependency_graph::guid_list affected_by(resource_guid resource) const {
    return dependency_graph_ != nullptr ? dependency_graph_->affected_by(resource)
                                        : make_guid_list();
  }

  /** @brief 查询某个资源变化后递归受影响的资源。未设置依赖图时返回空列表。 */
  [[nodiscard]] dependency_graph::guid_list affected_closure(resource_guid resource) const {
    return dependency_graph_ != nullptr ? dependency_graph_->affected_closure(resource)
                                        : make_guid_list();
  }

private:
  struct type_entry {
    type_id id{};
    core::string_name resource_type{};
    void* cache = nullptr;
    void* loader = nullptr;
    void* async_loader = nullptr;
    void* request_loader() const noexcept { return async_loader ? async_loader : loader; }
    std::shared_ptr<void> load_requests{};
    std::shared_ptr<void> reload_requests{};
    detail::type_entry_ops ops{};
  };

  using entry_map = core::hash_map<std::uint64_t, type_entry>;
  using resource_type_map = core::hash_map<core::string_name, std::uint64_t>;
  struct type_guid_hash {
    [[nodiscard]] std::size_t operator()(type_guid guid) const noexcept {
      return core::guid_hash{}(guid.value);
    }
  };
  using type_guid_map = core::hash_map<type_guid, std::uint64_t, type_guid_hash>;

  template <typename T> [[nodiscard]] error ensure_entry(type_entry*& entry) {
    const type_id id = meta::id_of<T>();
    if (!id) {
      return {error_domain::resources, errc::missing_type};
    }

    const auto position = entries_.find(id.value());
    if (position != entries_.end()) {
      entry = &position->second;
      return {};
    }

    type_entry created{};
    created.id = id;
    created.load_requests = std::allocate_shared<resources::load_request_table<T>>(
        allocator<resources::load_request_table<T>>{resource_}, resource_);
    created.reload_requests = std::allocate_shared<resources::load_request_table<T>>(
        allocator<resources::load_request_table<T>>{resource_}, resource_);
    const auto [inserted, _] = entries_.emplace(id.value(), created);
    entry = &inserted->second;
    return {};
  }

  template <typename T> [[nodiscard]] error bind_resource_type(core::string_name resource_type) {
    if (!resource_type) {
      return {error_domain::resources, errc::invalid_argument};
    }

    type_entry* entry = nullptr;
    const error status = ensure_entry<T>(entry);
    if (status) {
      return status;
    }

    return bind_entry_resource_type(*entry, resource_type);
  }

  [[nodiscard]] error bind_entry_resource_type(type_entry& entry, core::string_name resource_type) {
    if (!resource_type || !entry.id) {
      return {error_domain::resources, errc::invalid_argument};
    }

    if (entry.resource_type && entry.resource_type != resource_type) {
      return {error_domain::resources, errc::already_exists};
    }

    const auto position = entries_by_resource_type_.find(resource_type);
    if (position != entries_by_resource_type_.end() && position->second != entry.id.value()) {
      return {error_domain::resources, errc::already_exists};
    }

    entry.resource_type = resource_type;
    entries_by_resource_type_[resource_type] = entry.id.value();
    return {};
  }

  [[nodiscard]] error bind_entry_type_guid(type_entry& entry, type_guid guid) {
    if (!guid || !entry.id) {
      return {error_domain::resources, errc::invalid_argument};
    }

    const auto position = entries_by_type_guid_.find(guid);
    if (position != entries_by_type_guid_.end() && position->second != entry.id.value()) {
      return {error_domain::resources, errc::already_exists};
    }

    entries_by_type_guid_[guid] = entry.id.value();
    return {};
  }

  template <typename T> [[nodiscard]] type_entry* find_entry() {
    const type_id id = meta::id_of<T>();
    if (!id) {
      return nullptr;
    }

    const auto position = entries_.find(id.value());
    return position != entries_.end() ? &position->second : nullptr;
  }

  template <typename T> [[nodiscard]] const type_entry* find_entry() const {
    const type_id id = meta::id_of<T>();
    if (!id) {
      return nullptr;
    }

    const auto position = entries_.find(id.value());
    return position != entries_.end() ? &position->second : nullptr;
  }

  [[nodiscard]] type_entry* find_entry(core::string_name resource_type) {
    if (!resource_type) {
      return nullptr;
    }

    const auto type_position = entries_by_resource_type_.find(resource_type);
    if (type_position == entries_by_resource_type_.end()) {
      const std::optional<core::guid> parsed = core::guid::parse(resource_type.view());
      return parsed ? find_entry(type_guid{*parsed}) : nullptr;
    }

    const auto position = entries_.find(type_position->second);
    return position != entries_.end() ? &position->second : nullptr;
  }

  [[nodiscard]] const type_entry* find_entry(core::string_name resource_type) const {
    if (!resource_type) {
      return nullptr;
    }

    const auto type_position = entries_by_resource_type_.find(resource_type);
    if (type_position == entries_by_resource_type_.end()) {
      const std::optional<core::guid> parsed = core::guid::parse(resource_type.view());
      return parsed ? find_entry(type_guid{*parsed}) : nullptr;
    }

    const auto position = entries_.find(type_position->second);
    return position != entries_.end() ? &position->second : nullptr;
  }

  [[nodiscard]] type_entry* find_entry(type_guid guid) {
    if (!guid) {
      return nullptr;
    }

    const auto type_position = entries_by_type_guid_.find(guid);
    if (type_position == entries_by_type_guid_.end()) {
      return nullptr;
    }

    const auto position = entries_.find(type_position->second);
    return position != entries_.end() ? &position->second : nullptr;
  }

  [[nodiscard]] const type_entry* find_entry(type_guid guid) const {
    if (!guid) {
      return nullptr;
    }

    const auto type_position = entries_by_type_guid_.find(guid);
    if (type_position == entries_by_type_guid_.end()) {
      return nullptr;
    }

    const auto position = entries_.find(type_position->second);
    return position != entries_.end() ? &position->second : nullptr;
  }

  template <typename T>
  [[nodiscard]] static resources::resource* find_in_cache(void* cache, resource_guid guid) {
    return static_cast<resources::cache<T>*>(cache)->find(guid);
  }

  template <typename T>
  [[nodiscard]] static error load_in_cache(void* cache, void* loader, resource_guid guid) {
    handle_result<T> loaded =
        resources::load_handle_into(*static_cast<resources::cache<T>*>(cache),
                                    *static_cast<resources::loader<T>*>(loader), guid);
    return loaded.status;
  }

  template <typename T>
  [[nodiscard]] static error
  request_load_in_cache(void* cache, void* loader, void* requests, preload_item item,
                        const tasks::executor& executor, const tasks::executor& completion_executor,
                        const std::shared_ptr<detail::preload_request_state>& state,
                        std::shared_ptr<void> activity) {
    auto& target = *static_cast<resources::cache<T>*>(cache);
    auto& source = *static_cast<resources::loader<T>*>(loader);
    if (!item.guid) {
      const error status{error_domain::resources, errc::invalid_argument};
      detail::preload_request_access::fail_item(state, item, status);
      return status;
    }

    if (target.find(item.guid) != nullptr) {
      detail::preload_request_access::complete_item(state, item);
      return {};
    }

    auto* request_table = static_cast<resources::load_request_table<T>*>(requests);
    resources::load_request<T> request =
        request_table != nullptr
            ? detail::request_load_into_impl(target, source, item.guid, executor,
                                             completion_executor, request_table, false, state, item,
                                             activity)
            : detail::request_load_into_impl<T>(target, source, item.guid, executor,
                                                completion_executor, nullptr, false, state, item,
                                                activity);
    if (!request) {
      const error status{error_domain::resources, errc::backend_failure};
      detail::preload_request_access::fail_item(state, item, status);
      return status;
    }

    return request.failed() ? request.error() : error{};
  }

  template <typename T> [[nodiscard]] static bool resolve_in_cache(void* cache, void* reference) {
    return static_cast<resources::cache<T>*>(cache)->resolve(
        *static_cast<resources::ref<T>*>(reference));
  }

  template <typename T>
  [[nodiscard]] static bool is_current_in_cache(void* cache, const void* reference) {
    return static_cast<resources::cache<T>*>(cache)->is_current(
        *static_cast<const resources::ref<T>*>(reference));
  }

  template <typename T>
  void append_preload_guid(resource_guid guid, dependency_graph::guid_set& visited,
                           preload_plan& plan) const {
    if (!guid) {
      plan.status = {error_domain::resources, errc::invalid_argument};
      return;
    }

    const auto [_, inserted] = visited.insert(guid);
    if (!inserted) {
      return;
    }

    if (dependency_graph_ != nullptr) {
      for (resource_guid dependency : dependency_graph_->dependencies(guid)) {
        append_preload_guid<T>(dependency, visited, plan);
        if (plan.status.failed()) {
          return;
        }
      }
    }

    if (manifest_ != nullptr && !manifest_->contains(guid)) {
      plan.missing_manifest.push_back(guid);
      return;
    }

    const type_entry* entry = find_entry<T>();
    const core::string_name resource_type =
        entry != nullptr ? entry->resource_type : core::string_name{};
    if (find<T>(guid) != nullptr) {
      plan.already_loaded.push_back(preload_item{guid, resource_type});
      return;
    }

    plan.load_order.push_back(preload_item{guid, resource_type});
  }

  void append_dispatched_preload_guid(resource_guid guid, dependency_graph::guid_set& visited,
                                      preload_plan& plan) const {
    if (!guid) {
      plan.status = {error_domain::resources, errc::invalid_argument};
      return;
    }

    const auto [_, inserted] = visited.insert(guid);
    if (!inserted) {
      return;
    }

    if (dependency_graph_ != nullptr) {
      for (resource_guid dependency : dependency_graph_->dependencies(guid)) {
        append_dispatched_preload_guid(dependency, visited, plan);
        if (plan.status.failed()) {
          return;
        }
      }
    }

    const manifest_entry* entry_data = manifest_->find(guid);
    if (entry_data == nullptr) {
      plan.missing_manifest.push_back(guid);
      return;
    }

    if (!entry_data->type) {
      plan.missing_type.push_back(guid);
      return;
    }

    const preload_item item{guid, entry_data->type};
    const type_entry* entry = find_entry(entry_data->type);
    if (entry == nullptr || entry->cache == nullptr || entry->ops.find == nullptr) {
      plan.missing_cache.push_back(item);
    } else if (entry->ops.find(entry->cache, guid) != nullptr) {
      plan.already_loaded.push_back(item);
      return;
    }

    if (entry == nullptr || entry->request_loader() == nullptr || entry->ops.load == nullptr) {
      plan.missing_loader.push_back(item);
    }

    if (entry != nullptr && entry->cache != nullptr && entry->ops.find != nullptr &&
        entry->request_loader() != nullptr && entry->ops.load != nullptr) {
      plan.load_order.push_back(item);
    }
  }

  static void finalize_preload_status(preload_plan& plan) {
    if (!plan.cycle.empty()) {
      plan.status = {error_domain::resources, errc::cycle_detected};
      return;
    }

    if (!plan.missing_manifest.empty() || !plan.missing_cache.empty() ||
        !plan.missing_loader.empty()) {
      plan.status = {error_domain::resources, errc::not_found};
      return;
    }

    if (!plan.missing_type.empty()) {
      plan.status = {error_domain::resources, errc::missing_type};
    }
  }

  [[nodiscard]] dependency_graph::guid_list make_guid_list() const {
    return dependency_graph::guid_list{
        typename dependency_graph::guid_list::allocator_type{resource_}};
  }

  [[nodiscard]] std::shared_ptr<void> make_activity() {
    return std::allocate_shared<detail::resource_activity_lease>(
        allocator<detail::resource_activity_lease>{resource_}, activity_);
  }
  memory_resource* resource_ = nullptr;
  std::shared_ptr<detail::resource_activity> activity_;
  std::uint64_t generation_ = 0;
  const resources::manifest* manifest_ = nullptr;
  resources::provider* provider_ = nullptr;
  resources::dependency_graph* dependency_graph_ = nullptr;
  entry_map entries_;
  resource_type_map entries_by_resource_type_;
  type_guid_map entries_by_type_guid_;
};

} // namespace caors::resources

namespace caors {

using resource_database = resources::database;

} // namespace caors

#endif
