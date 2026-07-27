// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_PLUGIN_MANAGER_H_
#define CAORS_PLUGINS_PLUGIN_MANAGER_H_

#include "caors/core/guid.h"
#include "caors/export.h"
#include "caors/plugins/plugin_binary.h"
#include "caors/plugins/plugin_host.h"
#include "caors/plugins/registration_scope.h"
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

/** @brief plugin_manager 管理插件动态库和 registry layer 的生命周期策略。 */
enum class plugin_lifetime_policy {
  /** @brief 默认可卸载策略：插件注册进入 detachable registry layer，unload 会 detach 并等待 lease
     释放。 */
  detachable,
  /** @brief 固定到 manager 或进程生命周期；显式 unload 返回 unsupported，不提前关闭动态库。 */
  pinned_process,
};

/** @brief 加载插件时使用的管理选项。 */
struct plugin_load_options {
  plugin_lifetime_policy lifetime = plugin_lifetime_policy::detachable;
};

/** @brief plugin_manager 最近一次卸载尝试的结构化诊断。 */
struct plugin_unload_diagnostic {
  guid id{};
  std::uint64_t generation = 0;
  plugin_state state = plugin_state::unloaded;
  plugin_lifetime_policy lifetime = plugin_lifetime_policy::detachable;
  plugin_unload_blocker blockers = plugin_unload_blocker::none;
  std::size_t layer_lease_count = 0;
  std::size_t in_flight_calls = 0;
  std::size_t local_type_count = 0;
  bool layer_published = false;

  [[nodiscard]] bool blocked() const noexcept {
    return blockers != plugin_unload_blocker::none || in_flight_calls != 0 || layer_lease_count > 0;
  }
};

/** @brief 由 plugin_manager 管理的稳定只读插件句柄。 */
class CAORS_API plugin final {
public:
  plugin() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] guid id() const noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept;
  [[nodiscard]] plugin_state state() const noexcept;
  [[nodiscard]] plugin_lifetime_policy lifetime() const noexcept;
  [[nodiscard]] plugin_unload_diagnostic unload_diagnostic() const noexcept;
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

  [[nodiscard]] result<plugin> load(const std::filesystem::path& path,
                                    plugin_load_options options = {});
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
