// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_PLUGIN_LIBRARY_LEASE_H_
#define CAORS_PLUGINS_PLUGIN_LIBRARY_LEASE_H_

#include "caors/export.h"
#include "caors/error.h"

#include <cstddef>
#include <filesystem>
#include <memory>

namespace caors::plugins {

class plugin_binary;

} // namespace caors::plugins

namespace caors::meta {

class registry_layer;

} // namespace caors::meta

namespace caors::plugins {

/**
 * @brief 保持插件动态库处于加载状态的共享租约。
 *
 * 租约不允许直接访问平台句柄。只要任意租约仍存在，底层动态库就不会执行
 * `FreeLibrary` 或 `dlclose`。
 */
class CAORS_API plugin_library_lease final {
public:
  plugin_library_lease() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  [[nodiscard]] std::size_t use_count() const noexcept;
  /**
   * @brief 让 registry layer 共同持有当前动态库。
   *
   * 只能在 layer 发布前绑定；绑定成功后，由该 layer 产生的视图和值会通过 layer
   * 状态间接保持动态库已加载。
   */
  [[nodiscard]] error bind(meta::registry_layer& layer) const;

private:
  friend class plugin_binary;

  struct impl;
  explicit plugin_library_lease(std::shared_ptr<impl> data) noexcept : data_(std::move(data)) {}

  std::shared_ptr<impl> data_{};
};

} // namespace caors::plugins

#endif
