// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_host.h"
#include "caors/version.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>

namespace {

constexpr std::string_view service_id_text = "9b44f937-48f5-42f2-acf7-666f24194cc2";

void CAORS_PLUGIN_CALL capture_log(void* context, std::uint32_t, const char* message,
                                   std::size_t size) {
  static_cast<std::string*>(context)->assign(message, size);
}

} // namespace

TEST_CASE("插件宿主提供运行时扩展并支持精确版本降级") {
  caors::plugins::plugin_host host{};
  const caors_plugin_host& abi = host.abi();
  REQUIRE(abi.get_extension != nullptr);

  const auto* runtime = static_cast<const caors_plugin_runtime_extension*>(abi.get_extension(
      abi.context, CAORS_PLUGIN_RUNTIME_EXTENSION_ID,
      std::strlen(CAORS_PLUGIN_RUNTIME_EXTENSION_ID), CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION));
  REQUIRE(runtime != nullptr);
  CHECK(runtime->runtime_major == static_cast<std::uint32_t>(caors::k_version.major));
  CHECK(abi.get_extension(abi.context, CAORS_PLUGIN_RUNTIME_EXTENSION_ID,
                          std::strlen(CAORS_PLUGIN_RUNTIME_EXTENSION_ID),
                          CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION + 1) == nullptr);
  CHECK(abi.get_extension(abi.context, "caors.unknown", 13, 1) == nullptr);
}

TEST_CASE("插件宿主转发日志并拥有自定义扩展记录") {
  caors::plugins::plugin_host host{};
  std::string message{};
  host.set_log(&capture_log, &message);
  host.abi().log(host.abi().context, 1, "hello", 5);
  CHECK(message == "hello");

  constexpr int extension = 42;
  CHECK_FALSE(host.add_extension("caors.test.extension", 1, &extension));
  CHECK(host.find_extension("caors.test.extension", 1) == &extension);
  CHECK(host.find_extension("caors.test.extension", 2) == nullptr);
  CHECK(host.add_extension("caors.test.extension", 1, &extension).value() ==
        caors::errc::already_exists);
}

TEST_CASE("插件服务按稳定 GUID 选择最高兼容版本并按 owner 移除") {
  caors::plugins::plugin_host host{};
  const caors::guid id = *caors::guid::parse(service_id_text);
  int owner = 0;
  constexpr int version_one = 1;
  constexpr int version_three = 3;

  CHECK_FALSE(host.publish_service(id, 1, &version_one, &owner));
  CHECK_FALSE(host.publish_service(id, 3, &version_three, &owner));
  std::uint32_t selected = 0;
  CHECK(host.find_service(id, 1, 3, &selected) == &version_three);
  CHECK(selected == 3);
  CHECK(host.find_service(id, 1, 2, &selected) == &version_one);
  CHECK(selected == 1);
  CHECK(host.find_service(id, 4, 5, &selected) == nullptr);
  CHECK(selected == 0);
  CHECK(host.remove_services(&owner) == 2);
  CHECK(host.find_service(id, 1, 3) == nullptr);
}

TEST_CASE("插件可以通过纯 C Host Extension 发布和查询服务") {
  caors::plugins::plugin_host host{};
  const caors_plugin_host& abi = host.abi();
  const auto* services = static_cast<const caors_plugin_services_extension*>(abi.get_extension(
      abi.context, CAORS_PLUGIN_SERVICES_EXTENSION_ID,
      std::strlen(CAORS_PLUGIN_SERVICES_EXTENSION_ID), CAORS_PLUGIN_SERVICES_EXTENSION_VERSION));
  REQUIRE(services != nullptr);
  REQUIRE(services->struct_size >= sizeof(caors_plugin_services_extension));

  const caors::guid id = *caors::guid::parse(service_id_text);
  caors_plugin_guid abi_id{};
  std::copy(id.bytes().begin(), id.bytes().end(), std::begin(abi_id.bytes));
  int owner = 0;
  constexpr int service = 7;
  CHECK(services->publish(services->context, &abi_id, 1, &service, &owner) == 0);

  std::uint32_t selected = 0;
  CHECK(services->find(services->context, &abi_id, 1, 1, &selected) == &service);
  CHECK(selected == 1);
  CHECK(services->remove_owner(services->context, &owner) == 1);
}
