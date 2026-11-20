// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/async.h"

#include <optional>

namespace {
class host_resource final : public caors::resource {
public:
  using caors::resource::resource;
};

// 模拟宿主保存完成凭证，不要求文件系统、来源或实际网络库。
class host_loader final : public caors::resources::loader<host_resource> {
public:
  caors::error start(caors::resources::async_load_context,
                     caors::resources::load_completion<host_resource> completion) override {
    pending.emplace(std::move(completion));
    return {};
  }
  std::optional<caors::resources::load_completion<host_resource>> pending;
};

// 实例化完整请求路径，验证模板实现也没有偷偷引入文件协议。
[[maybe_unused]] auto request(caors::resources::cache<host_resource>& cache, host_loader& loader,
                              caors::resource_guid key, caors::tasks::executor worker,
                              caors::tasks::executor completion) {
  return caors::resources::request_load_into(cache, loader, key, worker, completion);
}
}
