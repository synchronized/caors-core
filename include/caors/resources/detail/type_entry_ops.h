// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_DETAIL_TYPE_ENTRY_OPS_H_
#define CAORS_RESOURCES_DETAIL_TYPE_ENTRY_OPS_H_

#include "caors/error.h"
#include "caors/resources/preload.h"

#include <memory>

namespace caors::tasks {

class executor;

} // namespace caors::tasks

namespace caors::resources {

class resource;
class load_snapshot;

namespace detail {

struct preload_request_state;

/** @brief database 类型绑定条目使用的内部函数表。 */
struct type_entry_ops {
  using find_fn = resources::resource* (*)(void* cache, resource_guid guid);
  using load_fn = error (*)(void* cache, void* loader, resource_guid guid);
  using request_load_fn = error (*)(void* cache, void* loader, void* requests, preload_item item,
                                    const tasks::executor& executor,
                                    const tasks::executor& completion_executor,
                                    const std::shared_ptr<preload_request_state>& state,
                                    std::shared_ptr<void> activity,
                                    std::shared_ptr<const load_snapshot> sources);
  using resolve_fn = bool (*)(void* cache, void* reference);
  using is_current_fn = bool (*)(void* cache, const void* reference);

  find_fn find = nullptr;
  load_fn load = nullptr;
  request_load_fn request_load = nullptr;
  resolve_fn resolve = nullptr;
  is_current_fn is_current = nullptr;
};

} // namespace detail

} // namespace caors::resources

#endif
