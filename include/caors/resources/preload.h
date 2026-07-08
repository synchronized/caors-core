// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_PRELOAD_H_
#define CAORS_RESOURCES_PRELOAD_H_

#include "caors/error.h"
#include "caors/resources/dependency_graph.h"

namespace caors::resources {

/**
 * @brief 同步资源预加载计划。
 *
 * 第一版计划只描述从一个根资源出发的依赖闭包。`load_order` 按“依赖先于依赖者”的顺序排列，
 * 适合后续同步加载；已经在 cache 中的资源会进入 `already_loaded`，不会重复加载。
 */
struct preload_plan {
  using guid_list = dependency_graph::guid_list;

  explicit preload_plan(memory_resource* memory = nullptr)
      : resource(memory != nullptr ? memory : get_default_memory_resource()),
        load_order(typename guid_list::allocator_type{resource}),
        already_loaded(typename guid_list::allocator_type{resource}),
        missing_manifest(typename guid_list::allocator_type{resource}),
        cycle(typename guid_list::allocator_type{resource}) {}

  /** @brief 计划内部容器使用的内存资源。 */
  memory_resource* resource = nullptr;
  /** @brief 需要加载的资源 GUID，依赖资源排在使用者之前。 */
  guid_list load_order;
  /** @brief 计划阶段发现已经位于 cache 中的资源 GUID。 */
  guid_list already_loaded;
  /** @brief 当前 manifest 缺失的资源 GUID。未设置 manifest 时不会填充。 */
  guid_list missing_manifest;
  /** @brief 发现的循环依赖路径；为空表示没有环。 */
  guid_list cycle;
  /** @brief 计划生成状态。成功时为空错误。 */
  error status{};

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
};

/**
 * @brief 同步资源预加载结果。
 *
 * `plan` 保存本次预加载使用的计划；`loaded` 保存本次实际成功加载进 cache 的资源 GUID。
 */
struct preload_result {
  using guid_list = dependency_graph::guid_list;

  explicit preload_result(memory_resource* memory = nullptr)
      : resource(memory != nullptr ? memory : get_default_memory_resource()), plan(resource),
        loaded(typename guid_list::allocator_type{resource}) {}

  /** @brief 结果内部容器使用的内存资源。 */
  memory_resource* resource = nullptr;
  /** @brief 本次预加载计划。 */
  preload_plan plan;
  /** @brief 本次实际同步加载成功的资源 GUID。 */
  guid_list loaded;
  /** @brief 预加载执行状态。成功时为空错误。 */
  error status{};

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
};

} // namespace caors::resources

namespace caors {

using resource_preload_plan = resources::preload_plan;
using resource_preload_result = resources::preload_result;

} // namespace caors

#endif
