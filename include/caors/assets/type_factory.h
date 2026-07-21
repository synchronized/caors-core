// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_TYPE_FACTORY_H_
#define CAORS_ASSETS_TYPE_FACTORY_H_

#include "caors/assets/asset_manager.h"

#include <type_traits>
#include <utility>

namespace caors::assets {
namespace detail {

template <typename T, typename Loader>
[[nodiscard]] error make_cached_type(asset_manager::type_factory_context& context,
                                     const project_resource_type_config& config, void* user_data) {
  (void)user_data;
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");
  static_assert(std::is_base_of_v<resources::loader<T>, Loader>,
                "Loader must derive from caors::resources::loader<T>");
  static_assert(std::is_constructible_v<Loader, const resources::provider&>,
                "Loader must be constructible from const caors::resources::provider&");

  if (!config.type) {
    return {error_domain::resources, errc::invalid_argument};
  }

  auto* cache = context.manager.create_owned<resources::cache<T>>(context.resource);
  if (cache == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* loader = context.manager.create_owned<Loader>(context.provider);
  if (loader == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.database.register_type(*cache, *loader, config.type);
}

} // namespace detail

/**
 * @brief 注册通用 cache/loader 资源类型工厂。
 *
 * 该 helper 复用 `asset_manager` 的拥有式对象创建逻辑，构造 `resources::cache<T>` 和指定
 * `Loader`，再注册到内部 `resources::database`。`cache_strategy` / `loader_strategy`
 * 用于匹配 `project_config.resource_types[]` 中的策略名，例如 `"memory"` / `"json"`。
 */
template <typename T, typename Loader>
[[nodiscard]] error
register_cached_type_factory(asset_manager& manager, core::string_name resource_type,
                             core::string_name cache_strategy, core::string_name loader_strategy) {
  asset_manager::type_factory_ops ops{&detail::make_cached_type<T, Loader>};
  ops.cache_strategy = cache_strategy;
  ops.loader_strategy = loader_strategy;
  return manager.register_type_factory(resource_type, std::move(ops));
}

} // namespace caors::assets

#endif
