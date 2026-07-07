// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_DEPENDENCY_GRAPH_H_
#define CAORS_RESOURCES_DEPENDENCY_GRAPH_H_

#include "caors/core/containers.h"
#include "caors/resources/id.h"

#include <cstddef>

namespace caors::resources {

/**
 * @brief 资源依赖图。
 *
 * `dependency_graph` 只记录资源 GUID 之间的依赖和反向依赖关系。它不加载资源、不拥有资源对象，
 * 也不决定热重载策略；调用方可以用它做预加载、打包、资源数据库查询和 reload 影响范围分析。
 */
class dependency_graph final {
public:
  using guid_set = core::hash_set<resource_guid, resource_guid_hash>;
  using guid_list = core::vector<resource_guid>;

  explicit dependency_graph(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        dependencies_by_resource_(typename guid_map::allocator_type{resource_}),
        dependents_by_resource_(typename guid_map::allocator_type{resource_}) {}

  dependency_graph(const dependency_graph&) = delete;
  dependency_graph& operator=(const dependency_graph&) = delete;
  dependency_graph(dependency_graph&&) = delete;
  dependency_graph& operator=(dependency_graph&&) = delete;

  /** @brief 查询内部容器使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询是否没有任何依赖边。 */
  [[nodiscard]] bool empty() const noexcept { return edge_count_ == 0; }

  /** @brief 查询依赖边数量。 */
  [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }

  /** @brief 判断 resource 是否依赖 dependency。 */
  [[nodiscard]] bool contains(resource_guid resource, resource_guid dependency) const {
    const auto position = dependencies_by_resource_.find(resource);
    if (position == dependencies_by_resource_.end()) {
      return false;
    }

    return position->second.find(dependency) != position->second.end();
  }

  /**
   * @brief 增加一条依赖边。
   *
   * `resource` 表示拥有依赖的资源，`dependency` 表示它需要的另一个资源。nil GUID 和自依赖会失败。
   */
  bool add(resource_guid resource, resource_guid dependency) {
    if (!resource || !dependency || resource == dependency) {
      return false;
    }

    guid_set& dependencies = ensure_set(dependencies_by_resource_, resource);
    const auto [_, inserted] = dependencies.insert(dependency);
    if (!inserted) {
      return true;
    }

    guid_set& dependents = ensure_set(dependents_by_resource_, dependency);
    dependents.insert(resource);
    ++edge_count_;
    return true;
  }

  /** @brief 移除一条依赖边。不存在时返回 false。 */
  bool remove(resource_guid resource, resource_guid dependency) {
    const auto dependencies_position = dependencies_by_resource_.find(resource);
    if (dependencies_position == dependencies_by_resource_.end()) {
      return false;
    }

    if (dependencies_position->second.erase(dependency) == 0) {
      return false;
    }

    erase_empty_set(dependencies_by_resource_, dependencies_position);

    const auto dependents_position = dependents_by_resource_.find(dependency);
    if (dependents_position != dependents_by_resource_.end()) {
      dependents_position->second.erase(resource);
      erase_empty_set(dependents_by_resource_, dependents_position);
    }

    --edge_count_;
    return true;
  }

  /** @brief 清除某个资源的全部出边，并从其它资源的反向依赖中移除它。 */
  void clear_dependencies(resource_guid resource) {
    const auto dependencies_position = dependencies_by_resource_.find(resource);
    if (dependencies_position == dependencies_by_resource_.end()) {
      return;
    }

    guid_list dependencies = copy_set(dependencies_position->second);
    for (resource_guid dependency : dependencies) {
      (void)remove(resource, dependency);
    }
  }

  /** @brief 清除某个资源的全部入边，并从依赖它的资源中移除对应出边。 */
  void clear_dependents(resource_guid resource) {
    const auto dependents_position = dependents_by_resource_.find(resource);
    if (dependents_position == dependents_by_resource_.end()) {
      return;
    }

    guid_list dependents = copy_set(dependents_position->second);
    for (resource_guid dependent : dependents) {
      (void)remove(dependent, resource);
    }
  }

  /** @brief 清除某个资源关联的全部入边和出边。 */
  void clear_resource(resource_guid resource) {
    clear_dependencies(resource);
    clear_dependents(resource);
  }

  /** @brief 清空整张依赖图。 */
  void clear() noexcept {
    dependencies_by_resource_.clear();
    dependents_by_resource_.clear();
    edge_count_ = 0;
  }

  /** @brief 获取 resource 直接依赖的资源 GUID 列表。 */
  [[nodiscard]] guid_list dependencies(resource_guid resource) const {
    return lookup_copy(dependencies_by_resource_, resource);
  }

  /** @brief 获取直接依赖 resource 的资源 GUID 列表。 */
  [[nodiscard]] guid_list dependents(resource_guid resource) const {
    return lookup_copy(dependents_by_resource_, resource);
  }

  /** @brief 获取 resource 变化后可能直接受影响的资源 GUID 列表。 */
  [[nodiscard]] guid_list affected_by(resource_guid resource) const { return dependents(resource); }

private:
  using guid_map = core::hash_map<resource_guid, guid_set, resource_guid_hash>;
  using guid_map_iterator = typename guid_map::iterator;

  [[nodiscard]] guid_set make_set() const {
    return guid_set{typename guid_set::allocator_type{resource_}};
  }

  [[nodiscard]] guid_set& ensure_set(guid_map& map, resource_guid guid) {
    const auto [position, _] = map.try_emplace(guid, make_set());
    return position->second;
  }

  void erase_empty_set(guid_map& map, guid_map_iterator position) {
    if (position->second.empty()) {
      map.erase(position);
    }
  }

  [[nodiscard]] guid_list make_list() const {
    return guid_list{typename guid_list::allocator_type{resource_}};
  }

  [[nodiscard]] guid_list copy_set(const guid_set& values) const {
    guid_list result = make_list();
    result.reserve(values.size());
    for (resource_guid value : values) {
      result.push_back(value);
    }
    return result;
  }

  [[nodiscard]] guid_list lookup_copy(const guid_map& map, resource_guid guid) const {
    const auto position = map.find(guid);
    return position != map.end() ? copy_set(position->second) : make_list();
  }

  memory_resource* resource_ = nullptr;
  guid_map dependencies_by_resource_;
  guid_map dependents_by_resource_;
  std::size_t edge_count_ = 0;
};

} // namespace caors::resources

namespace caors {

using resource_dependency_graph = resources::dependency_graph;

} // namespace caors

#endif
