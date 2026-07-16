// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_PRELOAD_H_
#define CAORS_RESOURCES_PRELOAD_H_

#include "caors/core/string_name.h"
#include "caors/error.h"
#include "caors/resources/dependency_graph.h"

namespace caors::resources {

/** @brief 预加载计划中的一个资源项。 */
struct preload_item {
  resource_guid guid{};
  core::string_name type{};

  friend constexpr bool operator==(preload_item lhs, preload_item rhs) noexcept {
    return lhs.guid == rhs.guid && lhs.type == rhs.type;
  }

  friend constexpr bool operator!=(preload_item lhs, preload_item rhs) noexcept {
    return !(lhs == rhs);
  }
};

/**
 * @brief 同步资源预加载计划。
 *
 * `load_order` 按“依赖先于依赖者”的顺序排列，适合后续同步加载；已经在 cache 中的资源会进入
 * `already_loaded`，不会重复加载。跨类型预加载时每个资源项都会携带 manifest type。
 */
struct preload_plan {
  using guid_list = dependency_graph::guid_list;
  using item_list = core::vector<preload_item>;

  explicit preload_plan(memory_resource* memory = nullptr)
      : resource(memory != nullptr ? memory : get_default_memory_resource()),
        load_order(typename item_list::allocator_type{resource}),
        already_loaded(typename item_list::allocator_type{resource}),
        missing_manifest(typename guid_list::allocator_type{resource}),
        missing_type(typename guid_list::allocator_type{resource}),
        missing_cache(typename item_list::allocator_type{resource}),
        missing_loader(typename item_list::allocator_type{resource}),
        cycle(typename guid_list::allocator_type{resource}) {}

  /** @brief 计划内部容器使用的内存资源。 */
  memory_resource* resource = nullptr;
  /** @brief 需要加载的资源项，依赖资源排在使用者之前。 */
  item_list load_order;
  /** @brief 计划阶段发现已经位于 cache 中的资源项。 */
  item_list already_loaded;
  /** @brief 当前 manifest 缺失的资源 GUID。未设置 manifest 时不会填充。 */
  guid_list missing_manifest;
  /** @brief 当前 manifest 条目缺失 type 的资源 GUID。 */
  guid_list missing_type;
  /** @brief 已有 type 但缺少已注册 cache 的资源项。 */
  item_list missing_cache;
  /** @brief 已有 type 但缺少已注册 loader 的资源项。 */
  item_list missing_loader;
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
  using item_list = preload_plan::item_list;

  explicit preload_result(memory_resource* memory = nullptr)
      : resource(memory != nullptr ? memory : get_default_memory_resource()), plan(resource),
        loaded(typename item_list::allocator_type{resource}) {}

  /** @brief 结果内部容器使用的内存资源。 */
  memory_resource* resource = nullptr;
  /** @brief 本次预加载计划。 */
  preload_plan plan;
  /** @brief 本次实际同步加载成功的资源项。 */
  item_list loaded;
  /** @brief 预加载执行状态。成功时为空错误。 */
  error status{};

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
};

} // namespace caors::resources

#endif
