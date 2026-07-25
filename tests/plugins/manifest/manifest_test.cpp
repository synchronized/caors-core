// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/manifest.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

caors::plugins::plugin_manifest make_manifest(std::string_view id, std::string name,
                                              caors::plugins::semantic_version version) {
  caors::plugins::plugin_manifest value{};
  value.id = *caors::guid::parse(id);
  value.name = std::move(name);
  value.version = version;
  value.abi_version = 1;
  value.library = "plugin.dll";
  return value;
}

} // namespace

TEST_CASE("插件语义版本支持精确值和比较范围") {
  using caors::plugins::semantic_version;
  using caors::plugins::version_range;

  const auto version = semantic_version::parse("1.2.3");
  REQUIRE(version);
  CHECK(version->string() == "1.2.3");

  const auto range = version_range::parse(">=1.0.0 <2.0.0");
  REQUIRE(range);
  CHECK(range->contains(*version));
  CHECK_FALSE(range->contains({2, 0, 0}));
  CHECK_FALSE(version_range::parse("1.2"));
  CHECK_FALSE(version_range::parse(">2.0.0 <1.0.0"));
}

TEST_CASE("插件 manifest 无需 meta 注册即可解析") {
  const auto manifest = caors::plugins::parse_plugin_manifest(R"({
    "id": "2a6d5992-ff3c-45ae-a167-d861e469aa90",
    "name": "caors.image.png",
    "display_name": "PNG",
    "version": "1.2.3",
    "abi": 1,
    "library": "caors_image_png.dll",
    "capabilities": ["image.decoder"],
    "dependencies": [{
      "id": "1cc73a41-dabb-4545-88e5-28a58dc3f821",
      "version": ">=1.0.0 <2.0.0",
      "optional": true
    }]
  })");

  REQUIRE(manifest);
  CHECK(manifest->name == "caors.image.png");
  CHECK(manifest->display_name == "PNG");
  CHECK(manifest->dependencies.size() == 1);
  CHECK(manifest->dependencies[0].optional);
  CHECK(manifest->capabilities == std::vector<std::string>{"image.decoder"});
}

TEST_CASE("插件依赖按拓扑正序加载并按逆序停止") {
  using namespace caors::plugins;
  auto base = make_manifest("00000000-0000-0000-0000-000000000001", "base", {1, 1, 0});
  auto middle = make_manifest("00000000-0000-0000-0000-000000000002", "middle", {1, 0, 0});
  auto leaf = make_manifest("00000000-0000-0000-0000-000000000003", "leaf", {1, 0, 0});
  middle.dependencies.push_back({base.id, *version_range::parse(">=1.0.0 <2.0.0"), false});
  leaf.dependencies.push_back({middle.id, {}, false});

  const std::array manifests{leaf, middle, base};
  const auto order = resolve_plugin_dependencies(manifests);
  REQUIRE(order);
  CHECK(order->load == std::vector<std::size_t>{2, 1, 0});
  CHECK(order->unload == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("插件依赖报告缺失、版本冲突和循环") {
  using namespace caors::plugins;
  auto first = make_manifest("00000000-0000-0000-0000-000000000011", "first", {1, 0, 0});
  auto second = make_manifest("00000000-0000-0000-0000-000000000012", "second", {2, 0, 0});

  first.dependencies.push_back(
      {*caors::guid::parse("00000000-0000-0000-0000-000000000099"), {}, true});
  REQUIRE(resolve_plugin_dependencies(std::array{first}));

  first.dependencies.back().optional = false;
  auto order = resolve_plugin_dependencies(std::array{first});
  REQUIRE_FALSE(order);
  CHECK(order.error() == caors::error{caors::error_domain::plugins, caors::errc::not_found});

  first.dependencies.back() = {second.id, *version_range::parse("<2.0.0"), false};
  order = resolve_plugin_dependencies(std::array{first, second});
  REQUIRE_FALSE(order);
  CHECK(order.error() ==
        caors::error{caors::error_domain::plugins, caors::errc::inconsistent_type});

  first.dependencies.back() = {second.id, {}, false};
  second.dependencies.push_back({first.id, {}, false});
  order = resolve_plugin_dependencies(std::array{first, second});
  REQUIRE_FALSE(order);
  CHECK(order.error() == caors::error{caors::error_domain::plugins, caors::errc::cycle_detected});
}
