// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_PLUGIN_HOST_H_
#define CAORS_PLUGINS_PLUGIN_HOST_H_

#include "caors/core/guid.h"
#include "caors/export.h"
#include "caors/plugins/abi.h"
#include "caors/result.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace caors::plugins {

/**
 * @brief 插件宿主 ABI、扩展和借用型服务的所有者。
 *
 * `plugin_host` 必须比使用其 `abi()` 的所有 `plugin_binary` 活得更久。该类不是进程级全局对象；
 * manager 后续以显式成员持有它。扩展和服务的发布、移除与查询可并发调用。
 */
class CAORS_API plugin_host final {
public:
  plugin_host();
  plugin_host(const plugin_host&) = delete;
  plugin_host& operator=(const plugin_host&) = delete;
  plugin_host(plugin_host&&) = delete;
  plugin_host& operator=(plugin_host&&) = delete;
  ~plugin_host();

  /** @brief 返回生命周期由当前对象拥有的 C ABI 表。 */
  [[nodiscard]] const caors_plugin_host& abi() const noexcept;

  /** @brief 设置日志回调；context 的生命周期由调用者管理。 */
  void set_log(caors_plugin_log_fn callback, void* context = nullptr) noexcept;

  /** @brief 注册精确版本的自定义 Host Extension，地址由调用者拥有。 */
  [[nodiscard]] error add_extension(std::string_view id, std::uint32_t version,
                                    const void* extension);
  /** @brief 查询精确版本的 Host Extension。 */
  [[nodiscard]] const void* find_extension(std::string_view id,
                                           std::uint32_t version) const noexcept;

  /** @brief 发布借用型服务；服务和 owner 必须在移除记录前保持有效。 */
  [[nodiscard]] error publish_service(guid id, std::uint32_t version, const void* service,
                                      void* owner);
  /** @brief 在闭区间内选择最高可用服务版本。 */
  [[nodiscard]] const void* find_service(guid id, std::uint32_t minimum_version,
                                         std::uint32_t maximum_version,
                                         std::uint32_t* selected_version = nullptr) const noexcept;
  /** @brief 移除指定 owner 发布的全部服务，并返回移除数量。 */
  std::size_t remove_services(void* owner) noexcept;

private:
  struct impl;
  impl* data_ = nullptr;
};

} // namespace caors::plugins

#endif
