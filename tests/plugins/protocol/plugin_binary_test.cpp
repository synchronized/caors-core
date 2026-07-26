// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_binary.h"
#include "caors/plugins/plugin_manager.h"

#include "caors/meta/instance.h"
#include "caors/meta/property.h"
#include "caors/meta/registry.h"
#include "caors/meta/value/any.h"
#include "caors/plugins/plugin_host.h"
#include "plugin_protocol_test_extension.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <utility>

namespace {

[[nodiscard]] int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

void CAORS_PLUGIN_CALL capture_log(void* context, std::uint32_t, const char* message,
                                   std::size_t size) {
  static_cast<std::string*>(context)->assign(message, size);
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    return fail("插件协议测试需要四个动态库路径");
  }

  caors::plugins::plugin_host host{};
  std::string log_message{};
  host.set_log(&capture_log, &log_message);
  auto good_result = caors::plugins::plugin_binary::load(argv[1], &host.abi());
  if (!good_result) {
    return fail("无法加载有效插件");
  }
  auto plugin = std::move(*good_result);
  const std::filesystem::path plugin_path = plugin.path();
  caors::plugins::plugin_library_lease library_lease = plugin.library_lease();
  if (!plugin.valid() || plugin.state() != caors::plugins::plugin_state::loaded || !library_lease ||
      library_lease.path() != plugin_path || library_lease.use_count() < 2 ||
      plugin.descriptor().name != "caors.test.good" ||
      plugin.descriptor().display_name != "CAORS 测试插件" ||
      plugin.descriptor().plugin_version != caors::version{1, 2, 3} ||
      log_message != "插件已协商宿主扩展") {
    return fail("有效插件描述信息不正确");
  }

  if (plugin.start().value() != caors::errc::invalid_argument || plugin.initialize() ||
      plugin.state() != caors::plugins::plugin_state::initialized || plugin.start() ||
      plugin.state() != caors::plugins::plugin_state::running) {
    return fail("插件初始化或启动状态转换失败");
  }

  const auto* answer = static_cast<const int*>(plugin.get_extension("caors.test.answer", 1));
  if (answer == nullptr || *answer != 42 ||
      plugin.get_extension("caors.test.answer", 2) != nullptr) {
    return fail("插件 extension 查询失败");
  }

  if (plugin.shutdown().value() != caors::errc::invalid_argument || plugin.stop() ||
      plugin.state() != caors::plugins::plugin_state::stopped || plugin.shutdown() ||
      plugin.state() != caors::plugins::plugin_state::loaded) {
    return fail("插件停止或关闭状态转换失败");
  }

  caors::meta::registry registry{};
  auto layer = registry.create_layer();
  if (library_lease.bind(*layer)) {
    return fail("无法将 library lease 绑定到 registry layer");
  }
  plugin = {};
  if (!library_lease || library_lease.path() != plugin_path || library_lease.use_count() != 2) {
    return fail("插件二进制释放后 registry layer 没有继续保持动态库");
  }
  layer.reset();
  if (library_lease.use_count() != 1) {
    return fail("registry layer 释放后没有归还动态库租约");
  }

  const auto bad_abi = caors::plugins::plugin_binary::load(argv[2]);
  if (bad_abi || bad_abi.error() !=
                     caors::error{caors::error_domain::plugins, caors::errc::inconsistent_type}) {
    return fail("不兼容插件 ABI 没有被拒绝");
  }

  const auto missing_entry = caors::plugins::plugin_binary::load(argv[3]);
  if (missing_entry ||
      missing_entry.error() != caors::error{caors::error_domain::plugins, caors::errc::not_found}) {
    return fail("缺少入口的动态库没有被拒绝");
  }

  auto failure_result = caors::plugins::plugin_binary::load(argv[4]);
  if (!failure_result) {
    return fail("无法加载生命周期失败插件");
  }
  auto failure = std::move(*failure_result);
  if (!failure.initialize() || failure.state() != caors::plugins::plugin_state::failed ||
      failure.shutdown() || failure.state() != caors::plugins::plugin_state::loaded) {
    return fail("插件生命周期失败没有进入 failed 状态");
  }

