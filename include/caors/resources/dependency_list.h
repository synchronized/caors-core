// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_DEPENDENCY_LIST_H_
#define CAORS_RESOURCES_DEPENDENCY_LIST_H_

#include "caors/core/containers.h"
#include "caors/meta/type_id.h"
#include "caors/resources/dependency_graph.h"
#include "caors/resources/id.h"

namespace caors::resources {

/** @brief 序列化或资源扫描过程中发现的单个资源依赖。 */
struct resource_dependency {
  resources::resource_guid guid{};
  type_id type{};
};

/**
 * @brief 保存一次遍历收集到的资源依赖事实。
 *
 * 该类型不依赖序列化模块，只保存 `resource_guid` 和可选类型 id。序列化模块会提供 adapter
 * 把它挂接到 `serialization::context` 的外部引用收集器中。
 */
class resource_dependency_list {
public:
  explicit resource_dependency_list(memory::memory_resource* resource = nullptr)
      : dependencies_(allocator<resource_dependency>{resource}) {}

  void clear() noexcept { dependencies_.clear(); }
  [[nodiscard]] bool empty() const noexcept { return dependencies_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return dependencies_.size(); }
  [[nodiscard]] const vector<resource_dependency>& entries() const noexcept {
    return dependencies_;
  }

  void collect(resources::resource_guid guid, type_id type) {
    if (!guid) {
      return;
    }

    dependencies_.push_back(resource_dependency{guid, type});
  }

private:
  vector<resource_dependency> dependencies_;
};

/**
 * @brief 把依赖收集结果追加写入资源依赖图。
 *
 * `resource` 是拥有这些依赖的资源 GUID。依赖条目中的 `type` 只用于上层资源数据库或调试诊断；
 * 资源依赖图当前只保存 GUID 关系。返回值是新增的依赖边数量。
 */
[[nodiscard]] inline std::size_t
add_resource_dependencies(resources::dependency_graph& graph, resources::resource_guid resource,
                          const resource_dependency_list& dependencies) {
  if (!resource) {
    return 0;
  }

  std::size_t added = 0;
  for (const resource_dependency& dependency : dependencies.entries()) {
    const bool exists = graph.contains(resource, dependency.guid);
    if (graph.add(resource, dependency.guid) && !exists) {
      ++added;
    }
  }

  return added;
}

/**
 * @brief 用依赖收集结果替换资源依赖图中某个资源的直接依赖。
 *
 * 该函数适合资源重新导入、重新序列化或热重载后刷新资源数据库中的依赖关系。返回值是替换后新增的
 * 直接依赖边数量。
 */
[[nodiscard]] inline std::size_t
replace_resource_dependencies(resources::dependency_graph& graph, resources::resource_guid resource,
                              const resource_dependency_list& dependencies) {
  if (!resource) {
    return 0;
  }

  graph.clear_dependencies(resource);
  return add_resource_dependencies(graph, resource, dependencies);
}

} // namespace caors::resources

#endif
