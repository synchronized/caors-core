// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_binary.h"
#include "caors/plugins/plugin_host.h"
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
}

int main(int argc, char** argv) {
  if (argc != 2 || !verify(argv[1], false) || !verify(argv[1], true)) {
    std::cerr << "模块 pin、弱控制块、任务丢弃或真实卸载顺序错误\n";
    return 1;
  }
  return 0;
}
