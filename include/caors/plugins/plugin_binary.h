// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_PLUGIN_BINARY_H_
#define CAORS_PLUGINS_PLUGIN_BINARY_H_

#include "caors/core/guid.h"
#include "caors/export.h"
#include "caors/plugins/abi.h"
#include "caors/plugins/plugin_library_lease.h"
#include "caors/result.h"
#include "caors/version.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace caors::plugins {

/** @brief 单个插件二进制的生命周期状态。 */
enum class plugin_state {
  unloaded,
  loaded,
  initialized,
  running,
  stopped,
  detached_pending_release,
  failed,
};

/** @brief host 与 plugin 构建环境不兼容字段。 */
enum class plugin_build_mismatch : std::uint32_t {
  none = 0,
  missing_build_extension = 1U << 0U,
  compiler_family = 1U << 1U,
  compiler_version = 1U << 2U,
  compiler_abi_version = 1U << 3U,
  crt_mode = 1U << 4U,
  iterator_debug_level = 1U << 5U,
  pointer_size = 1U << 6U,
  architecture = 1U << 7U,
};

[[nodiscard]] constexpr plugin_build_mismatch operator|(plugin_build_mismatch lhs,
                                                        plugin_build_mismatch rhs) noexcept {
  return static_cast<plugin_build_mismatch>(static_cast<std::uint32_t>(lhs) |
                                            static_cast<std::uint32_t>(rhs));
}

constexpr plugin_build_mismatch& operator|=(plugin_build_mismatch& lhs,
                                            plugin_build_mismatch rhs) noexcept {
  lhs = lhs | rhs;
  return lhs;
}

/** @brief 判断诊断是否包含指定不兼容字段。 */
[[nodiscard]] constexpr bool has_mismatch(plugin_build_mismatch value,
                                          plugin_build_mismatch field) noexcept {
  return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(field)) != 0;
}

/** @brief 插件构建兼容性诊断和双方环境快照。 */
struct plugin_build_compatibility_diagnostic {
  plugin_build_mismatch mismatches = plugin_build_mismatch::none;
  caors_plugin_build_environment host{};
  caors_plugin_build_environment plugin{};

  [[nodiscard]] constexpr bool compatible() const noexcept {
    return mismatches == plugin_build_mismatch::none;
  }
};

/** @brief 由宿主拥有的插件描述信息。 */
struct plugin_descriptor {
  guid id{};
  std::string name{};
  std::string display_name{};
  version plugin_version{};

  [[nodiscard]] bool valid() const noexcept { return id && !name.empty(); }
};

/**
 * @brief 已完成 ABI 校验的插件二进制。
 *
 * 该对象拥有底层动态库。当前阶段不提供显式卸载；对象销毁只适合进程关闭或确认插件未向外部
 * runtime 发布代码指针的测试场景。
 */
class CAORS_API plugin_binary final {
public:
  plugin_binary() noexcept = default;
  plugin_binary(const plugin_binary&) = delete;
  plugin_binary& operator=(const plugin_binary&) = delete;
  plugin_binary(plugin_binary&& other) noexcept;
  plugin_binary& operator=(plugin_binary&& other) noexcept;
  ~plugin_binary();

  /** @brief 加载插件并校验入口、ABI 和描述信息。 */
  [[nodiscard]] static result<plugin_binary> load(const std::filesystem::path& path,
                                                  const caors_plugin_host* host = nullptr);
  /** @brief 加载插件并通过可选输出参数返回构建环境兼容性诊断。 */
  [[nodiscard]] static result<plugin_binary>
  load(const std::filesystem::path& path, const caors_plugin_host* host,
       plugin_build_compatibility_diagnostic* diagnostic);

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] plugin_state state() const noexcept;
  [[nodiscard]] const plugin_descriptor& descriptor() const noexcept;
  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  /** @brief 返回插件入口声明的实例标识，仅用于宿主生命周期绑定。 */
  [[nodiscard]] void* owner_token() const noexcept;
  /** @brief 获取一份保持当前插件动态库已加载的共享租约。 */
  [[nodiscard]] plugin_library_lease library_lease() const noexcept;

  [[nodiscard]] error initialize() noexcept;
  [[nodiscard]] error start() noexcept;
  [[nodiscard]] error stop() noexcept;
  [[nodiscard]] error shutdown() noexcept;
  [[nodiscard]] const void* get_extension(std::string_view id,
                                          std::uint32_t version) const noexcept;

private:
  struct impl;
  explicit plugin_binary(impl* data) noexcept : data_(data) {}

  impl* data_ = nullptr;
};

} // namespace caors::plugins

#endif
