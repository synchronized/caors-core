// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "plugin_identity_test_support.h"

#include "caors/error.h"
#include "caors/meta/type_id.h"
#include "caors/platform/shared_library.h"

#include <cstdint>
#include <iostream>
#include <thread>
#include <utility>

namespace {

[[nodiscard]] int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    return fail("插件 identity 测试需要 plugin A 和 plugin B 路径");
  }

  auto plugin_a_result = caors::platform::shared_library::open(argv[1]);
  auto plugin_b_result = caors::platform::shared_library::open(argv[2]);
  if (!plugin_a_result || !plugin_b_result) {
    return fail("无法加载 identity 测试插件");
  }
  auto plugin_a = std::move(*plugin_a_result);
  auto plugin_b = std::move(*plugin_b_result);

  const auto plugin_a_id = plugin_a.find_function<caors::tests::plugin_identity::type_id_fn>(
      "caors_test_plugin_a_type_id");
  const auto plugin_b_id = plugin_b.find_function<caors::tests::plugin_identity::type_id_fn>(
      "caors_test_plugin_b_type_id");
  if (plugin_a_id == nullptr || plugin_b_id == nullptr) {
    return fail("无法查找 identity 测试插件导出函数");
  }

  std::uint64_t first_id = 0;
  std::uint64_t second_id = 0;
  std::thread first{[&] { first_id = plugin_a_id(); }};
  std::thread second{[&] { second_id = plugin_b_id(); }};
  first.join();
  second.join();

  const std::uint64_t host_id =
      caors::meta::detail::type_id_of_cpp_type<caors::tests::plugin_identity::shared_plugin_type>()
          .value();
  if (first_id == 0 || first_id != second_id || first_id != host_id) {
    return fail("host 与插件没有获得统一的 type_id");
  }

  plugin_a = caors::platform::shared_library{};
  auto reloaded_plugin_a_result = caors::platform::shared_library::open(argv[1]);
  if (!reloaded_plugin_a_result) {
    return fail("无法重新加载 identity 测试插件");
  }
  auto reloaded_plugin_a = std::move(*reloaded_plugin_a_result);
  const auto reloaded_plugin_a_id =
      reloaded_plugin_a.find_function<caors::tests::plugin_identity::type_id_fn>(
          "caors_test_plugin_a_type_id");
  if (reloaded_plugin_a_id == nullptr || reloaded_plugin_a_id() != host_id) {
    return fail("插件重新加载后没有复用原 type_id");
  }

  const auto plugin_a_register =
      reloaded_plugin_a.find_function<caors::tests::plugin_identity::register_type_fn>(
          "caors_test_plugin_a_register_type");
  const auto plugin_b_register =
      plugin_b.find_function<caors::tests::plugin_identity::register_type_fn>(
          "caors_test_plugin_b_register_type");
  if (plugin_a_register == nullptr || plugin_b_register == nullptr) {
    return fail("无法查找插件类型注册函数");
  }

  if (plugin_a_register() != static_cast<int>(caors::errc::success)) {
    return fail("plugin A 无法注册基础静态类型模型");
  }
  if (plugin_b_register() != static_cast<int>(caors::errc::inconsistent_type_model)) {
    return fail("不同插件中的冲突静态类型模型没有被诊断");
  }

  const auto plugin_a_register_consistent =
      reloaded_plugin_a.find_function<caors::tests::plugin_identity::register_type_fn>(
          "caors_test_plugin_a_register_consistent_type");
  const auto plugin_b_register_consistent =
      plugin_b.find_function<caors::tests::plugin_identity::register_type_fn>(
          "caors_test_plugin_b_register_consistent_type");
  const auto plugin_a_capability_key =
      reloaded_plugin_a.find_function<caors::tests::plugin_identity::capability_key_fn>(
          "caors_test_plugin_a_capability_key");
  const auto plugin_b_capability_key =
      plugin_b.find_function<caors::tests::plugin_identity::capability_key_fn>(
          "caors_test_plugin_b_capability_key");
  if (plugin_a_register_consistent == nullptr || plugin_b_register_consistent == nullptr ||
      plugin_a_capability_key == nullptr || plugin_b_capability_key == nullptr) {
    return fail("无法查找一致模型测试函数");
  }

  if (plugin_a_capability_key() != plugin_b_capability_key()) {
    return fail("相同 capability 类型没有获得统一的 runtime key");
  }
  if (plugin_a_register_consistent() != static_cast<int>(caors::errc::success)) {
    return fail("plugin A 无法注册一致静态类型模型");
  }
  if (plugin_b_register_consistent() != static_cast<int>(caors::errc::duplicate_id)) {
    return fail("相同静态类型模型被错误诊断为冲突");
  }

  return 0;
}
