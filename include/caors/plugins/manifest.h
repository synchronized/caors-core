// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_MANIFEST_H_
#define CAORS_PLUGINS_MANIFEST_H_

#include "caors/core/guid.h"
#include "caors/export.h"
#include "caors/plugins/semantic_version.h"
#include "caors/result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace caors::plugins {

/** @brief Manifest 中声明的插件依赖。 */
struct plugin_dependency {
  guid id{};
  version_range version{};
  bool optional = false;
};

/** @brief 无需加载动态库即可读取的插件描述。 */
struct plugin_manifest {
  guid id{};
  std::string name{};
  std::string display_name{};
  semantic_version version{};
  std::uint32_t abi_version = 0;
  std::filesystem::path library{};
  std::vector<plugin_dependency> dependencies{};
  std::vector<std::string> capabilities{};

  [[nodiscard]] bool valid() const noexcept {
    return id && !name.empty() && abi_version != 0 && !library.empty();
  }
};

/** @brief 解析 UTF-8 JSON manifest；该过程不依赖 meta 注册。 */
[[nodiscard]] CAORS_API result<plugin_manifest> parse_plugin_manifest(std::string_view text);

/** @brief 从显式路径读取并解析 manifest。 */
[[nodiscard]] CAORS_API result<plugin_manifest>
load_plugin_manifest(const std::filesystem::path& path);

/** @brief 依赖解析产生的正序加载和逆序停止下标。 */
struct plugin_dependency_order {
  std::vector<std::size_t> load{};
  std::vector<std::size_t> unload{};
};

/**
 * @brief 校验插件 ID、依赖和版本，并生成确定的拓扑顺序。
 *
 * 返回下标引用输入 span；调用者必须自行维持 manifest 集合的生命周期。
 */
[[nodiscard]] CAORS_API result<plugin_dependency_order>
resolve_plugin_dependencies(std::span<const plugin_manifest> manifests);

} // namespace caors::plugins

#endif
