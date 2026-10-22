// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_TYPE_FACTORY_H_
#define CAORS_ASSETS_TYPE_FACTORY_H_

#include "caors/assets/project_builder.h"

#include <type_traits>
#include <utility>

namespace caors::assets {
namespace detail {

template <typename T, typename Loader>
[[nodiscard]] error make_cached_type(project_builder::type_factory_context& context,
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

  auto* cache = context.project().create_owned<resources::cache<T>>(context.resource);
  if (cache == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* loader = context.project().create_owned<Loader>(context.provider);
  if (loader == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.database.register_type(*cache, *loader, config.type);
}

} // namespace detail

/**
 * @brief 注册通用 cache/loader 资源类型工厂。
 *
 * 该 helper 通过 `project_builder` 创建由 manager 保活的对象，构造 `resources::cache<T>` 和指定
 * `Loader`，再注册到内部 `resources::database`。`cache_strategy` / `loader_strategy`
 * 用于匹配 `project_config.resource_types[]` 中的策略名，例如 `"memory"` / `"json"`。
 */
template <typename T, typename Loader>
[[nodiscard]] error
register_cached_type_factory(project_builder project, core::string_name resource_type,
                             core::string_name cache_strategy, core::string_name loader_strategy) {
  project_builder::type_factory ops{&detail::make_cached_type<T, Loader>};
  ops.cache_strategy = cache_strategy;
  ops.loader_strategy = loader_strategy;
  return project.register_type_factory(resource_type, std::move(ops));
}

} // namespace caors::assets

#endif
