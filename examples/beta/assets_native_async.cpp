// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets.h"
#include "caors/tasks.h"

#include <iostream>
#include <optional>

namespace {
class texture final : public caors::resources::resource {
public:
  using caors::resources::resource::resource;
  int width = 0;
};

// 模拟宿主 I/O：start 不等待，宿主稍后从自己的事件回调调用 finish。
class host_loader final : public caors::resources::loader<texture> {
public:
  caors::error start(caors::resources::async_load_context context,
                     caors::resources::load_completion<texture> completion) override {
    if (pending_) {
      return {caors::error_domain::resources, caors::errc::already_exists};
    }
    pending_.emplace(operation{std::move(context), std::move(completion)});
    return {};
  }
  void finish() {
    auto operation = std::move(*pending_);
    pending_.reset();
    if (operation.context.stop.stop_requested()) {
      (void)operation.completion.complete(
          {{caors::error_domain::resources, caors::errc::cancelled}, {}});
      return;
    }
    auto value =
        caors::memory::make_unique<texture>(operation.context.resource, operation.context.guid);
    value->width = 256;
    operation.context.progress.report(1.0, "宿主读取完成");
    (void)operation.completion.complete({{}, std::move(value)});
  }

private:
  struct operation {
    caors::resources::async_load_context context;
    caors::resources::load_completion<texture> completion;
  };
  std::optional<operation> pending_;
};
} // namespace

int main() {
  // 演示使用主动 pump；实际宿主也可通过 external_executor 接入工作队列。
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completions;
  caors::resources::cache<texture> cache;
  host_loader loader;
  caors::assets::asset_manager assets{{worker.get_executor(), completions.get_executor()}};
  if (auto status =
          assets.register_type(cache, loader, caors::core::string_name{"example.texture"})) {
    return 1;
  }
  const caors::resources::resource_guid guid{
      caors::core::guid::from_bytes(caors::core::guid::byte_array{1})};
  auto group = assets.create_group(worker.get_executor(), completions.get_executor());
  auto request = group.request_load<texture>(guid);
  auto subscriber = assets.request_load<texture>(guid);
  if (worker.drain() != 1 || request.done()) {
    return 2;
  }
  // 此时没有 worker 阻塞等待。这里只模拟一次事件，不代表实际 libuv 读文件。
  loader.finish();
  if (request.done() || completions.drain() != 1) {
    return 3;
  }
  auto result = request.result();
  if (result.failed() || !result.value || !subscriber.loaded()) {
    return 4;
  }
  std::cout << "宿主异步加载并在所属线程提交，width=" << result.value->width << '\n';
  return 0;
}
