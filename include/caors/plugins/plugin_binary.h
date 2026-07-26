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

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] plugin_state state() const noexcept;
  [[nodiscard]] const plugin_descriptor& descriptor() const noexcept;
  [[nodiscard]] const std::filesystem::path& path() const noexcept;
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
