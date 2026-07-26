// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_PLUGIN_MANAGER_H_
#define CAORS_PLUGINS_PLUGIN_MANAGER_H_

#include "caors/core/guid.h"
#include "caors/export.h"
#include "caors/plugins/plugin_binary.h"
#include "caors/plugins/plugin_host.h"
#include "caors/result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace caors::meta {

class registry;

} // namespace caors::meta

namespace caors::plugins {

namespace detail {

class managed_plugin;

} // namespace detail

/** @brief 由 plugin_manager 管理的稳定只读插件句柄。 */
class CAORS_API plugin final {
public:
  plugin() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] guid id() const noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept;
  [[nodiscard]] plugin_state state() const noexcept;
  [[nodiscard]] const plugin_descriptor& descriptor() const noexcept;
  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  /** @brief 查询插件提供的版本化扩展；返回地址不拥有插件或动态库。 */
  [[nodiscard]] const void* get_extension(std::string_view id,
                                          std::uint32_t version) const noexcept;

private:
  friend class plugin_manager;
  explicit plugin(std::shared_ptr<detail::managed_plugin> data) noexcept : data_(std::move(data)) {}

  std::shared_ptr<detail::managed_plugin> data_{};
};

/**
 * @brief 显式拥有插件宿主、插件二进制和注册生命周期的管理器。
 *
 * 生命周期操作按插件串行化；调用插件代码时不持有 manager 的索引锁。
 */
class CAORS_API plugin_manager final {
public:
  explicit plugin_manager(meta::registry& registry);
  plugin_manager(const plugin_manager&) = delete;
  plugin_manager& operator=(const plugin_manager&) = delete;
  ~plugin_manager();

  [[nodiscard]] plugin_host& host() noexcept;
  [[nodiscard]] const plugin_host& host() const noexcept;

  [[nodiscard]] result<plugin> load(const std::filesystem::path& path);
  [[nodiscard]] plugin find(guid id) const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] error initialize(guid id);
  [[nodiscard]] error start(guid id);
  [[nodiscard]] error stop(guid id);
  /**
   * @brief 停止、关闭并分离插件；仍有 layer lease 或调用在途时返回 pending。
   */
  [[nodiscard]] error unload(guid id);

private:
  struct impl;
  impl* data_ = nullptr;
};

} // namespace caors::plugins

#endif
