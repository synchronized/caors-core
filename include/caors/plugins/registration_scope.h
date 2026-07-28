// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_REGISTRATION_SCOPE_H_
#define CAORS_PLUGINS_REGISTRATION_SCOPE_H_

#include "caors/events/connection.h"
#include "caors/export.h"
#include "caors/plugins/plugin_host.h"
#include "caors/tasks/request.h"

#include <cstddef>
#include <cstdint>

namespace caors::plugins {

/** @brief 当前阶段能够识别的插件安全卸载阻止原因。 */
enum class plugin_unload_blocker : std::uint32_t {
  none = 0,
  pending_task = 1u << 1u,
  plugin_owned_value = 1u << 2u,
  pinned_process = 1u << 3u,
};

[[nodiscard]] constexpr plugin_unload_blocker operator|(plugin_unload_blocker lhs,
                                                        plugin_unload_blocker rhs) noexcept {
  return static_cast<plugin_unload_blocker>(static_cast<std::uint32_t>(lhs) |
                                            static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr bool has_blocker(plugin_unload_blocker value,
                                         plugin_unload_blocker blocker) noexcept {
  return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(blocker)) != 0;
}

/** @brief registration scope 清理结果和结构化卸载诊断。 */
struct plugin_cleanup_result {
  error status{};
  plugin_unload_blocker blockers = plugin_unload_blocker::none;
  std::size_t removed_services = 0;
  std::size_t disconnected_events = 0;
  std::size_t cancelled_requests = 0;

  [[nodiscard]] bool can_unload() const noexcept { return !status; }
};

/**
 * @brief 追踪单个插件注册和运行期资源的显式作用域。
 *
 * owner 是插件在服务 ABI 中使用的稳定实例地址。scope 必须早于 `plugin_host` 销毁。析构会执行
 * 幂等清理，但调用者应显式调用 `cleanup()` 读取卸载阻止原因。
 */
class CAORS_API plugin_registration_scope final {
public:
  plugin_registration_scope(plugin_host& host, void* owner);
  plugin_registration_scope(const plugin_registration_scope&) = delete;
  plugin_registration_scope& operator=(const plugin_registration_scope&) = delete;
  plugin_registration_scope(plugin_registration_scope&&) = delete;
  plugin_registration_scope& operator=(plugin_registration_scope&&) = delete;
  ~plugin_registration_scope();

  /** @brief 通过当前 owner 发布借用型服务。 */
  [[nodiscard]] error publish_service(guid id, std::uint32_t version, const void* service);
  /** @brief 接管事件连接，并在清理时断开。 */
  [[nodiscard]] error track(events::connection connection);
  /** @brief 追踪请求，并在清理时请求取消。 */
  [[nodiscard]] error track(tasks::request request);

  /** @brief 记录一个仍需插件代码析构的擦除值。 */
  void record_plugin_owned_value(std::size_t count = 1) noexcept;
  /** @brief 通知 scope 已释放对应数量的插件擦除值。 */
  void release_plugin_owned_value(std::size_t count = 1) noexcept;

  /** @brief 执行幂等清理并返回结构化卸载诊断。 */
  [[nodiscard]] plugin_cleanup_result cleanup() noexcept;
  [[nodiscard]] const plugin_cleanup_result& result() const noexcept;

private:
  struct impl;
  impl* data_ = nullptr;
};

} // namespace caors::plugins

#endif