  caors::plugins::plugin_manager manager{registry};
  auto managed_result = manager.load(argv[1]);
  if (!managed_result) {
    return fail("plugin manager 无法加载有效插件");
  }
  const caors::plugins::plugin managed = *managed_result;
  const caors::guid managed_id = managed.id();
  const std::uint64_t first_generation = managed.generation();
  if (!managed || manager.size() != 1 ||
      manager.find(managed_id).generation() != first_generation ||
      manager.load(argv[1]).error() !=
          caors::error{caors::error_domain::plugins, caors::errc::duplicate_id}) {
    return fail("plugin manager 索引或重复 ID 校验错误");
  }
  if (manager.initialize(managed_id) ||
      managed.state() != caors::plugins::plugin_state::initialized) {
    return fail("plugin manager 初始化或 layer 发布失败");
  }
  auto reflected = registry.find_type("caors.test.plugin.reflected");
  auto reflected_property = reflected.find_property("value");
  auto reflected_method = reflected.find_method("block");
  const auto* reflected_capability = reflected.get_capability<caors_plugin_protocol_capability>();
  auto owned = caors::any::default_construct(reflected);
  caors::meta::instance object{owned.ref()};
  const auto* control =
      static_cast<const caors_plugin_protocol_test_extension*>(managed.get_extension(
          CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_ID, CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_VERSION));
  if (!reflected || !reflected_property || !reflected_method || reflected_capability == nullptr ||
      reflected_capability->value != 73 || !owned || !object || control == nullptr ||
      control->struct_size < sizeof(caors_plugin_protocol_test_extension) ||
      manager.start(managed_id) || managed.state() != caors::plugins::plugin_state::running) {
    return fail("插件反射类型、method 或测试控制扩展没有发布");
  }
  control->reset(control->context);
  auto invocation = std::async(std::launch::async, [reflected_method, object] {
    return reflected_method.invoke_result_of(object);
  });
  const auto enter_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
  while (control->entered(control->context) == 0 &&
         std::chrono::steady_clock::now() < enter_deadline) {
    std::this_thread::yield();
  }
  if (control->entered(control->context) == 0) {
    control->release(control->context);
    (void)invocation.get();
    return fail("插件 method 没有进入 in-flight 状态");
  }
  if (manager.unload(managed_id) !=
          caors::error{caors::error_domain::plugins, caors::errc::pending} ||
      managed.state() != caors::plugins::plugin_state::detached_pending_release ||
      registry.find_type("caors.test.plugin.reflected") || reflected || reflected_property ||
      reflected_method || owned || object) {
    control->release(control->context);
    (void)invocation.get();
    return fail("in-flight method 或 owning value 没有阻止 DLL 提前关闭");
  }
  control->release(control->context);
  if (!invocation.get()) {
    return fail("detach 前已进入的 method 没有安全完成");
  }
  object = {};
  owned.reset();
  if (control->destructions(control->context) != 1) {
    return fail("detach 后 owning value 没有在 DLL 保活期间安全析构");
  }
  reflected_property = {};
  reflected_method = {};
  reflected = {};
  if (manager.unload(managed_id)) {
    return fail("释放旧反射视图后 plugin manager 仍无法完成卸载");
  }
  if (managed || managed.state() != caors::plugins::plugin_state::unloaded || manager.size() != 0 ||
      manager.find(managed_id)) {
    return fail("plugin manager 卸载后旧句柄没有失效");
  }

  auto reloaded_result = manager.load(argv[1]);
  if (!reloaded_result || reloaded_result->generation() == first_generation ||
      manager.unload(reloaded_result->id())) {
    return fail("plugin manager 重新加载没有生成新的 generation");
  }

  return 0;
}
