// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_binary.h"

#include <filesystem>
#include <iostream>
#include <utility>

namespace {

[[nodiscard]] int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    return fail("插件协议测试需要四个动态库路径");
  }

  auto good_result = caors::plugins::plugin_binary::load(argv[1]);
  if (!good_result) {
    return fail("无法加载有效插件");
  }
  auto plugin = std::move(*good_result);
  if (!plugin.valid() || plugin.state() != caors::plugins::plugin_state::loaded ||
      plugin.descriptor().name != "caors.test.good" ||
      plugin.descriptor().display_name != "CAORS 测试插件" ||
      plugin.descriptor().plugin_version != caors::version{1, 2, 3}) {
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

  return 0;
}
