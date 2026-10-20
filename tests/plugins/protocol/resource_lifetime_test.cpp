// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io.h"
#include "caors/plugins/plugin_binary.h"
#include "caors/plugins/plugin_host.h"
#include "caors/resources/source.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/task.h"
#include "plugin_protocol_test_extension.h"

#include <iostream>
#include <utility>

namespace {
// 成员按逆序销毁，保证弱控制块释放完成后才释放模块 pin。
struct pinned_work {
  caors::plugins::plugin_library_lease pin;
  caors::resource_weak_handle<caors::resource> weak;
};
bool verify(const char* path, bool discard) {
  static int unloaded = 0;
  unloaded = 0;
  int destroyed = 0;
  int deallocated = 0;
  caors::plugins::plugin_host host;
  auto loaded = caors::plugins::plugin_binary::load(path, &host.abi());
  if (!loaded) {
    return false;
  }
  auto plugin = std::move(*loaded);
  auto pin = plugin.library_lease();
  const auto* native = static_cast<const caors_native_resource_test_extension*>(
      plugin.get_extension("caors.test.native-resource", 1));
  if (!native) {
    return false;
  }
  native->watch_unload(&unloaded);
  auto strong = native->create(&destroyed, &deallocated);
  if (!strong) {
    return false;
  }
  caors::resource_weak_handle<caors::resource> weak = strong;
  plugin = {};
  if (unloaded != 0) {
    return false;
  }
  strong.reset();
  if (destroyed != 1 || deallocated != 1 || !weak.expired() || unloaded != 0) {
    return false;
  }
  caors::tasks::task queued{
      [work = pinned_work{std::move(pin), std::move(weak)}]() mutable { work.weak.reset(); }};
  if (!discard && queued.run()) {
    return false;
  }
  if (unloaded != 0) {
    return false;
  }
  queued.reset();
  return destroyed == 1 && deallocated == 2 && unloaded == 1;
}
bool verify_io(const char* path, bool discard) {
  static int unloaded = 0;
  unloaded = 0;
  int destroyed = 0;
  caors::plugins::plugin_host host;
  auto loaded = caors::plugins::plugin_binary::load(path, &host.abi());
  if (!loaded)
    return false;
  auto plugin = std::move(*loaded);
  const auto* native = static_cast<const caors_native_resource_test_extension*>(
      plugin.get_extension("caors.test.native-resource", 1));
  if (!native || !native->create_io)
    return false;
  native->watch_unload(&unloaded);
  caors::tasks::execution_context notifications;
  caors::io::io_request<std::shared_ptr<caors::io::async_file>> request;
  {
    caors::io::mount_file_system router;
    auto pin = std::make_shared<caors::plugins::plugin_library_lease>(plugin.library_lease());
    if (router.mount("assets", native->create_io(&destroyed), {}, std::move(pin)))
      return false;
    caors::resources::file_source source{router};
    caors::resources::provider provider;
    const caors::resource_guid first{caors::guid::from_bytes(caors::guid::byte_array{51})};
    const caors::resource_guid second{caors::guid::from_bytes(caors::guid::byte_array{52})};
    if (provider.bind(first, source, "assets/data") || provider.bind(second, source, "assets/data"))
      return false;
    auto snapshot = provider.snapshot();
    auto resolved = snapshot->resolve(first);
    auto other = snapshot->resolve(second);
    if (!other || !resolved || other->source_id != resolved->source_id)
      return false;
    if (!resolved)
      return false;
    auto opening = caors::io::request_open(std::move(resolved.value), notifications.get_executor());
    if (!opening)
      return false;
    request = std::move(opening.value);
    router.clear();
  }
  plugin = {};
  if (unloaded || destroyed)
    return false;
  if (discard) {
    notifications.clear();
    auto result = request.result();
    if (!result || result->error().value != caors::io::io_errc::completion_rejected)
      return false;
  } else {
    if (notifications.drain() != 1)
      return false;
    auto result = request.result();
    if (!result || !*result)
      return false;
    auto file = result->value;
    result.reset();
    request = {};
    if (unloaded || destroyed)
      return false;
    auto read = caors::io::request_read(file, 1, 2, 2, caors::tasks::inline_executor());
    if (!read || !read->result() || !*read->result() || read->result()->value.size() != 2)
      return false;
    std::uint64_t consumed = 0;
    auto stream = caors::io::consume_chunks(file, {file->size(), 1},
                                            caors::tasks::inline_executor(), [&](auto, auto block) {
                                              consumed += block.size();
                                              return caors::io::io_error{};
                                            });
    if (!stream || !stream->result() || !*stream->result() || consumed != file->size())
      return false;
    auto close = caors::io::request_close(file, caors::tasks::inline_executor());
    if (!close || !close->result() || !*close->result())
      return false;
    // read/close 请求也持有文件，离开作用域后才释放最后后端和模块凭据。
  }
  request = {};
  return destroyed == 1 && unloaded == 1;
}

}

int main(int argc, char** argv) {
  if (argc != 2 || !verify(argv[1], false) || !verify(argv[1], true) ||
      !verify_io(argv[1], false) || !verify_io(argv[1], true)) {
    std::cerr << "模块 pin、弱控制块、任务丢弃或真实卸载顺序错误\n";
    return 1;
  }
  return 0;
}
