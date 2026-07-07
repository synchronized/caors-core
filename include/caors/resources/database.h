// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_DATABASE_H_
#define CAORS_RESOURCES_DATABASE_H_

#include "caors/resources/async.h"
#include "caors/resources/dependency_graph.h"
#include "caors/resources/io.h"
#include "caors/serialization/types.h"

#include <cstdint>
#include <type_traits>

namespace caors::resources {

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
        entries_(typename entry_map::allocator_type{resource_}) {}

  database(const database&) = delete;
  database& operator=(const database&) = delete;
  database(database&&) = delete;
  database& operator=(database&&) = delete;

  /** @brief 返回 database 内部索引使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

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
    entry->find = &find_in_cache<T>;
    entry->resolve = &resolve_in_cache<T>;
    entry->is_current = &is_current_in_cache<T>;
    return {};
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
    return {};
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
      if (entry.cache == nullptr || entry.find == nullptr) {
        continue;
      }

      resources::resource* value = entry.find(entry.cache, guid);
      if (value != nullptr) {
        return value;
      }
    }

    return nullptr;
  }

  /** @brief 使用已注册 loader 把资源加载进对应 cache，并返回强 handle。 */
  template <typename T> [[nodiscard]] handle_result<T> load_handle(resource_guid guid) {
    type_entry* entry = find_entry<T>();
    if (entry == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
    }

    if (entry->cache == nullptr || entry->loader == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
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
    type_entry* entry = find_entry<T>();
    if (entry == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
    }

    if (entry->cache == nullptr || entry->loader == nullptr) {
      return {{error_domain::resources, errc::not_found}, {}};
    }

    return resources::reload_handle_into(*static_cast<resources::cache<T>*>(entry->cache),
                                         *static_cast<resources::loader<T>*>(entry->loader), guid);
  }

  /** @brief 使用已注册 loader 重新加载并替换对应 cache 中已有资源。 */
  template <typename T> [[nodiscard]] result<T*> reload(resource_guid guid) {
    handle_result<T> reloaded = reload_handle<T>(guid);
    return {reloaded.status, reloaded.value.get()};
  }

  /** @brief 异步加载资源，并在 completion queue 中提交到对应 cache。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_load(resource_guid guid, tasks::executor& executor,
                                             tasks::completion_queue& completions) {
    type_entry* entry = find_entry<T>();
    if (entry == nullptr || entry->cache == nullptr || entry->loader == nullptr) {
      return load_request<T>::failed({error_domain::resources, errc::not_found}, resource_);
    }

    return resources::request_load_into(*static_cast<resources::cache<T>*>(entry->cache),
                                        *static_cast<resources::loader<T>*>(entry->loader), guid,
                                        executor, completions);
  }

  /** @brief 异步重载资源，并在 completion queue 中替换对应 cache 绑定。 */
  template <typename T>
  [[nodiscard]] load_request<T> request_reload(resource_guid guid, tasks::executor& executor,
                                               tasks::completion_queue& completions) {
    type_entry* entry = find_entry<T>();
    if (entry == nullptr || entry->cache == nullptr || entry->loader == nullptr) {
      return load_request<T>::failed({error_domain::resources, errc::not_found}, resource_);
    }

    return resources::request_reload_into(*static_cast<resources::cache<T>*>(entry->cache),
                                          *static_cast<resources::loader<T>*>(entry->loader), guid,
                                          executor, completions);
  }

  /** @brief 用已注册 cache 解析资源引用。未命中时保留 GUID 并清除已解析指针。 */
  template <typename T> [[nodiscard]] bool resolve(resources::ref<T>& reference) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");

    const type_entry* entry = find_entry<T>();
    if (entry != nullptr && entry->cache != nullptr && entry->resolve != nullptr) {
      return entry->resolve(entry->cache, &reference);
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
    return entry != nullptr && entry->cache != nullptr && entry->is_current != nullptr &&
           entry->is_current(entry->cache, &reference);
  }

  /** @brief 把依赖收集结果追加写入当前依赖图。未设置依赖图时返回 0。 */
  [[nodiscard]] std::size_t
  add_dependencies(resource_guid resource,
                   const serialization::resource_dependency_list& dependencies) {
    return dependency_graph_ != nullptr ? serialization::add_resource_dependencies(
                                              *dependency_graph_, resource, dependencies)
                                        : 0;
  }

  /** @brief 用依赖收集结果替换当前依赖图中某个资源的直接依赖。未设置依赖图时返回 0。 */
  [[nodiscard]] std::size_t
  replace_dependencies(resource_guid resource,
                       const serialization::resource_dependency_list& dependencies) {
    return dependency_graph_ != nullptr ? serialization::replace_resource_dependencies(
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

  /** @brief 创建可挂到 `serialization::context` 的资源引用 resolver。 */
  [[nodiscard]] serialization::resource_resolver resolver() noexcept {
    return serialization::resource_resolver{&database_resource_resolve, this};
  }

private:
  using find_fn = resources::resource* (*)(void* cache, resource_guid guid);
  using resolve_fn = bool (*)(void* cache, void* reference);
  using is_current_fn = bool (*)(void* cache, const void* reference);

  struct type_entry {
    type_id id{};
    void* cache = nullptr;
    void* loader = nullptr;
    find_fn find = nullptr;
    resolve_fn resolve = nullptr;
    is_current_fn is_current = nullptr;
  };

  using entry_map = core::hash_map<std::uint64_t, type_entry>;

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
    const auto [inserted, _] = entries_.emplace(id.value(), created);
    entry = &inserted->second;
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

  template <typename T>
  [[nodiscard]] static resources::resource* find_in_cache(void* cache, resource_guid guid) {
    return static_cast<resources::cache<T>*>(cache)->find(guid);
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

  [[nodiscard]] dependency_graph::guid_list make_guid_list() const {
    return dependency_graph::guid_list{
        typename dependency_graph::guid_list::allocator_type{resource_}};
  }

  [[nodiscard]] static resources::resource* database_resource_resolve(resource_guid guid,
                                                                      void* user_data) noexcept {
    auto* database = static_cast<resources::database*>(user_data);
    return database != nullptr ? database->find(guid) : nullptr;
  }

  memory_resource* resource_ = nullptr;
  const resources::manifest* manifest_ = nullptr;
  resources::provider* provider_ = nullptr;
  resources::dependency_graph* dependency_graph_ = nullptr;
  entry_map entries_;
};

} // namespace caors::resources

namespace caors {

using resource_database = resources::database;

} // namespace caors

#endif
